#include "approx/poisson.h"
#include "approx/utils.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <spdlog/stopwatch.h>
#include <utils/error.h>
#include <utils/log.h>

namespace date_time = boost::gregorian;

namespace approx {
auto logger = utils::create_logger("approx::poisson");

void blend_images_poisson(MultiChannelImage& input_images, MultiChannelImage const& replacement_images, int start_row, int start_column, f64 sentinel_value)
{
    spdlog::stopwatch sw;
    // Sanity checks
    if (replacement_images.size() > input_images.size()) {
        logger->error("Cannot solve problem: replacement image is larger than the input image ({} vs {})", replacement_images.size(), input_images.size());
        return;
    }
    if (start_row < 0 || start_column < 0 || start_row >= input_images.rows() || start_column >= input_images.cols()) {
        logger->error("Cannot solve problem: row/column is out of bounds. Row: {}, Column: {}", start_row, start_column);
        return;
    }
    if (start_row + replacement_images.rows() > input_images.rows() || start_column + replacement_images.cols() > input_images.cols()) {
        logger->error("Cannot solve problem: replacement image goes beyond the bounds of the input image"
                      "({}, {} vs {}, {})",
            start_row + replacement_images.rows(), start_column + replacement_images.cols(), input_images.rows(), input_images.cols());
        return;
    }

    // Coordinate transformations
    auto replacement_to_input = [&](Eigen::Index row, Eigen::Index col) {
        return index_t { row + start_row, col + start_column };
    };

    auto flatten_index = [&](Eigen::Index row, Eigen::Index col) {
        return col + row * replacement_images.cols();
    };

    // Map from the flattened index, into the number of variables
    std::unordered_map<Eigen::Index, int> variable_numbers;
    int i = 0;
    for (Eigen::Index row = 0; row < replacement_images.rows(); ++row) {
        for (Eigen::Index col = 0; col < replacement_images.cols(); ++col) {
            if (replacement_images.valid_pixel(row, col)) {
                variable_numbers.emplace(flatten_index(row, col), i);
                i += 1;
            }
        }
    }
    auto num_unknowns = (int)variable_numbers.size();

    std::vector<triplet_t> triplets;

    int irow = 0;
    int invalid_pixels = 0;
    // The A matrix is generated from the missing area, and does not depend on the specific image we are solving for
    for (Eigen::Index row = 0; row < replacement_images.rows(); ++row) {
        for (Eigen::Index col = 0; col < replacement_images.cols(); ++col) {
            if (!replacement_images.valid_pixel(row, col))
                continue;

            invalid_pixels += 1;
            // First, take care of the left hand side of the equation
            std::vector<index_t> neighbours = valid_neighbours(replacement_images[0], { row, col });
            triplets.emplace_back(irow, variable_numbers.at(flatten_index(row, col)), (f64)neighbours.size());

            for (auto const& [nrow, ncol] : neighbours) {
                // Sum over all the neighbours that are not on the boundary
                if (replacement_images.valid_pixel(nrow, ncol)) {
                    triplets.emplace_back(irow, variable_numbers.at(flatten_index(nrow, ncol)), -1);
                }
            }

            irow += 1;
        }
    }

    logger->debug("Found {} invalid pixels", invalid_pixels);
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
                if (!replacement_images.valid_pixel(row, col))
                    continue;

                std::vector<index_t> neighbours = valid_neighbours(replacement_images[0], { row, col });
                for (auto const& [nrow, ncol] : neighbours) {
                    // This computes the sum of the gradient (difference) for each pixel in the replacement image
                    b(irow) += (replacement_images.images[c](row, col) - replacement_images.images[c](nrow, ncol));

                    // If the neighbours are not part of the mask, they must be on the region boundary.
                    // In that case we will add them to the RHS of the equation, because their values are known
                    if (!replacement_images.valid_pixel(nrow, ncol)) {
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
                // No need to do any modifications if the pixel is not part of the mask
                if (!replacement_images.valid_pixel(row, col))
                    continue;

                index_t index_input = replacement_to_input(row, col);
                input_images.images[c](index_input.row, index_input.col) = solutions[c](variable_numbers.at(flatten_index(row, col)));
            }
        }
    }

    logger->debug("It took {:.2f} seconds to solve the poisson equation", sw);
}

