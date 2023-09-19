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

    bool on_bounds(MatX<bool> const &image, Eigen::Index row, Eigen::Index col) {
        return (row == 0 || row == image.rows() - 1) || (col == 0 || col == image.cols() - 1);
    }

    void solve_matrix(MatX<f64> &input, MatX<bool> const &invalid_mask, std::vector<index_t> const &invalid_pixels) {
        spdlog::debug("Matrix size: {}x{}", invalid_mask.rows(), invalid_mask.cols());

        auto [min_x, max_x] = minmax(invalid_pixels | view::transform([](index_t i) { return i.row; }));
        auto [min_y, max_y] = minmax(invalid_pixels | view::transform([](index_t i) { return i.col; }));
        auto width = (max_x - min_x) + 1;
        auto height = (max_y - min_y) + 1;

        auto matrix_size = width * height;

        auto index = [&](Eigen::Index x, Eigen::Index y) {
            return (x - min_x) + (y - min_y) * width;
        };

        Eigen::VectorXd b(matrix_size);
        b.setZero();

        std::vector<Triplet<f64>> coefficients;

        auto dirichlet_boundary_constraint_row = [&](Eigen::Index x, Eigen::Index y) {
            // Results in a row with [... 0 0 1 0 0 ... ]
            // And the corresponding value in the b vector: [ ... 0 0 v 0 0 ...]^T
            int i = index(x, y);
            coefficients.push_back(Triplet<f64>(i, i, 1.0));
            b[i] = input(x, y);
        };

        auto set_coefficient = [&](Eigen::Index x, Eigen::Index y, int x_offset, int y_offset, f64 v) {
            size_t i = index(x, y);
            Eigen::Index x2 = x + x_offset;
            Eigen::Index y2 = y + y_offset;

            f64 pixel = input(x2, y2);
            if (!invalid_mask(x2, y2)) {
                // If we know the value, then we move it into the b vector
                // to keep symmetry with the dirichlet boundary constraint rows.
                b[i] -= v * pixel;
                return;
            }
            size_t j = index(x2, y2);
            assert(i >= 0 && i < invalid_mask.rows() - 1 && j >= 0 && j < invalid_mask.cols() - 1);
            coefficients.push_back(Triplet<f64>(i, j, v));
        };

        auto laplacian_row = [&](Eigen::Index x, Eigen::Index y) {
            // Finite difference to construct laplacian
            set_coefficient(x, y, -1, 0, 1.0);
            set_coefficient(x, y, +1, 0, 1.0);
            set_coefficient(x, y, 0, -1, 1.0);
            set_coefficient(x, y, 0, +1, 1.0);
            set_coefficient(x, y, 0, 0, -4.0);
        };

        for (auto [x, y]: view::cartesian_product(view::ints(min_x, max_x + 1), view::ints(min_y, max_y + 1))) {
            if (on_bounds(invalid_mask, x, y)) {
                dirichlet_boundary_constraint_row(x, y);
            } else if (invalid_mask(x, y)) {
                laplacian_row(x, y);
            } else {
                dirichlet_boundary_constraint_row(x, y);
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

        auto connected_components = find_connected_components(invalid_pixels);

        for (auto const &[region_num, pixels_in_region]: connected_components.region_map) {
            solve_matrix(input_image, invalid_pixels, pixels_in_region);
        }
    }
}