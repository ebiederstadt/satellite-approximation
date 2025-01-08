#include <fmt/std.h>
#include <gdal_priv.h>
#include <spdlog/spdlog.h>
#include <utils/geotiff.h>

#include <cloud_shadow_detection/ComputeEnvironment.h>
#include <cloud_shadow_detection/GaussianBlur.h>
#include <cloud_shadow_detection/PitFillAlgorithm.h>
#include <cloud_shadow_detection/automatic_detection.h>

using namespace remote_sensing;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        spdlog::error("Usage: {} working_dir");
        return 1;
    }
    fs::path base_folder = argv[1];

    ComputeEnvironment::InitMainContext();
    GaussianBlur::init();
    PitFillAlgorithm::init();

    GDALAllRegister();
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Log location: {}", utils::log_location());

    // clang-format off
    std::array<f64, 4> bbox = { 56.92120903285525, 111.93141764318219,
                                57.105787570770836, -111.68172179675481 };
    // clang-format on
    auto diagonal_distance = get_diagonal_distance(bbox[1], bbox[0], bbox[3], bbox[2]);

    fs::path folder = base_folder / "test_data" / "2019-05-22";
    CloudParams params(folder);
    auto status = detect(params, diagonal_distance, {}, false);
    if (status.has_value()) {
        if (status->percent_shadows.has_value()) {
            spdlog::info("Finished detection procedure. {:.3f}% of the region was invalid, with {:.3f}% clouds and {:.3f}% shadows\"",
                status->percent_invalid * 100, status->percent_clouds * 100, *status->percent_shadows * 100);
        } else {
            spdlog::info("Finished detection procedure. {:.3f}% of the region was invalid, with {:.3f}% clouds",
                status->percent_invalid * 100, status->percent_clouds * 100);
        }

    } else {
        spdlog::warn("Failed to compute");
    }
}