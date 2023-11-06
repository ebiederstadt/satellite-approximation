#include "analysis/filters.h"

#include <fmt/ostream.h>
#include <utils/error.h>
#include <utils/log.h>

namespace analysis {
static auto logger = utils::create_logger("analysis::filters");

f64 convolve(MatX<f64> const& input, MatX<f64> const& kernel, Eigen::Index in_row, Eigen::Index in_col)
{
    // Reflect an input around the boundary
    auto reflect = [](Eigen::Index& index, Eigen::Index max_index_plus_one) {
        if (index < 0) {
            index = -(index + 1);
        } else if (index >= max_index_plus_one) {
            index = (max_index_plus_one - 1) - (index - max_index_plus_one);
        }
    };

    f64 output = 0;

    Vec2<Eigen::Index> translation = (Vec2<Eigen::Index> { in_row, in_col }).array() - 1;

    // On the boundary, we are using the "reflect" boundary conditions
    // Working with the coordinates of the filter matrix
    for (Eigen::Index row = 0; row < kernel.rows(); ++row) {
        for (Eigen::Index col = 0; col < kernel.cols(); ++col) {
            Vec2<Eigen::Index> input_index = Vec2<Eigen::Index> { row, col } - translation;
            reflect(input_index(0), input.rows());
            reflect(input_index(1), input.cols());
            output += input(input_index(0), input_index(1)) * kernel(row, col);
        }
    }

    return output;
}

MatX<f64> frost_filter(MatX<f64> const& input_image, int kernel_size, f64 damping_factor)
{
    if (kernel_size % 2 != 1) {
        throw utils::GenericError(fmt::format("Kernel size must be an odd number. Provided size is {}", kernel_size), *logger);
    }

    MatX<f64> kernel(kernel_size, kernel_size);
    kernel.setZero();

    // Calculate S (the distance from each pixel)
    MatX<f64> distances(kernel_size, kernel_size);
    int center = kernel_size / 2;
    for (Eigen::Index i = 0; i < kernel_size; ++i) {
        for (Eigen::Index j = 0; j < kernel_size; ++j) {
            distances(i, j) = std::sqrt(std::pow(j - center, 2) + std::pow(i - center, 2));
        }
    }

    MatX<f64> average_matrix(kernel_size, kernel_size);
    average_matrix.setOnes();
    average_matrix = average_matrix.array() / static_cast<f64>(kernel_size * kernel_size);

    MatX<f64> squared_input = input_image.unaryExpr([](f64 v) { return v * v; });

    // Make a copy of the input image to store the result of the convolution
    MatX<f64> convolution_result(input_image.rows(), input_image.cols());

    int results = 0;
    int results_with_nan = 0;
    for (Eigen::Index row = 0; row < input_image.rows(); ++row) {
        for (Eigen::Index col = 0; col < input_image.cols(); ++col) {

            f64 mean = convolve(input_image, average_matrix, row, col);
            f64 mean_squared = convolve(squared_input, average_matrix, row, col);

            // sigma^2 = E(X^2) - E(x)^2
            f64 variance = mean_squared - (mean * mean);
            f64 B = damping_factor * (variance / mean * mean);

            // e^(-B * S)
            MatX<f64> weight = (-B * distances);
            weight = weight.unaryExpr([](f64 v) { return std::exp(v); });
            weight = weight.array() / weight.sum();

            f64 result = convolve(input_image, weight, row, col);
            if (std::isnan(result)) {
                result = 0.0;
                results_with_nan += 1;
            }
            convolution_result(row, col) = result;
            results += 1;
        }
    }

    logger->debug("{:.2f}% of the results have nans ({} out of {})", ((f64)results_with_nan / (f64)results) * 100, results_with_nan, results);

    return convolution_result;
}
}
