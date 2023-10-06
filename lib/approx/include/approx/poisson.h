#pragma once

#include "utils.h"
#include <givde/types.hpp>

using namespace givde;

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
}