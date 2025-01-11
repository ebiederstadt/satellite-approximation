#pragma once

#include "db.h"
#include "utils.h"

#include <utils/types.h>

namespace date_time = boost::gregorian;
using namespace utils;

namespace approx {
struct PerfInfo {
    long region_size = 0;
    f64 tolerance = 0.0;
    long max_iterations = 0;
    long iterations = 0;
    f64 error = 0.0;
    f64 solve_time = 0.0;

    void write(fs::path const& output) const;
};

/**
 * Blend two images together using the poisson equation.
 * @param input_images: The original image(s)
 * @param replacement_images: New image(s) that we want to blend with the original image(s).
 * @param start_row: An offset into the original image, representing the row where replacement should start
 * @param start_column: An offset into the original image, representing the column where replacement should start
 */
void blend_images_poisson(
    MultiChannelImage& input_images,
    MultiChannelImage const& replacement_images,
    int start_row, int start_column);

/**
 * Blend two images together using the poisson equation.
 * @param input_images: The original image(s)
 * @param replacement_images: New image(s) that we want to blend with the original image(s).
 * @param invalid_mask: A mask with true at places where the pixels in the original image are invalid, and false where the pixels are determined to be valid
 */
void blend_images_poisson(
    MultiChannelImage& input_images,
    MultiChannelImage const& replacement_images,
    MatX<bool> const& invalid_mask,
    f64 tolerance = 1e-6,
    std::optional<int> max_iterations = {});
std::vector<MatX<f64>> blend_images_poisson(
    std::vector<MatX<f64>> const& input_images,
    std::vector<MatX<f64>> const& replacement_images,
    MatX<bool> const& invalid_mask,
    f64 tolerance = 1e-6,
    std::optional<int> max_iterations = {});

void highlight_area_replaced(MultiChannelImage& input_images, MultiChannelImage const& replacement_images, int start_row, int start_col, Vec3<f64> const& color);

/**
 * Find a good image that is close to the current image, but
 * @param date_string
 * @param distance_weight
 * @param db
 * @return
 */
std::string find_good_close_image(std::string const& date_string, f64 distance_weight, DataBase& db);
}