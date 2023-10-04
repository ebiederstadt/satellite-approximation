#pragma once

#include <givde/types.hpp>

using namespace givde;

namespace approx {
/**
 * Blend two images together using the poisson equation.
 * @param input_image: The original image
 * @param replacement_image: A new image that we want to blend with the original image.
 * @param start_row: An offset into the original image, representing the row where replacement should start
 * @param start_column: An offset into the original image, representing the column where replacement should start
 * @param sentinel_value: A value indicating that the pixel in the replacement image should not be replaced.
 *  Eigen matrices cannot have arbitrary structures, but we want to allow for arbitrary structures
 */
void blend_images_poisson(
    MatX<f64>& input_image,
    MatX<f64> const &replacement_image,
    int start_row, int start_column,
    f64 sentinel_value);
}