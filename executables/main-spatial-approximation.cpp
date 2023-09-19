#include <cloud_shadow_detection/types.h>
#include <cloud_shadow_detection/Imageio.h>
#include <cloud_shadow_detection/fmt_filesystem.h>
#include <spatial_approximation/approx.h>
#include <spdlog/spdlog.h>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc != 4) {
        spdlog::error("Usage: {} image_path.tif, cloud_path.tif output_path.tif", argv[0]);
        return -1;
    }

    spdlog::set_level(spdlog::level::debug);

    fs::path data_path = argv[1];
    if (!fs::exists(data_path)) {
        spdlog::error("Data path does not exist: {}", data_path);
        return -1;
    }
    std::shared_ptr<ImageFloat> data = Imageio::ReadSingleChannelFloat(data_path);
    MatX<f64> data_matrix = data->cast<f64>();

    fs::path cloud_path = argv[2];
    if (!fs::exists(cloud_path)) {
        spdlog::error("Cloud path does not exist: {}", cloud_path);
        return -1;
    }
    std::shared_ptr<ImageUint> data_clouds = Imageio::ReadSingleChannelUint8(cloud_path);
    MatX<bool> cloud_matrix = data_clouds->cast<bool>();

    spatial_approximation::fill_missing_portion_smooth_boundary(data_matrix, cloud_matrix);

    fs::path output_path = argv[3];
    std::shared_ptr<ImageFloat> output_image = std::make_shared<ImageFloat>(data_matrix.cast<f32>());
    Imageio::WriteSingleChannelFloat(output_path, output_image);
}