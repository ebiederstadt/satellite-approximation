#include "analysis/noise.h"

#include <opencv2/core/eigen.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <utils/eigen.h>
#include <utils/geotiff.h>
#include <utils/log.h>

namespace analysis {
static auto logger = utils::create_logger("analysis::noise");

void remove_noise_in_clouds_and_shadows(fs::path folder, int min_region_size, bool use_cache, DataBase& db)
{
    if (use_cache && fs::exists(folder / "cloud_shadows_noise_removed.tif")) {
        return;
    }

    CloudShadowStatus status = db.get_status(folder.filename());
    if (!(status.shadows_exist && status.clouds_exist)) {
        logger->warn("Could not compute: clouds and shadows both do not exist in {}", folder);
        return;
    }

    utils::GeoTIFF<u8> tiff(folder / "cloud_mask.tif");
    MatX<bool> invalid_pixels = tiff.values.cast<bool>();
    if (status.shadows_exist) {
        invalid_pixels = invalid_pixels || utils::GeoTIFF<u8>(folder / "shadow_mask.tif").values.cast<bool>();
    }
    MatX<int> eigen_flood_result = (invalid_pixels).select(-100, invalid_pixels.cast<int>());
    cv::Mat flood_result;
    cv::eigen2cv(eigen_flood_result, flood_result);
    eigen_flood_result.setZero();

    int height = flood_result.rows;
    int width = flood_result.cols;
    int nelem = 0;

    logger->debug("Performing flood fill with an image of {}x{}", width, height);
    logger->debug("Before removing regions, {:.2f}% of the pixels are invalid", 100 * utils::percent_valid(invalid_pixels));
    for (int x = 0; x < height; x++) {
        for (int y = 0; y < width; y++) {
            if (flood_result.at<int>(x, y) == -100) {
                nelem++;

                cv::Rect rect;
                cv::Mat flood_mask = cv::Mat::zeros(flood_result.rows + 2, flood_result.cols + 2, CV_8U);
                int num_filled_pixels = cv::floodFill(flood_result, flood_mask, cv::Point(y, x), cv::Scalar(nelem), &rect, 0, 8);

                for (int i = rect.y; i < rect.y + rect.height; i++) {
                    for (int j = rect.x; j < rect.x + rect.width; j++) {
                        if (flood_mask.at<uchar>(i, j)) {
                            eigen_flood_result(i, j) = (num_filled_pixels < min_region_size) ? 0 : 1;
                        }
                    }
                }
            }
        }
    }

    logger->debug("After flood fill, {:.2f}% pixels are invalid", 100 * utils::percent_valid(eigen_flood_result));
    logger->debug("Min: {}, max: {}, mean: {}", eigen_flood_result.minCoeff(), eigen_flood_result.maxCoeff(), eigen_flood_result.mean());

    tiff.values = eigen_flood_result.cast<u8>();
    tiff.write(folder / "cloud_shadows_noise_removed.tif");
}
}
