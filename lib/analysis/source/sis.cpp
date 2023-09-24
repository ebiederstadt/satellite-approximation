#include "analysis/sis.h"
#include "analysis/db.h"
#include "utils/filesystem.h"
#include "utils/geotiff.h"
#include "utils/fmt_filesystem.h"

#include <vector>
#include <execution>
#include <magic_enum.hpp>
#include <fmt/format.h>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <sqlite3.h>

namespace date_time = boost::gregorian;

namespace analysis {
    std::filesystem::path cache_string(int start_year, int end_year, Indices index, f64 threshold) {
        auto index_name = magic_enum::enum_name(index);
        return fmt::format("SIS_{}_{}_{}_{:.1f}.tif", index_name, start_year, end_year, threshold);
    }

    utils::GeoTIFF<f64> compute_index(fs::path const &folder, Indices index) {
        auto tiff_name = fmt::format("{}.tif", magic_enum::enum_name(index));
        fs::path tiff_path = folder / fs::path(tiff_name);
        if (fs::exists(tiff_path)) {
            return {tiff_path};
        }

        // Compute the result of (a - b) / (a + b), where 0 / 0 = 0
        // Also stores the result to the disk
        auto normalized_computation = [&tiff_path](MatX<f64> const &a, MatX<f64> const &b) {
            fs::path tiff_template = tiff_path.parent_path() / "B04.tif";
            utils::GeoTIFF<f64> result(tiff_template);
            MatX<f64> computation = (a.array() - b.array()) / (a.array() + b.array());
            computation = computation.unaryExpr([](f64 v) { return std::isfinite(v) ? v : 0.0; });
            result.values = computation;
            result.write(tiff_path, tiff_template);
            return result;
        };

        auto compute_swi = [&folder, &tiff_path]() {
            auto green = utils::GeoTIFF<f64>{folder / "B03.tif"};
            auto swir = utils::GeoTIFF<f64>{folder / "B11.tif"};
            auto nir = utils::GeoTIFF<f64>{folder / "B08.tif"};

            fs::path tiff_template = folder / "B04.tif";
            auto result = utils::GeoTIFF<f64>(tiff_template);
            auto denominator = (green.values.array() + nir.values.array()) * (nir.values.array() + swir.values.array());
            result.values = (green.values.array() * (nir.values.array() - swir.values.array()) / denominator);
            result.values = result.values.unaryExpr([](f64 v) { return std::isfinite(v) ? v : 0.0; });
            result.write(tiff_path, tiff_template);
            return result;
        };

        switch (index) {
            case Indices::NDVI: {
                auto nir = utils::GeoTIFF<f64>{folder / "B08.tif"};
                auto red = utils::GeoTIFF<f64>{folder / "B04.tif"};
                return normalized_computation(nir.values, red.values);
            }
            case Indices::NDMI: {
                auto nir = utils::GeoTIFF<f64>{folder / "B08.tif"};
                auto swir = utils::GeoTIFF<f64>{folder / "B11.tif"};
                return normalized_computation(nir.values, swir.values);
            }
            case Indices::mNDWI: {
                auto green = utils::GeoTIFF<f64>{folder / "B03.tif"};
                auto swir = utils::GeoTIFF<f64>{folder / "B11.tif"};
                return normalized_computation(green.values, swir.values);
            }
            case Indices::SWI:
                return compute_swi();
            default:
                throw std::runtime_error(fmt::format("Failed to map the index: {}", magic_enum::enum_name(index)));
        }
    }

    std::optional<Indices> from_str(std::string_view str) {
        return magic_enum::enum_cast<Indices>(str);
    }

    template<class... Ts>
    struct Visitor : Ts ... {
        using Ts::operator()...;
    };

    struct ResultContainer {
        ResultContainer(Eigen::Index rows, Eigen::Index cols) {
            histogram_matrix = MatX<f64>::Zero(rows, cols);
            count_matrix = MatX<i32>::Zero(rows, cols);
        }

        // Counts whenever the index is above the threshold of interest for each pixel
        MatX<f64> histogram_matrix;
        // Counts whenever the index is valid for each pixel
        MatX<i32> count_matrix;
    };

