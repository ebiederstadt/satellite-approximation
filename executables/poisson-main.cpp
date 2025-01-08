#include <approx/poisson.h>
#include <fmt/std.h>
#include <gdal_priv.h>
#include <opencv2/core.hpp>
#include <opencv2/core/eigen.hpp>
#include <spdlog/spdlog.h>
#include <utils/geotiff.h>
#include <utils/log.h>

MatX<bool> preprocess_cloud_band(utils::GeoTIFF<f64> const& tiff, int cloud_band)
{
    MatX<f64> cloud_mask_float = tiff.read(cloud_band);
    int dilation_size = 5;
    cv::MorphShapes dilation_shape = cv::MorphShapes::MORPH_RECT;
    auto kernel = cv::getStructuringElement(dilation_shape, { 2 * dilation_size + 1, 2 * dilation_size + 1 });
    cv::Mat cloud_mask_cv;
    cv::eigen2cv(cloud_mask_float, cloud_mask_cv);
    cv::morphologyEx(cloud_mask_cv, cloud_mask_cv, cv::MorphTypes::MORPH_CLOSE, kernel);
    cv::cv2eigen(cloud_mask_cv, cloud_mask_float);
    return cloud_mask_float.cast<bool>();
}

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Log folder: {}", utils::log_location());

    if (argc != 3) {
        spdlog::info("Usage: {} input_path replacement_path", argv[0]);
        return -1;
    }
    fs::path input(argv[1]);
    fs::path replacement(argv[2]);

    uint physical_cores = std::thread::hardware_concurrency();
    Eigen::setNbThreads(static_cast<int>(physical_cores));
    spdlog::info("Using {} threads for OpenMP/Eigen", physical_cores);

    if (!fs::exists(input)) {
        spdlog::error("{} does not exist", input);
        return -1;
    }
    if (!fs::exists(replacement)) {
        spdlog::error("{} does not exist", replacement);
        return -1;
    }

    auto logger = utils::create_logger("poisson");
    spdlog::set_default_logger(logger);

    GDALAllRegister();

    std::vector<int> bands = { 1, 2, 3, 4, 5 };
    int cloud_band = 6;

    utils::GeoTIFF<f64> tiff(input);
    auto input_bands = tiff.read(bands);

    MatX<bool> cloudmask = preprocess_cloud_band(tiff, cloud_band);
    spdlog::info("Finished close + dilate");

    utils::GeoTIFF<f64> replacement_tiff(replacement);
    auto replacement_bands = replacement_tiff.read(bands);

    spdlog::info("Starting solver...");
    auto res = std::make_shared<std::vector<MatX<f64>>>(approx::blend_images_poisson(input_bands, replacement_bands, cloudmask));
    {
        spdlog::info("Finished solving. Writing results");
        utils::GeoTiffWriter writer(res, input);
        writer.write(input.parent_path() / "poisson_simple_replace" / input.filename());
    }
}
