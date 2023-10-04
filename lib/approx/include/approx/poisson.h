#pragma once

#include <givde/types.hpp>

using namespace givde;

namespace approx {
struct MultiChannelImage {
    explicit MultiChannelImage(std::vector<MatX<f64>> images)
        : images(std::move(images))
    {
    }

    std::vector<MatX<f64>> images;

    f64 operator()(size_t c, Eigen::Index row, Eigen::Index col) const
    {
        return images.at(c)(row, col);
    }

    MatX<f64> operator[](size_t c) const
    {
        return images[c];
    }

    Eigen::Index size() const
    {
        return images[0].size();
    }

    Eigen::Index rows() const
    {
        return images[0].rows();
    }

    Eigen::Index cols() const
    {
        return images[0].cols();
    }
};

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