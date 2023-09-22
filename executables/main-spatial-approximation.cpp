#include <spatial_approximation/approx.h>
#include <utils/geotiff.h>
#include <utils/fmt_filesystem.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <gdal/gdal_priv.h>

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        spdlog::error("Usage: {} data_path", argv[0]);
        return -1;
    }

    spdlog::set_level(spdlog::level::debug);
    GDALAllRegister();

    spatial_approximation::fill_missing_data_folder(fs::path(argv[1]), {"B02", "B03", "B04", "B08", "B11"}, true, 0.8);
}