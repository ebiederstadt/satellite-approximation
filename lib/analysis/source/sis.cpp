#include "analysis/sis.h"
#include "analysis/db.h"
#include "analysis/utils.h"
#include "utils/filesystem.h"
#include "utils/geotiff.h"
#include "utils/log.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <execution>
#include <fmt/format.h>
#include <magic_enum.hpp>
#include <spdlog/stopwatch.h>
#include <sqlite3.h>
#include <vector>

namespace date_time = boost::gregorian;

static auto logger = utils::create_logger("analysis::SIS");

namespace analysis {
struct ResultContainer {
    ResultContainer(Eigen::Index rows, Eigen::Index cols)
    {
        histogram_matrix = MatX<f64>::Zero(rows, cols);
        count_matrix = MatX<i32>::Zero(rows, cols);
    }

    // Counts whenever the index is above the threshold of interest for each pixel
    MatX<f64> histogram_matrix;
    // Counts whenever the index is valid for each pixel
    MatX<i32> count_matrix;
    // If the result already exists on disk
    std::optional<int> result_if_exists;
    int num_days_used = 0;
};

std::string cache_string(int id, bool use_raw_data)
{
    return fmt::format("sis_{}{}.tif", id, use_raw_data ? "_raw" : "");
}

std::string count_string(int id)
{
    return fmt::format("count_{}.tif", id);
}

void single_image_summary(
    std::filesystem::path const& base_path,
    bool use_cache,
    int start_year,
    int end_year,
    utils::Indices index,
    f64 threshold,
    DataChoices choices)
{
    DataBase db(base_path);

    std::vector<fs::path> folders_to_process;
    for (auto const& path : fs::directory_iterator(base_path)) {
        if (fs::is_directory(path) && utils::find_directory_contents(path) == utils::DirectoryContents::MultiSpectral) {
            folders_to_process.push_back(path);
        }
    }

    auto find_tiff_info = [&]() {
        for (auto const& path : folders_to_process) {
            utils::GeoTIFF<f64> tiff_file { path / fs::path("viewZenithMean.tif") };
            return tiff_file;
        }
        throw std::runtime_error(
            fmt::format("Failed to find a directory containing satellite data! Path: {}", base_path));
    };

    utils::GeoTIFF<f64> example_tiff = find_tiff_info();
    auto ncols = example_tiff.width;
    auto nrows = example_tiff.height;

    // Initialize
    std::unordered_map<int, ResultContainer> yearly_data;
    for (int year = start_year; year <= end_year; ++year) {
        yearly_data.emplace(year, ResultContainer { nrows, ncols });
    }
    ResultContainer overall_result(nrows, ncols);
    if (use_cache) {
        overall_result.result_if_exists = db.result_exists(index, threshold, start_year, end_year, choices);
        for (auto& [year, data] : yearly_data) {
            data.result_if_exists = db.result_exists(index, threshold, year, year, choices);
        }
    }

    auto index_name = fmt::format("{}.tif", magic_enum::enum_name(index));

    compute_indices_for_all_dates(folders_to_process, index, db, choices);

    int num_dates_used_for_analysis = 0;
    spdlog::stopwatch sw;
    std::mutex mutex;
    std::for_each(std::execution::par_unseq, folders_to_process.begin(), folders_to_process.end(),
        [&](auto&& folder) {
            auto date = date_time::from_simple_string(folder.filename());
            if (date.year() < start_year || date.year() > end_year) {
                return;
            }
            std::visit(
                Visitor {
                    [&](UseApproximatedData) {
                        // Skip if we don't have any approximated data for the current site
                        if (!fs::exists(folder / "approximated_data" / index_name)) {
                            return;
                        }
                        auto year_string = date_time::from_simple_string(folder.filename()).year();
                        bool yearly_data_exists = yearly_data.at(year_string).result_if_exists.has_value();
                        bool overall_data_exists = overall_result.result_if_exists.has_value();
                        if (yearly_data_exists && overall_data_exists) {
                            return;
                        }

                        utils::GeoTIFF<f64> index_tiff = compute_index(
                            folder / fs::path("approximated_data"), folder / "viewZenithMean.tif", index);

                        std::lock_guard<std::mutex> lock(mutex);
                        auto& data_for_year = yearly_data.at(
                            date_time::from_simple_string(folder.filename()).year());
                        if (!yearly_data_exists) {
                            // For now with the approximated data we use every pixel
                            data_for_year.count_matrix = data_for_year.count_matrix + MatX<i32>::Ones(nrows, ncols);
                            data_for_year.histogram_matrix = data_for_year.histogram_matrix + MatX<f64>((index_tiff.values.array() >= threshold).cast<f64>());
                            data_for_year.num_days_used += 1;
                        }
                        if (!overall_data_exists) {
                            overall_result.count_matrix = overall_result.count_matrix + MatX<i32>::Ones(nrows, ncols);
                            overall_result.histogram_matrix = overall_result.histogram_matrix + MatX<f64>((index_tiff.values.array() >= threshold).cast<f64>());
                            overall_result.num_days_used += 1;
                        }
                        num_dates_used_for_analysis += 1;
                    },
                    [&](UseRealData const& choice) {
                        auto year_string = date_time::from_simple_string(folder.filename()).year();
                        bool yearly_data_exists = yearly_data.at(year_string).result_if_exists.has_value();
                        auto overall_data_exists = overall_result.result_if_exists.has_value();
                        if (yearly_data_exists && overall_data_exists) {
                            return;
                        }

                        utils::GeoTIFF<f64> index_tiff = utils::compute_index(folder, folder / "viewZenithMean.tif", index);
                        // Initially we assume that all pixels are valid
                        MatX<bool> valid_pixels = MatX<bool>::Ones(nrows, ncols);
                        utils::CloudShadowStatus status;
                        {
                            std::lock_guard<std::mutex> lock(mutex);
                            status = db.get_status(folder.filename().string());
                        }
                        if (choice.skip_threshold.has_value() && status.percent_invalid >= choice.skip_threshold.value()) {
                            return;
                        }
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
                        index_tiff.values = valid_pixels.select(index_tiff.values,
                            MatX<f64>::Constant(index_tiff.height,
                                index_tiff.width,
                                NO_DATA_INDICATOR));

                        std::lock_guard<std::mutex> lock(mutex);
                        auto& data_for_year = yearly_data.at(
                            date_time::from_simple_string(folder.filename()).year());
                        if (!yearly_data_exists) {
                            data_for_year.count_matrix = data_for_year.count_matrix + valid_pixels.cast<i32>();
                            data_for_year.histogram_matrix = data_for_year.histogram_matrix + MatX<f64>((index_tiff.values.array() >= threshold).cast<f64>());
                            data_for_year.num_days_used += 1;
                        }
                        if (!overall_data_exists) {
                            overall_result.count_matrix = overall_result.count_matrix + valid_pixels.cast<i32>();
                            overall_result.histogram_matrix = overall_result.histogram_matrix + MatX<f64>((index_tiff.values.array() >= threshold).cast<f64>());
                            overall_result.num_days_used += 1;
                        }
                        num_dates_used_for_analysis += 1;
                    } },
                choices);
        });

    logger->info("{} days used in analysis. Took {:.2f}s to compute", num_dates_used_for_analysis, sw);

    // Save the results to disk
    for (auto const& [year, result] : yearly_data) {
        if (result.result_if_exists.has_value())
            continue;

        MatX<f64> percent = (result.histogram_matrix.array() / result.count_matrix.cast<f64>().array());
        example_tiff.values = percent;
        int id = db.save_result_in_table(index, threshold, year, year, choices, percent.minCoeff(), percent.maxCoeff(), percent.mean(), result.num_days_used);
        example_tiff.write(base_path / fs::path(cache_string(id, false)));

        example_tiff.values = result.histogram_matrix;
        example_tiff.write(base_path / cache_string(id, true));

        example_tiff.values = result.count_matrix.cast<f64>();
        example_tiff.write(base_path / count_string(id));
    }
    if (overall_result.result_if_exists.has_value())
        return;

    MatX<f64> percent = (overall_result.histogram_matrix.array() / overall_result.count_matrix.cast<f64>().array());
    example_tiff.values = percent;
    int id = db.save_result_in_table(index, threshold, start_year, end_year, choices, percent.minCoeff(), percent.maxCoeff(), percent.mean(), overall_result.num_days_used);
    example_tiff.write(base_path / fs::path(cache_string(id, false)));

    example_tiff.values = overall_result.histogram_matrix;
    example_tiff.write(base_path / cache_string(id, true));

    example_tiff.values = overall_result.count_matrix.cast<f64>();
    example_tiff.write(base_path / count_string(id));
}

void compute_indices_for_all_dates(std::vector<fs::path> const& folders_to_process, utils::Indices index, DataBase& db, DataChoices choices)
{
    int num_computed = 0;
    auto index_name = fmt::format("{}.tif", magic_enum::enum_name(index));
    spdlog::stopwatch sw;
    std::mutex mutex;
    // Compute indices in a parallel-loop (which I hope will be nice and fast :)
    std::for_each(std::execution::par_unseq, folders_to_process.begin(), folders_to_process.end(),
        [&](auto&& folder) {
            fs::path index_path;
            fs::path template_path;
            // Provide the option to skip the computation if we don't have any data
            std::vector<std::string> approx_data;
            std::optional<MatX<bool>> valid_pixels;
            std::visit(
                Visitor {
                    [&](UseApproximatedData const&) {
                        index_path = folder / fs::path("approximated_data");
                        std::lock_guard<std::mutex> lock(mutex);
                        approx_data = db.get_approximated_data(folder.filename().string());
                    },
                    [&](UseRealData const& choice) {
                        index_path = folder;
                        approx_data = utils::required_files(index);
                        if (choice.exclude_cloudy_pixels) {
                            fs::path cloud_path = folder / fs::path("cloud_mask.tif");
                            utils::GeoTIFF<u8> cloud_tiff(cloud_path);
                            // The valid pixels are the pixels that are not cloudy
                            valid_pixels = !cloud_tiff.values.cast<bool>().array();
                        }
                        if (choice.exclude_shadow_pixels) {
                            fs::path shadow_path = folder / fs::path("shadow_mask.tif");
                            utils::CloudShadowStatus status;
                            {
                                std::lock_guard<std::mutex> lock(mutex);
                                status = db.get_status(folder.filename().string());
                            }
                            if (status.shadows_exist) {
                                utils::GeoTIFF<u8> shadow_tiff(shadow_path);
                                // The valid pixels are the pixels that are not covered with shadows,
                                // and that were valid before (prevents from overwriting cloud pixels)
                                valid_pixels = (valid_pixels->array() && !shadow_tiff.values.cast<bool>().array());
                            }
                        }
                    } },
                choices);

            // Don't bother to compute if we are missing files required for computation, or if the index already exists
            if (missing_files(approx_data, index) || fs::exists(index_path / index_name))
                return;
            else {
                utils::GeoTIFF<f64> result = compute_index(index_path, folder / "viewZenithMean.tif", index);
                std::lock_guard<std::mutex> lock(mutex);
                db.store_index_info(folder.filename().string(), index, result.values, choices);
                if (valid_pixels.has_value()) {
                    db.store_index_info(folder.filename().string(), index, result.values, *valid_pixels, std::get<UseRealData>(choices));
                }
                num_computed += 1;
            }
        });
    logger->info("Calculated {} spectral indices in {:.2f}s", num_computed, sw);
}
}
