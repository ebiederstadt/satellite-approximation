#include "analysis/temporal.h"
#include "analysis/db.h"
#include "analysis/utils.h"
#include "utils/log.h"

#include <execution>
#include <filesystem>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <variant>

namespace fs = std::filesystem;
static auto logger = utils::create_logger("analysis::temporal");

namespace analysis {
void compute_indices_for_all_dates(std::vector<fs::path> const& folders_to_process, Indices index, DataBase& db, DataChoices choices)
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
                        approx_data = required_files(index);
                        if (choice.exclude_cloudy_pixels) {
                            fs::path cloud_path = folder / fs::path("cloud_mask.tif");
                            utils::GeoTIFF<u8> cloud_tiff(cloud_path);
                            // The valid pixels are the pixels that are not cloudy
                            valid_pixels = !cloud_tiff.values.cast<bool>().array();
                        }
                        if (choice.exclude_shadow_pixels) {
                            fs::path shadow_path = folder / fs::path("shadow_mask.tif");
                            CloudShadowStatus status;
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
