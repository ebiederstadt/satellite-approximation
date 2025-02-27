#pragma once

#include "utils/error.h"
#include "utils/log.h"
#include "utils/noCopying.h"

#include <filesystem>
#include <fmt/std.h>
#include <gdal_priv.h>
#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <type_traits>
#include <utils/types.h>
#include <variant>

namespace fs = std::filesystem;

namespace utils {
class GDALDatasetWrapper {
    MAKE_NONCOPYABLE(GDALDatasetWrapper);

public:
    explicit GDALDatasetWrapper(std::string path);
    virtual ~GDALDatasetWrapper();

    GDALDataset* operator->() const { return dataset; }

private:
    GDALDataset* dataset;
};

u64 const EPSG_WGS84 = 4326;

// Helper to select the appropriate GDALDataType value from our
// internal type
template<typename ScalarT>
struct GDALTypeForOurType { };

template<>
struct GDALTypeForOurType<u8> {
    GDALDataType type = GDT_Byte;
};
template<>
struct GDALTypeForOurType<u16> {
    GDALDataType type = GDT_UInt16;
};
template<>
struct GDALTypeForOurType<u32> {
    GDALDataType type = GDT_UInt32;
};
template<>
struct GDALTypeForOurType<i16> {
    GDALDataType type = GDT_Int16;
};
template<>
struct GDALTypeForOurType<i32> {
    GDALDataType type = GDT_Int32;
};
template<>
struct GDALTypeForOurType<f32> {
    GDALDataType type = GDT_Float32;
};
template<>
struct GDALTypeForOurType<f64> {
    GDALDataType type = GDT_Float64;
};

template<typename T, typename = int>
struct HasType : std::false_type { };

template<typename T>
struct HasType<T, decltype((void)T::type, 0)> : std::true_type { };

template<typename T>
struct Domain {
    T start;
    T end;
};

template<typename T>
using MultiBandValues = std::shared_ptr<std::vector<MatX<T>>>;

template<typename T>
using SingleBandValues = std::shared_ptr<MatX<T>>;

template<typename T>
using TiffValues = std::variant<MultiBandValues<T>, SingleBandValues<T>>;

template<class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template<class... Ts>
overload(Ts...) -> overload<Ts...>;

template<typename ScalarT>
class GeoTiffWriter {
public:
    GeoTiffWriter(std::shared_ptr<std::vector<MatX<ScalarT>>> _values, fs::path const& template_path)
        : values(std::move(_values))
    {
        poSrcDS = static_cast<GDALDataset*>(GDALOpen(template_path.c_str(), GA_ReadOnly));

        width = poSrcDS->GetRasterXSize();
        height = poSrcDS->GetRasterYSize();
    }

    GeoTiffWriter(std::shared_ptr<MatX<ScalarT>> _values, fs::path const& template_path)
        : values(std::move(_values))
    {
        poSrcDS = static_cast<GDALDataset*>(GDALOpen(template_path.c_str(), GA_ReadOnly));

        width = poSrcDS->GetRasterXSize();
        height = poSrcDS->GetRasterYSize();
    }

    ~GeoTiffWriter()
    {
        poDstDS->FlushCache();

        GDALClose(poSrcDS);
        GDALClose(poDstDS);
    }

    void write(fs::path const& destination, int start_index = 1)
    {
        if (!ready_driver()) {
            return;
        }

        poDstDS = static_cast<GDALDataset*>(poDriver->CreateCopy(
            destination.c_str(), poSrcDS, true, nullptr, nullptr, nullptr));

        GDALTypeForOurType<ScalarT> type;

        auto write_band = [&](int band_index, MatX<ScalarT> const& band_data) {
            GDALRasterBand* band = poDstDS->GetRasterBand(band_index);
            auto err = band->RasterIO(
                GF_Write,
                0, 0,
                this->width, this->height,
                (void*)band_data.data(),
                this->width, this->height,
                type.type,
                0, 0, 0);

            if (err != CE_None) {
                spdlog::error("Could not write to file. Error code: {}", CPLGetLastErrorMsg());
                throw std::runtime_error("Unable to write raster image");
            }
        };

        std::visit(overload {
                       [&](SingleBandValues<ScalarT>& band_data) {
                           write_band(1, *band_data);
                           spdlog::debug("Wrote to {}", destination);
                       },
                       [&](MultiBandValues<ScalarT>& band_values) {
                           int band_index = start_index;
                           for (auto const& band_data : *band_values) {
                               write_band(band_index, band_data);
                               band_index += 1;
                           }
                           spdlog::debug("Wrote {} bands to {}", band_values->size(), destination);
                       } },
            values);
    }

private:
    bool ready_driver()
    {
        if (poDriver != nullptr)
            return true;

        char const* pszFormat = "GTiff";
        poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
        if (poDriver == nullptr) {
            throw std::runtime_error("Unable to find driver for " + std::string(pszFormat));
        }
        char** papszMetadata = poDriver->GetMetadata();
        if (CSLFetchBoolean(papszMetadata, GDAL_DCAP_CREATECOPY, FALSE) == 0) {
            spdlog::error("Driver {} does not support CreateCopy() method and cannot be used", pszFormat);
            return false;
        }
        return true;
    }

