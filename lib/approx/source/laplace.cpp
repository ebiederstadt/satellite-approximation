#include "approx/laplace.h"
#include "approx/db.h"
#include "utils/eigen.h"
#include "utils/filesystem.h"
#include "utils/geotiff.h"
#include "utils/log.h"

#include <execution>
#include <iostream>
#include <queue>

#include <Eigen/Sparse>
#include <range/v3/all.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <opencv2/core/eigen.hpp>

using namespace ranges;

namespace approx {
static auto logger = utils::create_logger("approx::laplace");

bool on_border(Eigen::Index row, Eigen::Index col, MatX<f64> const& image)
{
    bool row_border = row == 0 || row == image.rows() - 1;
    bool col_border = col == 0 || col == image.cols() - 1;

    return row_border || col_border;
}

void solve_matrix(MatX<f64>& input, MatX<bool> const& invalid_mask)
{
    std::vector<index_t> invalid_pixels;
    for (Eigen::Index row = 0; row < invalid_mask.rows(); ++row) {
        for (Eigen::Index col = 0; col < invalid_mask.cols(); ++col) {
            if (invalid_mask(row, col)) {
                invalid_pixels.push_back({ row, col });
            }
        }
    }
    if (invalid_pixels.empty()) {
        logger->info("Could not perform approximation: no invalid pixels");
        return;
    }

    auto [min_row, max_row] = minmax(invalid_pixels | views::transform([](index_t i) { return i.row; }));
    auto [min_col, max_col] = minmax(invalid_pixels | views::transform([](index_t i) { return i.col; }));

    auto height = (max_row - min_row) + 1;
    auto width = (max_col - min_col) + 1;

    auto matrix_size = height * width;

    auto index = [&](Eigen::Index row, Eigen::Index col) {
        return (col - min_col) + (row - min_row) * width;
    };

    Eigen::VectorXd b(matrix_size);
    b.setZero();

    std::vector<triplet_t> coefficients;

    auto dirichlet_boundary_constraint_row = [&](Eigen::Index row, Eigen::Index col) {
        // Results in a row with [... 0 0 1 0 0 ... ]
        // And the corresponding value in the b vector: [ ... 0 0 v 0 0 ...]^T
        auto i = index(row, col); // This is producing a value that is out of bounds... I wonder why?
        coefficients.emplace_back(i, i, 1.0);
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
        coefficients.emplace_back(i, j, v);
    };

    auto laplacian_row = [&](Eigen::Index row, Eigen::Index col) {
        // Finite difference to construct laplacian
        set_coefficient(row, col, -1, 0, 1.0);
        set_coefficient(row, col, +1, 0, 1.0);
        set_coefficient(row, col, 0, -1, 1.0);
        set_coefficient(row, col, 0, +1, 1.0);
        set_coefficient(row, col, 0, 0, -4.0);
    };

    for (auto [row, col] : views::cartesian_product(
             views::ints(min_row, max_row + 1), views::ints(min_col, max_col + 1))) {
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
    for (auto [row, col] : invalid_pixels) {
        input(row, col) = values[index(row, col)];
    }
}

void fill_missing_portion_smooth_boundary(MatX<f64>& input_image, MatX<bool> const& invalid_pixels)
{
    if (input_image.size() != invalid_pixels.size()) {
        throw std::runtime_error(fmt::format("Input image and mask are not the same size ({} vs {})",
            input_image.size(), invalid_pixels.size()));
    }

    spdlog::stopwatch sw;
    solve_matrix(input_image, invalid_pixels);
    logger->debug("It took {} seconds to solve the problem", sw);
}

cv::Mat apply_laplace(cv::Mat const &image, cv::Mat const &invalid_image, f64 red_threshold)
{
    std::vector<cv::Mat> channels_cv;
    cv::split(invalid_image, channels_cv);
    logger->debug("Laplace: found {} channels", channels_cv.size());


    MatX<f64> red_matrix;
    MatX<f64> green_matrix;
    cv::cv2eigen(channels_cv[2], red_matrix);
    cv::cv2eigen(channels_cv[1], green_matrix);

    MatX<bool> invalid_pixels = ((red_matrix.array() >= red_threshold).array() && (green_matrix.array() <= 150));
    logger->debug("Found {} pixels to replace", utils::count_non_zero(invalid_pixels));

    channels_cv.clear();
    cv::split(image, channels_cv);

    std::vector<cv::Mat> output;
    for (auto const & channel : channels_cv) {
        MatX<f64> mat_eigen;
        cv::cv2eigen(channel, mat_eigen);

        fill_missing_portion_smooth_boundary(mat_eigen, invalid_pixels);

        cv::Mat output_mat;
        cv::eigen2cv(mat_eigen, output_mat);
        output.push_back(output_mat);
    }

    cv::Mat final_matrix;
    cv::merge(output, final_matrix);

    return final_matrix;
}

//void fill_missing_data_folder(fs::path base_folder, std::vector<std::string> band_names, bool use_cache, f64 skip_threshold)
//{
//    logger->debug("Processing directory: {}", base_folder);
//    if (!is_directory(base_folder)) {
//        logger->warn("Could not process: base folder is not a directory ({})", base_folder);
//        return;
//    }
//
//    DataBase db(base_folder);
//
//    std::vector<fs::path> folders_to_process;
//    for (auto const& path : fs::directory_iterator(base_folder)) {
//        if (utils::find_directory_contents(path) == utils::DirectoryContents::MultiSpectral) {
//            folders_to_process.push_back(path);
//        }
//    }
//
//    std::mutex mutex;
//    std::for_each(folders_to_process.begin(), folders_to_process.end(), [&](auto&& folder) {
//        logger->debug("Starting folder: {}", folder);
//        fs::path output_dir = folder / fs::path("approximated_data");
//        if (!fs::exists(output_dir)) {
//            logger->info("Creating directory: {}", output_dir);
//            fs::create_directory(output_dir);
//        }
//
//        utils::CloudShadowStatus status;
//        {
//            std::lock_guard<std::mutex> lock(mutex);
//            status = db.get_status(folder.filename().string());
//        }
//        if (!(status.clouds_exist && status.shadows_exist)) {
//            logger->warn("Both clouds and shadows don't exist for folder {}. Skipping", folder);
//            return;
//        }
//        if (status.percent_invalid > skip_threshold) {
//            logger->info("Skipping {} because there is too little valid data ({:.1f}% invalid)", folder, status.percent_invalid * 100.0);
//            return;
//        }
//
//        MatX<bool> clouds;
//        MatX<bool> shadows;
//        utils::GeoTIFF<u8> cloud_tiff;
//        if (status.clouds_exist) {
//            clouds = utils::GeoTIFF<u8>(folder / "cloud_mask.tif").values.cast<bool>();
//        }
//        if (status.shadows_exist) {
//            shadows = utils::GeoTIFF<u8>(folder / "shadow_mask.tif").values.cast<bool>();
//        } else {
//            shadows.setZero(clouds.rows(), clouds.cols());
//        }
//        MatX<bool> mask = clouds.array() || shadows.array();
//
//        std::unordered_map<std::string, int> existing_data;
//        {
//            std::lock_guard<std::mutex> lock(mutex);
//            existing_data = db.get_approx_status(folder.filename().string(), ApproxMethod::Laplace);
//        }
//        for (auto const& band : band_names) {
//            if (use_cache && existing_data.contains(band)) {
//                continue;
//            }
//
//            fs::path input_path = folder / fs::path(fmt::format("{}.tif", band));
//            utils::GeoTIFF<f64> values(input_path);
//            fill_missing_portion_smooth_boundary(values.values, mask);
//
//            std::lock_guard<std::mutex> lock(mutex);
//            int id = db.write_approx_results(folder.filename().string(), band, ApproxMethod::Laplace);
////            values.write(output_dir / fs::path(fmt::format("{}_{}.tif", band, id)));
//        }
//
//        logger->info("Finished folder: {}", folder);
//    });
//}
}