#pragma once

#include "db.h"
#include "utils.h"
#include <givde/types.hpp>

using namespace givde;
namespace date_time = boost::gregorian;

namespace approx {
/**
 * Blend two images together using the poisson equation.
 * @param input_images: The original image(s)
 * @param replacement_images: New image(s) that we want to blend with the original image(s).
 * @param start_row: An offset into the original image, representing the row where replacement should start
 * @param start_column: An offset into the original image, representing the column where replacement should start
 * @param sentinel_value: A value indicating that the pixel in the replacement image should not be replaced.
 *  Eigen matrices cannot have arbitrary structures, but we want to allow for arbitrary structures
 */
void blend_images_poisson(
    MultiChannelImage& input_images,
    MultiChannelImage const& replacement_images,
    int start_row, int start_column,
    f64 sentinel_value);

/**
 * Blend two images together using the poisson equation.
 * @param input_images: The original image(s)
 * @param replacement_images: New image(s) that we want to blend with the original image(s).
 * @param invalid_mask: A mask with true at places where the pixels in the original image are invalid, and false where the pixels are determined to be valid
 */
void blend_images_poisson(
    MultiChannelImage& input_images,
    MultiChannelImage const& replacement_images,
    MatX<bool> const &invalid_mask);

void highlight_area_replaced(MultiChannelImage &input_images, MultiChannelImage const &replacement_images, int start_row, int start_col, Vec3<f64> const &color);

/**
 * Find a good image that is close to the current image, but
 * @param date_string
 * @param use_denoised_data
 * @param distance_weight
 * @param db
 * @return
 */
std::string find_good_close_image(std::string const &date_string, bool use_denoised_data, f64 distance_weight, DataBase &db);
void fill_missing_data_folder(fs::path base_folder, std::vector<std::string> band_names, bool use_cache, bool use_denoised_data, f64 distance_weight, f64 skip_threshold);
}