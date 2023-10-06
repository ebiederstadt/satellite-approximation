#include "approx/poisson.h"
#include "approx/utils.h"
#include "utils/log.h"

namespace approx {
auto logger = utils::create_logger("approx::poisson");

void blend_images_poisson(MultiChannelImage& input_images, MultiChannelImage const& replacement_images, int start_row, int start_column, f64 sentinel_value)
{
    // Sanity checks
    if (replacement_images.size() > input_images.size()) {
        logger->error("Cannot solve problem: replacement image is larger than the input image ({} vs {})", replacement_images.size(), input_images.size());
        return;
    }
    if (start_row < 0 || start_column < 0 || start_row >= input_images.rows() || start_column >= input_images.cols()) {
        logger->error("Cannot solve problem: row/column is out of bounds. Row: {}, Column: {}", start_row, start_column);
        return;
    }

    auto valid_pixel = [&](Eigen::Index row, Eigen::Index col) {
        bool invalid = replacement_images(0, row, col) == 255.0 && replacement_images(1, row, col) == 0.0 && replacement_images(2, row, col) == 0.0;
        return !invalid;
    };

    // Coordinate transformations
    auto replacement_to_input = [&](Eigen::Index row, Eigen::Index col) {
        return index_t { row + start_row, col + start_column };
    };

    auto input_to_replacement = [&](Eigen::Index row, Eigen::Index col) {
        return index_t { row - start_row, col - start_column };
    };

    auto flatten_index = [&](Eigen::Index row, Eigen::Index col) {
        return col + row * replacement_images.cols();
    };

    // Map from the flattened index, into the number of variables
    std::unordered_map<Eigen::Index, int> variable_numbers;
    int i = 0;
    for (Eigen::Index row = 0; row < replacement_images.rows(); ++row) {
        for (Eigen::Index col = 0; col < replacement_images.cols(); ++col) {
            if (valid_pixel(row, col)) {
                variable_numbers.emplace(flatten_index(row, col), i);
                i += 1;
            }
        }
    }
    auto num_unknowns = (int)variable_numbers.size();

    std::vector<triplet_t> triplets;

    int irow = 0;
    // The A matrix is generated from the missing area, and does not depend on the specific image we are solving for
    for (Eigen::Index row = 0; row < replacement_images.rows(); ++row) {
        for (Eigen::Index col = 0; col < replacement_images.cols(); ++col) {
            if (!valid_pixel(row, col))
                continue;

            // First, take care of the left hand side of the equation
            std::vector<index_t> neighbours = valid_neighbours(replacement_images[0], { row, col });
            triplets.emplace_back(irow, variable_numbers.at(flatten_index(row, col)), (f64)neighbours.size());

            for (auto const& [nrow, ncol] : neighbours) {
                // Sum over all the neighbours that are not on the boundary
                if (valid_pixel(nrow, ncol)) {
                    triplets.emplace_back(irow, variable_numbers.at(flatten_index(nrow, ncol)), -1);
                }
            }

            irow += 1;
        }
    }

    sparse_t A(num_unknowns, num_unknowns);
    A.setFromTriplets(triplets.begin(), triplets.end());
    cholesky_t chol(A);

    // Solve for each channel of the multi-band image
    std::vector<VecX<f64>> solutions;

    logger->debug("Solving the system for {} image channels", input_images.images.size());
    for (size_t c = 0; c < input_images.images.size(); ++c) {
        VecX<f64> b(num_unknowns);
        b.setZero();
        irow = 0;

        for (Eigen::Index row = 0; row < replacement_images.rows(); ++row) {
            for (Eigen::Index col = 0; col < replacement_images.cols(); ++col) {
                if (!valid_pixel(row, col))
                    continue;

                std::vector<index_t> neighbours = valid_neighbours(replacement_images[0], { row, col });
                for (auto const& [nrow, ncol] : neighbours) {
                    // This computes the sum of the gradient (difference) for each pixel in the replacement image
                    b(irow) += (replacement_images.images[c](row, col) - replacement_images.images[c](nrow, ncol));

                    // If the neighbours are not part of the mask, they must be on the region boundary.
                    // In that case we will add them to the RHS of the equation, because their values are known
                    if (!valid_pixel(nrow, ncol)) {
                        index_t index_in_input = replacement_to_input(nrow, ncol);
                        b(irow) += input_images(c, index_in_input.row, index_in_input.col);
                    }
                }

                irow += 1;
            }
        }

        solutions.push_back(chol.solve(b));
    }

    // Put the new values into the images
    for (size_t c = 0; c < input_images.images.size(); ++c) {
        for (Eigen::Index row = 0; row < replacement_images.rows(); ++row) {
            for (Eigen::Index col = 0; col < replacement_images.cols(); ++col) {
                // No need to do any modifications if the pixel is nto part of the mask
                if (!valid_pixel(row, col))
                    continue;

                index_t index_input = replacement_to_input(row, col);
                input_images.images[c](index_input.row, index_input.col) = solutions[c](variable_numbers.at(flatten_index(row, col)));
            }
        }
    }
}
}