    void single_image_summary(
            std::filesystem::path const &base_path,
            bool use_cache,
            int start_year,
            int end_year,
            Indices index,
            f64 threshold,
            DataChoices choices) {
        fs::path cache_path;
        if (use_cache) {
            cache_path = base_path / cache_string(start_year, end_year, index, threshold);
            if (fs::exists(cache_path)) {
                return;
            }
        }

        // A little class to handle our connection to the sqlite db
        DataBase db(base_path);

        std::vector<fs::path> folders_to_process;
        for (auto const &path: fs::directory_iterator(base_path)) {
            if (fs::is_directory(path) &&
                utils::find_directory_contents(path) == utils::DirectoryContents::MultiSpectral) {
                folders_to_process.push_back(path);
            }
        }

        auto index_name = fmt::format("{}.tif", magic_enum::enum_name(index));

        // Compute indices in a parallel-loop (which I hope will be nice and fast :)
        std::for_each(std::execution::par_unseq, folders_to_process.begin(), folders_to_process.end(),
                      [&](auto &&folder) {
                          fs::path index_path;
                          // Provide the option to skip the computation if we don't have any data
                          bool skip_computation = false;
                          std::visit(Visitor {
                              [&](UseApproximatedData const &) {
                                  index_path = folder / fs::path("approximated_data");
                                  if (fs::is_empty(index_path)) {
                                      skip_computation = true;
                                  }
                              },
                              [&](auto) { index_path = folder; }
                              }, choices);

                          if (skip_computation || fs::exists(index_path / fs::path(index_name)))
                              return;
                          else
                              compute_index(index_path, index);
                      });

        auto find_tiff_info = [&]() {
            for (auto const &path: folders_to_process) {
                utils::GeoTIFF<f64> tiff_file{path / fs::path("B04.tif")};
                return tiff_file;
            }
            throw std::runtime_error(
                    fmt::format("Failed to find a directory containing satellite data! Path: {}", base_path));
        };
        utils::GeoTIFF<f64> example_tiff = find_tiff_info();
        auto width = example_tiff.width;
        auto height = example_tiff.height;

        // Initialize
        std::unordered_map<int, ResultContainer> yearly_data;
        for (int year = start_year; year <= end_year; ++year) {
            yearly_data.emplace(year, ResultContainer{width, height});
        }
        ResultContainer overall_result(width, height);

        std::mutex mutex;
        std::for_each(std::execution::par_unseq, folders_to_process.begin(), folders_to_process.end(), [&](auto&& folder) {
            auto date = date_time::from_simple_string(folder.filename());
            if (date.year() < start_year || date.year() > end_year) {
                return;
            }
            std::visit(Visitor {
                    [&](UseApproximatedData) {
                        // This assumes that we have data for the folder. Need to confirm this!
                        utils::GeoTIFF<f64> index_tiff = compute_index(folder / fs::path("approximated_data"), index);

                        std::lock_guard<std::mutex> lock(mutex);
                        auto &data_for_year = yearly_data.at(date_time::from_simple_string(folder.filename()).year());
                        // For now with the approximated data we use every pixel
                        data_for_year.count_matrix = data_for_year.count_matrix + MatX<i32>::Ones(width, height);
                        overall_result.count_matrix = overall_result.count_matrix + MatX<i32>::Ones(width, height);

                        data_for_year.histogram_matrix = (
                                data_for_year.histogram_matrix.array() + (index_tiff.values.array() >= threshold).cast<f64>());
                        overall_result.histogram_matrix = (
                                overall_result.histogram_matrix.array() + (index_tiff.values.array() >= threshold).cast<f64>());
                    },
                    [&](UseRealData const &choice) {
                        utils::GeoTIFF<f64> index_tiff = compute_index(folder, index);
                        // Initially we assume that all pixels are valid
                        MatX<bool> valid_pixels = MatX<bool>::Ones(width, height);
                        auto status = db.get_status(folder.filename().string());
                        if (choice.exclude_cloudy_pixels && status.clouds_exist) {
                            fs::path cloud_path = folder / fs::path("cloud_mask.tif");
                            utils::GeoTIFF<u8> cloud_tiff(cloud_path);
                            // The valid pixels are the pixels that are not cloudy
                            valid_pixels = !cloud_tiff.values.cast<bool>().array();
                        }
                        if (choice.exclude_shadow_pixels && status.shadows_exist) {
                            fs::path shadow_path = folder / fs::path("shadow_mask.tif");
                            utils::GeoTIFF<u8> shadow_tiff(shadow_path);
                            // The valid pixels are the pixels that are not covered with shadows,
                            // and that were valid before (prevents from overwriting cloud pixels)
                            valid_pixels = (valid_pixels.array() && !shadow_tiff.values.cast<bool>().array());
                        }
                        // Any pixel that is determined to be invalid is given a value of -500, which will never be selected
                        index_tiff.values = valid_pixels.select(index_tiff.values, MatX<f64>::Constant(index_tiff.width, index_tiff.height, NO_DATA_INDICATOR));

                        std::lock_guard<std::mutex> lock(mutex);
                        auto &data_for_year = yearly_data.at(date_time::from_simple_string(folder.filename()).year());
                        data_for_year.count_matrix = data_for_year.count_matrix + valid_pixels.cast<i32>();
                        overall_result.count_matrix = overall_result.count_matrix + valid_pixels.cast<i32>();

                        data_for_year.histogram_matrix = (
                                data_for_year.histogram_matrix.array() + (index_tiff.values.array() >= threshold).cast<f64>());
                        overall_result.histogram_matrix = (
                                overall_result.histogram_matrix.array() + (index_tiff.values.array() >= threshold).cast<f64>());
                    }
            }, choices);
        });

        // Save the results to disk
        for (auto const &[year, result] : yearly_data) {
            MatX<f64> percent = (result.histogram_matrix.array() / result.count_matrix.cast<f64>().array());
            example_tiff.values = percent;
            example_tiff.write(cache_string(year, year, index, threshold).string());
        }
        MatX<f64> percent = (overall_result.histogram_matrix.array() / overall_result.count_matrix.cast<f64>().array());
        example_tiff.values = percent;
        example_tiff.write(cache_path);
    }
}