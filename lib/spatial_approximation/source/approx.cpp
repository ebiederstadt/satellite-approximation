#include "spatial_approximation/approx.h"
#include "utils/fmt_filesystem.h"
#include "utils/geotiff.h"
#include "utils/filesystem.h"

#include <queue>
#include <iostream>
#include <execution>

#include <range/v3/all.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <Eigen/Sparse>

using namespace ranges;

namespace spatial_approximation {
    template<typename T>
    using Triplet = Eigen::Triplet<T>;
    using sparse_t = Eigen::SparseMatrix<f64>;
    using cholesky_t = Eigen::SimplicialCholesky<sparse_t>;

    bool on_border(Eigen::Index row, Eigen::Index col, MatX<f64> const &image) {
        bool row_border = row == 0 || row == image.rows() - 1;
        bool col_border = col == 0 || col == image.cols() - 1;

        return row_border || col_border;
    }

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
            return (col - min_col) + (row - min_row) * width;
        };

        Eigen::VectorXd b(matrix_size);
        b.setZero();

        std::vector<Triplet<f64>> coefficients;

        auto dirichlet_boundary_constraint_row = [&](Eigen::Index row, Eigen::Index col) {
            // Results in a row with [... 0 0 1 0 0 ... ]
            // And the corresponding value in the b vector: [ ... 0 0 v 0 0 ...]^T
            auto i = index(row, col); // This is producing a value that is out of bounds... I wonder why?
            coefficients.push_back(Triplet<f64>(i, i, 1.0));
            b[i] = input(row, col);
        };

        auto set_coefficient = [&](Eigen::Index row, Eigen::Index col, int row_offset, int col_offset, f64 v) {
            auto i = index(row, col);
            Eigen::Index row2 = row + row_offset;
            Eigen::Index col2 = col + col_offset;

            f64 pixel = input(row2, col2);
            if (!invalid_mask(row2, col2)) {
                // If we know the value, then we move it into the b vector
                // to keep symmetry with the dirichlet boundary constraint rows.
                b[i] -= v * pixel;
                return;
            }
            auto j = index(row2, col2);
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
            // If we are on the border, we assume that the pixel is "known" (even though they may or may not actually be known)
            if (on_border(row, col, input)) {
                dirichlet_boundary_constraint_row(row, col);
            } else if (invalid_mask(row, col)) {
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

        spdlog::stopwatch sw;
        solve_matrix(input_image, invalid_pixels);
        spdlog::debug("It took {} seconds to solve the problem", sw);
    }

    std::unordered_map<std::string, Status> fill_missing_data_folder(fs::path base_folder, std::vector<std::string> band_names, bool use_cache, f64 skip_threshold) {
        spdlog::debug("Processing directory: {}", base_folder);
        if (!is_directory(base_folder)) {
            spdlog::warn("Could not process: base folder is not a directory ({})", base_folder);
            return {};
        }

        std::unordered_map<std::string, Status> return_status;

        std::vector<fs::path> folders_to_process;
        for (auto const &path: fs::directory_iterator(base_folder)) {
            if (utils::find_directory_contents(path) == utils::DirectoryContents::MultiSpectral) {
                folders_to_process.push_back(path);
            }
        }

        std::mutex mutex;
        std::for_each(std::execution::par_unseq, folders_to_process.begin(), folders_to_process.end(), [&](auto &&folder) {
            spdlog::debug("Starting folder: {}", folder);
            fs::path output_dir = folder / fs::path("approximated_data");
            if (!fs::exists(output_dir)) {
                spdlog::info("Creating directory: {}", output_dir);
                fs::create_directory(output_dir);
            }

            bool found_cloud_data = false;
            bool found_shadow_data = false;
            utils::GeoTIFF<u16> cloud_tiff;
            utils::GeoTIFF<u16> shadow_tiff;
            if (fs::exists(folder / fs::path("cloud_mask.tif"))) {
                try {
                    cloud_tiff = utils::GeoTIFF<u16>(folder / fs::path("cloud_mask.tif"));
                    found_cloud_data = true;
                } catch (std::runtime_error const &e) {
                    spdlog::warn("Failed to open cloud file. Failed with error: {}", e.what());
                }
            }
            if (fs::exists(folder / fs::path("shadow_mask.tif"))) {
                try {
                    shadow_tiff = utils::GeoTIFF<u16>(folder / fs::path("shadow_mask.tif"));
                    found_shadow_data = true;
                } catch (std::runtime_error const &e) {
                    spdlog::warn("Failed to open shadow file. Failed with error: {}", e.what());
                }
            }
            if (!(found_cloud_data || found_shadow_data)) {
                spdlog::warn("Could not find mask data. Skipping dir: {}", folder);
                return;
            }

            Status status;
            status.band_computation_status = {};

            if (shadow_tiff.values.size() == 0) {
                shadow_tiff.values = MatX<u16>::Zero(cloud_tiff.values.rows(), cloud_tiff.values.cols());
            }
            MatX<bool> mask = cloud_tiff.values.cast<bool>().array() || shadow_tiff.values.cast<bool>().array();
            status.percent_clouds = static_cast<f64>(cloud_tiff.values.cast<int>().sum() / static_cast<f64>(cloud_tiff.values.size()));
            if (found_shadow_data) {
                status.percent_shadows = static_cast<f64>(shadow_tiff.values.cast<int>().sum() / static_cast<f64>(shadow_tiff.values.size()));
            }
            f64 percent_invalid = static_cast<f64>(mask.cast<int>().sum()) / static_cast<f64>(mask.size());
            spdlog::debug("Percent valid pixels: {:.2f}", (1.0 - percent_invalid) * 100.0);
            if (percent_invalid >= skip_threshold) {
                spdlog::info("Skipping {} because there is too little valid data ({:.1f}% invalid)", folder, percent_invalid * 100.0);
                return;
            }
            for (auto const &band: band_names) {
                fs::path output_path = output_dir / fs::path(fmt::format("{}.tif", band));
                if (use_cache && fs::exists(output_path)) {
                    status.band_computation_status.emplace_back(band, true);
                    continue;
                }

                fs::path input_path = folder / fs::path(fmt::format("{}.tif", band));
                utils::GeoTIFF<f64> values(input_path);
                fill_missing_portion_smooth_boundary(values.values, mask);
                values.write(output_dir / fs::path(fmt::format("{}.tif", band)));
                status.band_computation_status.emplace_back(band, true);
            }

            std::lock_guard<std::mutex> lock(mutex);
            return_status.insert({folder.filename().string(), status});

            spdlog::debug("Finished folder: {}", folder);
        });

        return return_status;
    }
}