    TiffValues<ScalarT> values;
    GDALDriver* poDriver = nullptr;
    GDALDataset* poSrcDS = nullptr;
    int width = 0;
    int height = 0;
    GDALDataset* poDstDS = nullptr;
};

//------------------------------------------------------------------------------
// Useful references:
// https://gdal.org/tutorials/geotransforms_tut.html#geotransforms-tut
// https://gdal.org/tutorials/raster_api_tut.html
//------------------------------------------------------------------------------

template<typename ScalarT>
class GeoTIFF {
public:
    static_assert(
        HasType<GDALTypeForOurType<ScalarT>>::value,
        "We only support a limited set of GDAL data types at the moment."
        " These include u8, u16, u32, i16, i32, f32, f64");

    GeoTIFF(std::string path)
        : m_path(std::move(path))
        , m_dataSet(m_path)
    {
        dataSetCRS = OGRSpatialReference { m_dataSet->GetProjectionRef() };
        dataSetCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        width = m_dataSet->GetRasterXSize();
        height = m_dataSet->GetRasterYSize();

        if (m_dataSet->GetGeoTransform(geoTransform) != CE_None) {
            throw IOError("Unable to load the geo transformation information", fs::path(path));
        }
    }

    // Default constructor for the GeoTIFF.
    GeoTIFF()
        : width(0)
        , height(0)
        , geoTransform {}
    {
    }

    MatX<ScalarT> read(int band_num) const
    {
        GDALRasterBand* band = m_dataSet->GetRasterBand(band_num);
        MatX<ScalarT> values = MatX<ScalarT>::Zero(static_cast<Eigen::Index>(height), static_cast<Eigen::Index>(width));

        GDALTypeForOurType<ScalarT> type;
        auto err = band->RasterIO(
            GF_Read,
            0, 0,
            width, height,
            values.data(),
            width, height,
            type.type,
            0, 0, 0);

        if (err != CE_None) {
            throw std::runtime_error("Unable to load raster image");
        }
        return values;
    }

    std::vector<MatX<ScalarT>> read(std::vector<int> const& bands) const
    {
        std::vector<MatX<ScalarT>> output;
        for (auto band_num : bands) {
            output.push_back(read(band_num));
        }

        return output;
    }

    std::vector<MatX<ScalarT>> read() const
    {
        std::vector<MatX<ScalarT>> output;
        for (int band_num = 0; band_num < m_dataSet->GetRasterCount(); ++band_num) {
            output.push_back(read(band_num));
        }

        return output;
    }

    void write(cv::Mat const& matrix, fs::path const& pathOfDestination, int bandIndex = 1) const
    {
        char const* pszFormat = "GTiff";
        GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
        if (poDriver == nullptr) {
            throw std::runtime_error("Unable to find driver for " + std::string(pszFormat));
        }
        char** papszMetadata = poDriver->GetMetadata();
        if (CSLFetchBoolean(papszMetadata, GDAL_DCAP_CREATECOPY, FALSE) == 0) {
            spdlog::error("Driver {} does not support CreateCopy() method and cannot be used", pszFormat);
            return;
        }

        // Use a unique_ptr to manage lifetime
        std::unique_ptr<GDALDataset, decltype(&GDALClose)> poSrcDS {
            static_cast<GDALDataset*>(GDALOpen(m_path.c_str(), GA_ReadOnly)),
            GDALClose
        };

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> poDstDS {
            poDriver->CreateCopy(pathOfDestination.c_str(), poSrcDS.get(), TRUE, nullptr, nullptr, nullptr),
            GDALClose
        };

        GDALRasterBand* band = poDstDS->GetRasterBand(bandIndex);
        spdlog::debug(
            "Writing to file. CRS.Name: {}, RasterCount: {}",
            dataSetCRS.GetName(),
            poDstDS->GetRasterCount());

        GDALTypeForOurType<ScalarT> type;
        auto err = band->RasterIO(
            GF_Write,
            0, 0,
            matrix.cols, matrix.rows,
            const_cast<void*>(static_cast<void const*>(matrix.data)),
            matrix.cols, matrix.rows,
            type.type,
            0, 0, 0);

        if (err != CE_None) {
            spdlog::error("Could not write to file. Error code: {}", CPLGetLastErrorMsg());
            throw std::runtime_error("Unable to write raster image");
        }
        poDstDS->FlushCache();
    }