void blend_images_poisson(
    MultiChannelImage& input_images,
    MultiChannelImage const& replacement_images,
    MatX<bool> const& invalid_mask)
{
    spdlog::stopwatch sw;
    // Sanity checks
    if (replacement_images.size() != input_images.size()) {
        logger->error("Cannot solve problem: replacement image is not the same size as input image ({} vs {})", replacement_images.size(), input_images.size());
        return;
    }
    if (input_images.size() != invalid_mask.size()) {
        logger->error("Cannot solve problem: input images and mask are different sizes ({} vs {})", input_images.size(), invalid_mask.size());
    }

    auto flatten_index = [&](Eigen::Index row, Eigen::Index col) {
        return col + row * replacement_images.cols();
    };

    // Map from the flattened index, into the number of variables
    std::unordered_map<Eigen::Index, int> variable_numbers;
    int i = 0;
    for (Eigen::Index row = 0; row < replacement_images.rows(); ++row) {
        for (Eigen::Index col = 0; col < replacement_images.cols(); ++col) {
            if (invalid_mask(row, col)) {
                variable_numbers.emplace(flatten_index(row, col), i);
                i += 1;
            }
        }
    }
    auto num_unknowns = (int)variable_numbers.size();

    std::vector<triplet_t> triplets;
    int irow = 0;
    int invalid_pixels = 0;
    // The A matrix is generated from the missing area, and does not depend on the specific image we are solving for
    for (Eigen::Index row = 0; row < replacement_images.rows(); ++row) {
        for (Eigen::Index col = 0; col < replacement_images.cols(); ++col) {
            if (!invalid_mask(row, col))
                continue;

            invalid_pixels += 1;
            // First, take care of the left hand side of the equation
            std::vector<index_t> neighbours = valid_neighbours(replacement_images[0], { row, col });
            triplets.emplace_back(irow, variable_numbers.at(flatten_index(row, col)), (f64)neighbours.size());

            for (auto const& [nrow, ncol] : neighbours) {
                // Sum over all the neighbours that are not on the boundary
                if (invalid_mask(nrow, ncol)) {
                    triplets.emplace_back(irow, variable_numbers.at(flatten_index(nrow, ncol)), -1);
                }
            }

            irow += 1;
        }
    }

    logger->debug("Found {} invalid pixels", invalid_pixels);
    sparse_t A(num_unknowns, num_unknowns);
    A.setFromTriplets(triplets.begin(), triplets.end());
    cholesky_t chol(A);

    // Solve for each channel of the multi-band image
    std::vector<VecX<f64>> solutions;
    for (size_t c = 0; c < input_images.images.size(); ++c) {
        VecX<f64> b(num_unknowns);
        b.setZero();
        irow = 0;

        for (Eigen::Index row = 0; row < replacement_images.rows(); ++row) {
            for (Eigen::Index col = 0; col < replacement_images.cols(); ++col) {
                if (!invalid_mask(row, col))
                    continue;

                std::vector<index_t> neighbours = valid_neighbours(replacement_images[0], { row, col });
                for (auto const& [nrow, ncol] : neighbours) {
                    // This computes the sum of the gradient (difference) for each pixel in the replacement image
                    b(irow) += (replacement_images.images[c](row, col) - replacement_images.images[c](nrow, ncol));

                    // If the neighbours are not part of the mask, they must be on the region boundary.
                    // In that case we will add them to the RHS of the equation, because their values are known
                    if (!invalid_mask(nrow, ncol)) {
                        b(irow) += input_images(c, nrow, ncol);
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
                // No need to do any modifications if the pixel is not part of the mask
                if (!invalid_mask(row, col))
                    continue;

                input_images.images[c](row, col) = solutions[c](variable_numbers.at(flatten_index(row, col)));
            }
        }
    }

    logger->debug("It took {:.2f} seconds to solve the poisson equation", sw);
}

void highlight_area_replaced(MultiChannelImage& input_images, MultiChannelImage const& replacement_images, int start_row, int start_column, Vec3<f64> const& color)
{
    auto replacement_to_input = [&](Eigen::Index row, Eigen::Index col) {
        return index_t { row + start_row, col + start_column };
    };

    for (Eigen::Index row = 0; row < replacement_images.rows(); ++row) {
        for (Eigen::Index col = 0; col < replacement_images.cols(); ++col) {
            if (replacement_images.valid_pixel(row, col)) {
                index_t input_index = replacement_to_input(row, col);
                input_images(0, input_index.row, input_index.col) = color[0];
                input_images(1, input_index.row, input_index.col) = color[1];
                input_images(2, input_index.row, input_index.col) = color[2];
            }
        }
    }
}

std::string find_good_close_image(std::string const& date_string, bool use_denoised_data, f64 distance_weight, DataBase& db)
{
    if (distance_weight < 0 || distance_weight > 1) {
        throw utils::GenericError("Could not find close image: distance weight not between 0 and 1", *logger);
    }

    auto date = date_time::from_simple_string(date_string);
    auto previous_month = date - date_time::months(1);
    auto next_month = date + date_time::months(1);

    std::vector<DayInfo> info = db.select_close_images(date_string);
    if (info.empty()) {
        logger->warn("Could not find any good images close by. Date: {}", date_time::to_simple_string(date));
        return {};
    }

    std::sort(info.begin(), info.end(), [&](DayInfo const& first, DayInfo const& second) {
        return first.distance(date, distance_weight, use_denoised_data) < second.distance(date, distance_weight, use_denoised_data);
    });

    DayInfo info_for_current_date = db.select_info_about_date(date_string);
    f64 current_invalid = (use_denoised_data ? info_for_current_date.percent_invalid_noise_removed : info_for_current_date.percent_invalid);
    f64 found_invalid = (use_denoised_data ? info[0].percent_invalid_noise_removed : info[0].percent_invalid);
    if (current_invalid < found_invalid) {
        logger->debug("The current date has fewer invalid pixels than the date we found. Use laplace approximation");
        return date_string;
    }

    auto string = date_time::to_iso_extended_string(info[0].date);
    logger->debug("Found image: {} {:.2f}% invalid", string, 100 * info[0].percent_invalid_noise_removed);
    return string;
}
}
