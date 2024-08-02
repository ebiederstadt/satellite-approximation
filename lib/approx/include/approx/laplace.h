#pragma once

#include "utils.h"

#include <filesystem>
#include <opencv2/core.hpp>

namespace fs = std::filesystem;

namespace approx {
struct ConnectedComponents {
    MatX<int> matrix;
    std::unordered_map<int, std::vector<index_t>> region_map;
};

/**
 * Find the portions of an image that are connected to each other
 * @param image: a binary image with true == pixel is invalid
 */
ConnectedComponents find_connected_components(MatX<bool> const& invalid);

/** Fill a missing region of an image, assuming that boundary of the image is smooth
 * (Laplace equation with dirichlet boundary conditions)
 * @param input_image: The input image
 * @param invalid_mask: A mask identifying what portions of the image are invalid
 * @returns: A matrix representing approximation status for each region
 */
void fill_missing_portion_smooth_boundary(MatX<f64>& input_image, MatX<bool> const& invalid_pixels);

// Apply the laplace equation to an image.
cv::Mat apply_laplace(cv::Mat const &image, cv::Mat const &invalid_region, f64 red_threshold);

//void fill_missing_data_folder(fs::path folder, std::vector<std::string> band_names, bool use_cache, f64 skip_threshold, bool use_denoised_data);
}