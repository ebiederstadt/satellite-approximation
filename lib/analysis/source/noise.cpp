#include "analysis/noise.h"

#include <execution>
#include <opencv2/core/eigen.hpp>
#include <opencv2/imgproc.hpp>
#include <utils/eigen.h>
#include <utils/filesystem.h>
#include <utils/geotiff.h>
#include <utils/log.h>

namespace analysis {
static auto logger = utils::create_logger("analysis::noise");

void remove_noise_in_clouds_and_shadows(fs::path folder, int min_region_size, bool use_cache, DataBase& db)
{
    std::mutex mutex;
    if (use_cache) {
        std::lock_guard<std::mutex> lock(mutex);
        if (db.noise_exists(folder.filename(), min_region_size)) {
            return;
        }
    }

    CloudShadowStatus status;
    {
        std::lock_guard<std::mutex> lock(mutex);
        status = db.get_status(folder.filename());
    }
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

    logger->debug("Before removing regions, {:.2f}% of the pixels are invalid", 100 * utils::percent_non_zero(invalid_pixels));
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

    f64 percent_invalid = utils::percent_non_zero(eigen_flood_result);
    logger->debug("After flood fill, {:.2f}% pixels are invalid", 100 * percent_invalid);

    // Write values to db
    tiff.values = eigen_flood_result.cast<u8>();
    {
        std::lock_guard<std::mutex> lock(mutex);
        tiff.write(folder / "cloud_shadows_noise_removed.tif");
        db.save_noise_removal(folder.filename(), percent_invalid, min_region_size);
    }
}

void remove_noise_folder(fs::path base_folder, int min_region_size, bool use_cache, DataBase& db)
{
    if (!is_directory(base_folder)) {
        logger->warn("Could not process. The provided path is not a folder: {}", base_folder);
        return;
    }

    std::vector<fs::path> folders_to_process;
    for (auto const& folder : fs::directory_iterator(base_folder)) {
        if (utils::find_directory_contents(folder) == utils::DirectoryContents::MultiSpectral) {
            folders_to_process.push_back(folder);
        }
    }

    std::for_each(std::execution::par_unseq, folders_to_process.begin(), folders_to_process.end(), [&](std::filesystem::path const& path) {
        remove_noise_in_clouds_and_shadows(path, min_region_size, use_cache, db);
    });
}
}
