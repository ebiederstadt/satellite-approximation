#include <spatial_approximation/approx.h>
#include <utils/geotiff.h>
#include <utils/fmt_filesystem.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <gdal/gdal_priv.h>

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        spdlog::error("Usage: {} template_path output_path", argv[0]);
        return -1;
    }

    spdlog::set_level(spdlog::level::debug);
    GDALAllRegister();

    fs::path template_path = argv[1];
    if (!fs::exists(template_path)) {
        spdlog::error("Could not find provided template: {}", template_path);
    }
    utils::GeoTIFF<f64> template_geotiff(template_path.string());

    MatX<bool> cloud_matrix(template_geotiff.height, template_geotiff.width);
    cloud_matrix.fill(false);
    // What happens if the unknown region is on the edge?
    cloud_matrix.block<50, 50>(0, 0) = Eigen::Matrix<bool, 50, 50>::Ones();
    template_geotiff.values.block<50, 50>(0, 0) = Eigen::Matrix<f64, 50, 50>::Constant(0.0);

    fs::path output_path = argv[2];
    fs::path test_path = output_path.parent_path() / fs::path("test.tif");
    template_geotiff.write(test_path);

    spatial_approximation::fill_missing_portion_smooth_boundary(template_geotiff.values, cloud_matrix);

    template_geotiff.write(output_path);
}