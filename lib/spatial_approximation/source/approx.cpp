#include "spatial_approximation/approx.h"

#include <queue>

#include <range/v3/all.hpp>
#include <spdlog/spdlog.h>
#include <Eigen/Sparse>
#include <iostream>

using namespace ranges;

namespace spatial_approximation {
    template<typename T>
    using Triplet = Eigen::Triplet<T>;
    using sparse_t = Eigen::SparseMatrix<f64>;
    using cholesky_t = Eigen::SimplicialCholesky<sparse_t>;

    void solve_matrix(MatX<f64> &input, MatX<bool> const &invalid_mask) {
        std::vector<index_t> invalid_pixels;
        for (Eigen::Index row = 0; row < invalid_mask.rows(); ++row) {
            for (Eigen::Index col = 0; col < invalid_mask.cols(); ++col) {
                if (invalid_mask(row, col)) {
                    invalid_pixels.push_back({row, col});
                }
            }
        }
        if (invalid_pixels.empty()) {
            spdlog::info("Could not perform approximation: no invalid pixels");
            return;
        }

        auto [min_row, max_row] = minmax(invalid_pixels | view::transform([](index_t i) { return i.row; }));
        auto [min_col, max_col] = minmax(invalid_pixels | view::transform([](index_t i) { return i.col; }));

        auto height = (max_row - min_row) + 1;
        auto width = (max_col - min_col) + 1;

        auto matrix_size = height * width;

        auto index = [&](Eigen::Index row, Eigen::Index col) {
            return (col - min_col) + (row - min_row) * height;
        };

        Eigen::VectorXd b(matrix_size);
        b.setZero();

        std::vector<Triplet<f64>> coefficients;

        auto dirichlet_boundary_constraint_row = [&](Eigen::Index row, Eigen::Index col) {
            // Results in a row with [... 0 0 1 0 0 ... ]
            // And the corresponding value in the b vector: [ ... 0 0 v 0 0 ...]^T
            auto i = index(row, col);
            coefficients.push_back(Triplet<f64>(i, i, 1.0));
            b[i] = input(row, col);
        };

        auto set_coefficient = [&](Eigen::Index row, Eigen::Index col, int x_offset, int y_offset, f64 v) {
            auto i = index(row, col);
            Eigen::Index row2 = row + x_offset;
            Eigen::Index col2 = col + y_offset;

            f64 pixel = input(row2, col2);
            if (!invalid_mask(row2, col2)) {
                // If we know the value, then we move it into the b vector
                // to keep symmetry with the dirichlet boundary constraint rows.
                b[i] -= v * pixel;
                return;
            }
            auto j = index(row2, col2);
            if (!(i < matrix_size && j < matrix_size) || !(i >= 0 && j >= 0)) {
                throw std::runtime_error(fmt::format("Failed to compute, i or j too large ({} and {}, matrix_size={})", i, col, matrix_size));
            }
            coefficients.push_back(Triplet<f64>(i, j, v));
        };

        // Problem comes from here!
        auto laplacian_row = [&](Eigen::Index row, Eigen::Index col) {
            // Finite difference to construct laplacian
            set_coefficient(row, col, -1, 0, 1.0);
            set_coefficient(row, col, +1, 0, 1.0);
            set_coefficient(row, col, 0, -1, 1.0);
            set_coefficient(row, col, 0, +1, 1.0);
            set_coefficient(row, col, 0, 0, -4.0);
        };

        for (auto [row, col]: view::cartesian_product(
                view::ints(min_row, max_row + 1), view::ints(min_col, max_col + 1))) {
           if (invalid_mask(row, col)) {
                laplacian_row(row, col);
            } else {
                dirichlet_boundary_constraint_row(row, col);
            }
        }

        sparse_t A(matrix_size, matrix_size);
        A.setFromTriplets(coefficients.begin(), coefficients.end());

        // This will always be a symmetric positive definite system, so we can use cholesky LDLt factorization
        // See: https://eigen.tuxfamily.org/dox/group__TopicSparseSystems.html
        cholesky_t chol(A);
        VecX<f64> values = chol.solve(b);

        // Move the solution values into the image
        for (auto [row, col]: invalid_pixels) {
            input(row, col) = values[index(row, col)];
        }
    }

    bool within_bounds(MatX<bool> const &image, index_t index) {
        return !(index.row < 0 || index.row >= image.rows() || index.col < 0 || index.col >= image.cols());
    }

    std::vector<index_t> valid_neighbours(MatX<bool> const &image, index_t index) {
        std::vector<index_t> retVal = {{-1, 0},
                                       {1,  0},
                                       {0,  -1},
                                       {0,  1}};
        // clang-format off
        return retVal
           | views::transform([&index](index_t i) { return index_t{i.row + index.row, i.col + index.col}; })
           | view::remove_if([&image](index_t i) { return !within_bounds(image, i); })
           | to<std::vector>();
        // clang-format on
    }

    std::vector<index_t> flood(MatX<bool> const &invalid, Eigen::Index row, Eigen::Index col) {
        std::queue<index_t> queue;
        queue.push(index_t{row, col});
        std::vector<index_t> connected_pixels;

        MatX<bool> considered(invalid.rows(), invalid.cols());
        considered.setConstant(false);

        while (!queue.empty()) {
            auto element = queue.front();
            queue.pop();

            // Continue to process this pixel only if it is invalid, and it has not yet been considered, otherwise
            // we are duplicating work. (Makes a massive difference for larger datasets)
            if (invalid(element.row, element.col) && !considered(element.row, element.col)) {
                connected_pixels.push_back(element);
                auto neighbours = valid_neighbours(invalid, element);
                considered(element.row, element.col) = true;
                for (auto const &neighbour: neighbours) {
                    if (!considered(neighbour.row, neighbour.col)) {
                        queue.push(neighbour);
                    }
                }
            }
        }

        return connected_pixels;
    }

    ConnectedComponents find_connected_components(MatX<bool> const &invalid) {
        MatX<int> dataclass(invalid.rows(), invalid.cols());
        dataclass.fill(0);
        int highest_class = 1;
        std::unordered_map<int, std::vector<index_t>> component_index;

        for (Eigen::Index col = 0; col < invalid.cols(); ++col) {
            for (Eigen::Index row = 0; row < invalid.rows(); ++row) {
                // If a Pixel is invalid and does not already belong to a dataclass, then assign it a dataclass using the flood fill algorithm
                if (invalid(row, col) && dataclass(row, col) == 0) {
                    auto connected_pixels = flood(invalid, row, col);
                    for (auto const &pixel: connected_pixels) {
                        dataclass(pixel.row, pixel.col) = highest_class;
                    }
                    component_index.emplace(highest_class, connected_pixels);
                    highest_class += 1;
                }
            }
        }

        return {dataclass, component_index};
    }

    void fill_missing_portion_smooth_boundary(MatX<f64> &input_image, MatX<bool> const &invalid_pixels) {
        if (input_image.size() != invalid_pixels.size()) {
            throw std::runtime_error(fmt::format("Input image and mask are not the same size ({} vs {})",
                                                 input_image.size(), invalid_pixels.size()));
        }

        solve_matrix(input_image, invalid_pixels);
    }
}