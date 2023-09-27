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
            std::visit(
                Visitor {
                    [&](UseApproximatedData const&) {
                        index_path = folder / fs::path("approximated_data");
                        std::lock_guard<std::mutex> lock(mutex);
                        approx_data = db.get_approximated_data(folder.filename().string());
                    },
                    [&](auto) {
                        index_path = folder;
                        approx_data = required_files(index);
                    } },
                choices);

            // Don't bother to compute if we are missing files required for computation, or if the index already exists
            if (missing_files(approx_data, index) || fs::exists(index_path / index_name))
                return;
            else {
                utils::GeoTIFF<f64> result = compute_index(index_path, folder / "viewZenithMean.tif", index);
                std::lock_guard<std::mutex> lock(mutex);
                db.store_index_info(folder.filename().string(), index, result.values.minCoeff(), result.values.maxCoeff(), result.values.mean(), choices);
                num_computed += 1;
            }
        });
    logger->info("Calculated {} spectral indices in {:.2f}s", num_computed, sw);
}
}
