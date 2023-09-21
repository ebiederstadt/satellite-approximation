#include <spatial_approximation/approx.h>
#include <utils/geotiff.h>
#include <utils/fmt_filesystem.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <gdal/gdal_priv.h>

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    if (argc != 4) {
        spdlog::error("Usage: {} data_path cloud_path output_path", argv[0]);
        return -1;
    }

    spdlog::set_level(spdlog::level::debug);
    GDALAllRegister();

    fs::path data_path = argv[1];
    if (!fs::exists(data_path)) {
        spdlog::error("Could not find provided template: {}", data_path);
    }
    utils::GeoTIFF<f64> data_geotiff(data_path.string());

    fs::path cloud_path = argv[2];
    utils::GeoTIFF<u8> cloud_geotiff(cloud_path);

    spatial_approximation::fill_missing_portion_smooth_boundary(data_geotiff.values, cloud_geotiff.values.cast<bool>());

    fs::path output_path = argv[3];
    data_geotiff.write(output_path);
}