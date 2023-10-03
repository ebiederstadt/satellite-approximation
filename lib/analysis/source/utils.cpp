#include "analysis/utils.h"

#include <fmt/format.h>
#include <magic_enum.hpp>

namespace analysis {
std::optional<Indices> from_str(std::string_view str)
{
    return magic_enum::enum_cast<Indices>(str);
}

std::vector<std::string> required_files(Indices index)
{
    switch (index) {
    case Indices::NDVI:
        return { "B08", "B04" };
    case Indices::NDMI:
        return { "B08", "B11" };
    case Indices::mNDWI:
        return { "B03", "B11" };
    case Indices::SWI:
        return { "B03", "B08", "B11" };
    default:
        throw std::runtime_error(fmt::format("Failed to handle case for {}", magic_enum::enum_name(index)));
    }
}

bool missing_files(std::vector<std::string> const& files, Indices index)
{
    auto necessary_files = required_files(index);
    bool files_missing = false;
    for (auto const& file : necessary_files) {
        files_missing |= !contains(files, file);
    }
    return files_missing;
}

utils::GeoTIFF<f64> compute_index(fs::path const& folder, fs::path const& template_path, Indices index)
{
    auto tiff_name = fmt::format("{}.tif", magic_enum::enum_name(index));
    fs::path tiff_path = folder / fs::path(tiff_name);
    if (fs::exists(tiff_path)) {
        return { tiff_path };
    }

    // Compute the result of (a - b) / (a + b), where 0 / 0 = 0
    // Also stores the result to the disk
    auto normalized_computation = [&](MatX<f64> const& a, MatX<f64> const& b) {
        utils::GeoTIFF<f64> result(template_path);
        MatX<f64> computation = (a.array() - b.array()) / (a.array() + b.array());
        computation = computation.unaryExpr([](f64 v) { return std::isfinite(v) ? v : 0.0; });
        result.values = computation;
        result.write(tiff_path, template_path);
        return result;
    };

    auto compute_swi = [&] {
        auto green = utils::GeoTIFF<f64> { folder / "B03.tif" };
        auto swir = utils::GeoTIFF<f64> { folder / "B11.tif" };
        auto nir = utils::GeoTIFF<f64> { folder / "B08.tif" };

        auto result = utils::GeoTIFF<f64>(template_path);
        auto denominator = (green.values.array() + nir.values.array()) * (nir.values.array() + swir.values.array());
        result.values = (green.values.array() * (nir.values.array() - swir.values.array()) / denominator);
        result.values = result.values.unaryExpr([](f64 v) { return std::isfinite(v) ? v : 0.0; });
        result.write(tiff_path, template_path);
        return result;
    };

    switch (index) {
    case Indices::NDVI: {
        auto nir = utils::GeoTIFF<f64> { folder / "B08.tif" };
        auto red = utils::GeoTIFF<f64> { folder / "B04.tif" };
        return normalized_computation(nir.values, red.values);
    }
    case Indices::NDMI: {
        auto nir = utils::GeoTIFF<f64> { folder / "B08.tif" };
        auto swir = utils::GeoTIFF<f64> { folder / "B11.tif" };
        return normalized_computation(nir.values, swir.values);
    }
    case Indices::mNDWI: {
        auto green = utils::GeoTIFF<f64> { folder / "B03.tif" };
        auto swir = utils::GeoTIFF<f64> { folder / "B11.tif" };
        return normalized_computation(green.values, swir.values);
    }
    case Indices::SWI:
        return compute_swi();
    default:
        throw std::runtime_error(fmt::format("Failed to map the index: {}", magic_enum::enum_name(index)));
    }
}

// https://stackoverflow.com/questions/69453972/get-all-non-zero-values-of-a-dense-eigenmatrix-object
VecX<f64> selectMatrixElements(MatX<f64> const& matrix, f64 removalValue)
{
    auto size = matrix.size();
    // create 1D view
    auto view = matrix.reshaped().transpose();
    // create boolean markers for nonzeros
    auto mask = view.array() != removalValue;
    // create index list and set useless elements to sentinel value
    int constexpr sentinel = std::numeric_limits<int>::lowest();
    auto idxs = mask.select(Eigen::RowVectorXi::LinSpaced(size, 0, (int)size), sentinel).eval();
    // sort to remove sentinel values
    std::partial_sort(idxs.begin(), idxs.begin() + size, idxs.end(), std::greater {});
    idxs.conservativeResize(mask.count());
    VecX<f64> newVector = view(idxs.reverse()).eval();
    return newVector;
}
}
