#include <cloud_shadow_detection/automatic_detection.h>
#include <gdal/gdal_priv.h>
#include <utils/geotiff.h>
#include <utils/log.h>

int main()
{
    GDALAllRegister();
    spdlog::info("Log location: {}", utils::log_location());

    // clang-format off
    std::array<f64, 4> bbox = { 56.92120903, -111.93141764,
                                57.10578757, -111.6817218 };
    // clang-format on

    fs::path base_folder = "/home/ebiederstadt/Documents/sentinel_cache/bbox-111.9314176_56.921209032_-111.6817217_57.105787570/2019-05-22";
    f32 diagonal_distance = remote_sensing::get_diagonal_distance(bbox[1], bbox[0], bbox[3], bbox[2]);
    remote_sensing::detect_single_folder(base_folder, diagonal_distance, {}, false);
}