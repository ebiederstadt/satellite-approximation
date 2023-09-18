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

//    void solve_matrix(Mat2<f64> const &input, std::vector<index_t> const &invalid_pixels) {
//        auto [min_x, max_x] = minmax(invalid_pixels | view::transform([](index_t i) { return i.row; }));
//        auto [min_y, max_y] = minmax(invalid_pixels | view::transform([](index_t i) { return i.col; }));
//        auto width = (max_x - min_x) + 1;
//        auto height = (max_y - min_y) + 1;
//
//        auto matrix_size = width * height;
//        spdlog::debug("Matrix size : {}", matrix_size);
//
//        auto index = [&](int x, int y) {
//            return (x - min_x) + (y - min_y) * width;
//        };
//
//        Eigen::VectorXd b(matrix_size);
//        b.setZero();
//
//        std::vector<Triplet<f64>> coefficients;
//        auto set_coefficient = [&](int x, int y, int x_offset, int y_offset, f64 v) {
//            size_t i = index(x, y);
//            int x2 = x + x_offset;
//            int y2 = y + y_offset;
//
//            f64 pixel = input(x2, y2);
//            if (!is_white(pixel)) {
//                // If we know the value, then we move it into the b vector
//                // to keep symmetry with the dirichlet boundary constraint rows.
//                b[i] -= v * pixel;
//                return;
//            }
//            size_t j = index(x2, y2);
//            coefficients.push_back(Triplet<f64>(i, j, v));
//        };
//
//        auto dirichlet_boundary_constraint_row = [&](int x, int y) {
//            // Results in a row with [... 0 0 1 0 0 ... ]
//            // And the corresponding value in the b vector: [ ... 0 0 v 0 0 ...]^T
//            int i = index(x, y);
//            coefficients.push_back(triplet_t(i, i, 1.));
//            b[i] = image.read_pixel(x, y, channel);
//        };
//
//        auto laplacian_row = [&](int x, int y) {
//            // Finite difference to construct laplacian
//            set_coefficient(x, y, -1, 0, 1.);
//            set_coefficient(x, y, +1, 0, 1.);
//            set_coefficient(x, y, 0, -1, 1.);
//            set_coefficient(x, y, 0, +1, 1.);
//            set_coefficient(x, y, 0, 0, -4.);
//        };
//
//        for (auto [x, y]: view::cartesian_product(view::ints(min_x, max_x + 1), view::ints(min_y, max_y + 1))) {
//            pixel_t pixel = image.read_pixel(x, y);
//            if (is_white(pixel)) {
//                laplacian_row(x, y);
//            } else {
//                dirichlet_boundary_constraint_row(x, y);
//            }
//        }
//
//        sparse_t A(matrix_size, matrix_size);
//        A.setFromTriplets(coefficients.begin(), coefficients.end());
//
//        // This will always be a symmetric positive definite system, so we can use cholesky LDLt factorization
//        // See: https://eigen.tuxfamily.org/dox/group__TopicSparseSystems.html
//        cholesky_t chol(A);
//        VecX<f64> values = chol.solve(b);
//
//        // Move the solution values into the image
//        for (auto [x, y]: interior_pixels) {
//            output.set_pixel(x, y, channel, values[index(x, y)]);
//        }
//    }

    bool within_bounds(MatX<bool> const &image, index_t index) {
        return !(index.row < 0 || index.row >= image.rows() || index.col < 0 || index.col >= image.cols());
    }

    std::vector<index_t> valid_neighbours(MatX<bool> const &image, index_t index) {
        std::vector<index_t> retVal = {{-1, 0},
                                       {1,  0},
                                       {0,  -1},
                                       {0,  1}};
        return retVal
               | views::transform([&index](index_t i) {
            return index_t{i.row + index.row, i.col + index.col};
        })
               | view::remove_if([&image](index_t i) { return !within_bounds(image, i); })
               | to<std::vector>();
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

            if (invalid(element.row, element.col)) {
                if (!considered(element.row, element.col)) {
                    connected_pixels.push_back(element);
                }
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
        std::unordered_map<int, index_t> component_index;

        for (Eigen::Index col = 0; col < invalid.cols(); ++col) {
            for (Eigen::Index row = 0; row < invalid.rows(); ++row) {
                if (invalid(row, col) && dataclass(row, col) == 0) {
                    component_index.emplace(highest_class, index_t{row, col});
                    auto connected_pixels = flood(invalid, row, col);
                    for (auto const &pixel: connected_pixels) {
                        dataclass(pixel.row, pixel.col) = highest_class;
                    }
                    highest_class += 1;
                }
            }
        }

        return {dataclass, component_index};
    }

    Mat2<f64> fill_missing_portion_smooth_boundary(MatX<f64> const &input_image, MatX<bool> const &invalid_mask) {
        // 1. Check validity of the input
        if (input_image.size() != invalid_mask.size()) {
            throw std::runtime_error(fmt::format("Input image and mask are not the same size ({} vs {})",
                                                 input_image.size(), invalid_mask.size()));
        }

        // 2. Identify regions of interest with the flood fill algorithm
        auto connected_components = find_connected_components(invalid_mask);

        // 3. For each region, solve the laplace equation
        for (auto const &[region_num, region_start] : connected_components.region_start) {
            solve_matrix(input_image, region_start, region_num);
        }

        // 4. Return the result
    }
}