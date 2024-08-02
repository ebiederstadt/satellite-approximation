#include <approx/poisson.h>
#include <gdal_priv.h>
#include <opencv2/core.hpp>
#include <opencv2/core/eigen.hpp>
#include <spdlog/spdlog.h>
#include <utils/geotiff.h>
#include <utils/log.h>

int main(int argc, char **argv)
{
    if (argc != 3) {
        spdlog::info("Usage: {} input_path replacement_path", argv[0]);
        return -1;
    }
    fs::path input(argv[1]);
    fs::path replacement(argv[2]);

    if (!fs::exists(input)) {
        spdlog::error("{} does not exist", input);
        return -1;
    } if (!fs::exists(replacement)) {
        spdlog::error("{} does not exist", replacement);
        return -1;
    }

    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Log location: {}", utils::log_location());
    GDALAllRegister();

    std::vector<int> bands = {1,2,3,4,5};
    int cloud_band = 6;

    utils::GeoTIFF<f64> tiff(input);
    auto input_bands = tiff.read(bands);

    MatX<bool> cloudmask;
    {
        MatX<f64> cloud_mask_float = tiff.read(cloud_band).cast<f64>();
        int dilation_size = 10;
        cv::MorphShapes dilation_shape = cv::MorphShapes::MORPH_RECT;
        auto kernel = cv::getStructuringElement(dilation_shape,  {2 * dilation_size + 1, 2 * dilation_size + 1});
        cv::Mat cloud_mask_cv;
        cv::eigen2cv(cloud_mask_float, cloud_mask_cv);
        cv::morphologyEx(cloud_mask_cv, cloud_mask_cv, cv::MorphTypes::MORPH_CLOSE, kernel);
        cv::dilate(cloud_mask_cv, cloud_mask_cv, kernel);
        cv::cv2eigen(cloud_mask_cv, cloud_mask_float);
        cloudmask = cloud_mask_float.cast<bool>();
    }

    utils::GeoTIFF<f64> replacement_tiff(replacement);
    auto replacement_bands = replacement_tiff.read(bands);

    auto res = approx::blend_images_poisson(input_bands, replacement_bands, cloudmask);
    {
        utils::GeoTiffWriter writer(res, input);
        writer.write(input.parent_path() / "poisson_simple_replace" / "s2_37.tif");
    }
}
