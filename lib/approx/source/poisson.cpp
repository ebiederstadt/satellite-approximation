#include "approx/poisson.h"
#include "approx/utils.h"
#include "utils/log.h"

namespace approx {
auto logger = utils::create_logger("approx::poisson");

void blend_images_poisson(MatX<f64>& input_image, MatX<f64> const& replacement_image, int start_row, int start_column, f64 sentinel_value)
{
    // Sanity checks
    if (replacement_image.size() > input_image.size()) {
        logger->error("Cannot solve problem: replacement image is larger than the input image ({} vs {})", replacement_image.size(), input_image.size());
        return;
    }
    if (start_row < 0 || start_column < 0 || start_row >= input_image.rows() || start_column >= input_image.cols()) {
        logger->error("Cannot solve problem: row/column is out of bounds. Row: {}, Column: {}", start_row, start_column);
        return;
    }

    auto valid_pixel = [&](Eigen::Index row, Eigen::Index col) {
        return replacement_image(row, col) != sentinel_value;
    };

    // Convert from the replacement image coordinates to the main image coordinates
    auto replacement_to_input = [&](Eigen::Index row, Eigen::Index col) {
        return index_t { row + start_row, col + start_column };
    };

    auto flatten_index = [&](Eigen::Index row, Eigen::Index col) {
        return col + row * replacement_image.cols();
    };

    // Map from the flattened index, into the number of variables
    std::unordered_map<Eigen::Index, int> variable_numbers;
    int i = 0;
    for (Eigen::Index row = 0; row < replacement_image.rows(); ++row) {
        for (Eigen::Index col = 0; col < replacement_image.cols(); ++col) {
            if (valid_pixel(row, col)) {
                variable_numbers.emplace(flatten_index(row, col), i);
                i += 1;
            }
        }
    }
    auto num_unknowns = (int)variable_numbers.size();

    std::vector<triplet_t> triplets;
    VecX<f64> b(num_unknowns);
    b.setZero();

    int irow = 0;
    for (Eigen::Index row = 0; row < replacement_image.rows(); ++row) {
        for (Eigen::Index col = 0; col < replacement_image.cols(); ++col) {
            if (!valid_pixel(row, col))
                continue;

            // First, take care of the left hand side of the equation
            std::vector<index_t> neighbours = valid_neighbours(replacement_image, { row, col });
            triplets.emplace_back(irow, variable_numbers.at(flatten_index(row, col)), (f64)neighbours.size());

            for (auto const& [nrow, ncol] : neighbours) {
                // This computes the sum of the gradient (difference) for each pixel in the replacement image
                b(irow) += (replacement_image(row, col) - replacement_image(nrow, ncol));

                // Sum over all the neighbours that are not on the boundary
                if (valid_pixel(nrow, ncol)) {
                    triplets.emplace_back(irow, variable_numbers.at(flatten_index(nrow, ncol)), -1);
                }
                // If the neighbours are not part of the mask, they must be on the region boundary.
                // In that case we will add them to the RHS of the equation, because their values are known from
                else {
                    index_t index_in_input = replacement_to_input(nrow, ncol);
                    b(irow) += input_image(index_in_input.row, index_in_input.col);
                }
            }

            irow += 1;
        }
    }

    sparse_t A(num_unknowns, num_unknowns);
    A.setFromTriplets(triplets.begin(), triplets.end());
    cholesky_t chol(A);
    VecX<f64> values = chol.solve(b);

    // Put the new values into the image
    for (Eigen::Index row = 0; row < replacement_image.rows(); ++row) {
        for (Eigen::Index col = 0; col < replacement_image.cols(); ++col) {
            auto [nrow, ncol] = replacement_to_input(row, col);
            input_image(nrow, ncol) = replacement_image(row, col);
        }
    }
}
}