    //------------------------------------------------------------------------------
    // https://gdal.org/tutorials/geotransforms_tut.html#geotransforms-tut
    // GT(0) x-coordinate of the upper-left corner of the upper-left pixel.
    // GT(1) w-e pixel resolution / pixel width.
    // GT(2) row rotation (typically zero).
    // GT(3) y-coordinate of the upper-left corner of the upper-left pixel.
    // GT(4) column rotation (typically zero).
    // GT(5) n-s pixel resolution / pixel height (negative value for a north-up image).
    //------------------------------------------------------------------------------
    f64 eastWestStep() const { return geoTransform[1]; }
    f64 northSouthStep() const { return geoTransform[5]; }

    // TODO: assumes north-up images
    f64 north() const { return geoTransform[3]; }
    f64 west() const { return geoTransform[0]; }
    f64 south() const { return geoTransform[3] + (height * northSouthStep()); }
    f64 east() const { return geoTransform[0] + (width * eastWestStep()); }

    LatLng northWest() const { return LatLng(north(), west()); }
    LatLng northEast() const { return LatLng(north(), east()); }
    LatLng southEast() const { return LatLng(south(), east()); }
    LatLng southWest() const { return LatLng(south(), west()); }

    ScalarT valueAt(LatLng const& pos, MatX<ScalarT> const& values) const
    {
        Vec2<i64> i = indexAt(pos);
        return values(static_cast<Eigen::Index>(i.y()), static_cast<Eigen::Index>(i.x()));
    }

    ScalarT bilinearValueAt(LatLng const& pos, MatX<ScalarT> const& values) const
    {
        // https://en.wikipedia.org/wiki/Bilinear_interpolation#Algorithm
        f64 x = (pos.y() - west()) / eastWestStep();
        f64 y = (pos.x() - north()) / northSouthStep();
        f64 x1 = std::floor(x);
        f64 x2 = std::ceil(x);
        f64 y1 = std::floor(y);
        f64 y2 = std::ceil(y);

        auto value = [&](f64 fx, f64 fy) {
            Eigen::Index x_i = std::clamp(static_cast<i64>(fx), static_cast<i64>(0), static_cast<i64>(width - 1));
            Eigen::Index y_i = std::clamp(static_cast<i64>(fy), static_cast<i64>(0), static_cast<i64>(height - 1));
            return values(y_i, x_i);
        };
        ScalarT f_Q11 = value(x1, y1);
        ScalarT f_Q12 = value(x1, y2);
        ScalarT f_Q21 = value(x2, y1);
        ScalarT f_Q22 = value(x2, y2);
        f64 s = 1.0 / ((x2 - x1) * (y2 - y1));
        Vec2<f64> v1 { x2 - x, x - x1 };
        Vec2<f64> v2 { y2 - y, y - y1 };
        Mat2<f64> M;
        M << f_Q11, f_Q12,
            f_Q21, f_Q22;
        return static_cast<ScalarT>(s * ((v1.transpose() * (M * v2))(0, 0)));
    }

    Vec2<f64> uvAt(LatLng const& pos)
    {
        Vec2<i64> i = indexAt(pos);

        f64 u = static_cast<f64>(i.x()) / width;
        f64 v = static_cast<f64>(i.y()) / height;

        return Vec2<f64>(u, v);
    }

    Vec2<i64> indexAt(LatLng const& pos) const
    {
        i64 x = i64((pos.y() - west()) / eastWestStep());
        i64 y = i64((pos.x() - north()) / northSouthStep());
        x = std::clamp(x, static_cast<i64>(0), static_cast<i64>(width - 1));
        y = std::clamp(y, static_cast<i64>(0), static_cast<i64>(height - 1));

        return Vec2<i64>(x, y);
    }

    LatLng midPointOfPixel(Vec2<i64> index) const
    {
        return LatLng(
            north() + (northSouthStep() * static_cast<f64>(index.x())) + (northSouthStep() * 0.5),
            west() + (eastWestStep() * static_cast<f64>(index.y())) + (eastWestStep() * 0.5));
    }

    template<typename OScalarT>
    Domain<OScalarT> valueDomain(MatX<ScalarT> const& values) const
    {
        return Domain<OScalarT> {
            static_cast<OScalarT>(values.minCoeff()),
            static_cast<OScalarT>(values.maxCoeff())
        };
    }

    // DEMs tend to use -32767.0 as the sentinel for "NO DATA"
    // This calculation will ignore those.
    template<typename OScalarT>
    Domain<OScalarT> demValueDomain(MatX<ScalarT> const& values)
    {
        auto maxValue = values.maxCoeff();
        MatX<ScalarT> selectionMatrix = (values.array() <= -32767.f).template cast<ScalarT>();
        MatX<ScalarT> tmpValues = values + (32767 * selectionMatrix) + (maxValue * selectionMatrix);
        return Domain<OScalarT> {
            static_cast<OScalarT>(tmpValues.minCoeff()),
            static_cast<OScalarT>(maxValue)
        };
    }

    int width;
    int height;

    f64 geoTransform[6];

private:
    std::string m_path;
    GDALDatasetWrapper m_dataSet;
    OGRSpatialReference dataSetCRS;
};

}
