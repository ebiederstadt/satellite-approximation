#include "approx/poisson.h"
#include "approx/utils.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <execution>
#include <spdlog/stopwatch.h>
#include <utils/error.h>
#include <utils/filesystem.h>
#include <utils/geotiff.h>
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

void fill_missing_data_folder(fs::path base_folder, std::vector<std::string> band_names, bool use_cache, bool use_denoised_data, f64 distance_weight, f64 skip_threshold)
{
    logger->debug("Processing directory: {}", base_folder);
    if (!is_directory(base_folder)) {
        logger->warn("Could not process: base folder is not a directory ({})", base_folder);
        return;
    }

    DataBase db(base_folder);

    std::vector<fs::path> folders_to_process;
    for (auto const& path : fs::directory_iterator(base_folder)) {
        if (utils::find_directory_contents(path) == utils::DirectoryContents::MultiSpectral) {
            folders_to_process.push_back(path);
        }
    }

    std::for_each(folders_to_process.begin(), folders_to_process.end(), [&](auto&& folder) {
        logger->debug("Starting folder: {}", folder);
        fs::path output_dir = folder / fs::path("approximated_data");
        if (!fs::exists(output_dir)) {
            logger->info("Creating directory: {}", output_dir);
            fs::create_directory(output_dir);
        }

        utils::CloudShadowStatus status = db.get_status(folder.filename().string());
        if (!(status.clouds_exist && status.shadows_exist)) {
            logger->warn("Both clouds and shadows don't exist for folder {}. Skipping", folder);
            return;
        }
        f64 invalid_percent = (use_denoised_data ? status.percent_invalid_denoised : status.percent_invalid);
        if (invalid_percent > skip_threshold) {
            logger->info("Skipping {} because there is too little valid data ({:.1f}% invalid)", folder, status.percent_invalid * 100.0);
            return;
        }

        MatX<bool> mask;
        if (use_denoised_data) {
            mask = utils::GeoTIFF<u8>(folder / "cloud_shadows_noise_removed.tif").values.cast<bool>();
        } else {
            MatX<bool> clouds;
            MatX<bool> shadows;
            utils::GeoTIFF<u8> cloud_tiff;
            if (status.clouds_exist) {
                clouds = utils::GeoTIFF<u8>(folder / "cloud_mask.tif").values.cast<bool>();
            }
            if (status.shadows_exist) {
                shadows = utils::GeoTIFF<u8>(folder / "shadow_mask.tif").values.cast<bool>();
            } else {
                shadows.setZero(clouds.rows(), clouds.cols());
            }
            mask = clouds.array() || shadows.array();
        }

        std::unordered_map<std::string, int> existing_data = db.get_approx_status(folder.filename().string(), ApproxMethod::Poisson, use_denoised_data);

        if (use_cache) {
            std::vector<bool> data_exists;
            for (auto const& band : band_names) {
                bool result = existing_data.contains(band);
                data_exists.push_back(result);
            }
            if (std::all_of(data_exists.begin(), data_exists.end(), [](bool v) { return v; })) {
                logger->debug("Skipping folder because all data already exists");
                return;
            }
        }

        approx::MultiChannelImage input_images {};
        approx::MultiChannelImage replacement_images {};
        std::vector<std::string> output_band_names;

        for (auto const& band : band_names) {
            if (use_cache && existing_data.contains(band)) {
                continue;
            }
            fs::path tiff_path = base_folder / fmt::format("{}.tif", band);
            utils::GeoTIFF<f64> tiff(tiff_path);
            input_images.images.push_back(tiff.values);
            output_band_names.push_back(band);
        }

        // TODO: Take care of the case where we can't find any better images.
        // In that case, we should use laplace approximation
        std::string folder_with_good_image = find_good_close_image(folder.filename().string(), use_denoised_data, distance_weight, db);
        fs::path replacement_path = base_folder / folder_with_good_image;
        for (auto const& band : band_names) {
            if (use_cache && existing_data.contains(band)) {
                continue;
            }

            fs::path tiff_path = replacement_path / fmt::format("{}.tif", band);
            utils::GeoTIFF<f64> tiff(tiff_path);
            replacement_images.images.push_back(tiff.values);
        }

        approx::blend_images_poisson(input_images, replacement_images, mask);
        utils::GeoTIFF<f64> template_tiff(folder / "viewZenithMean.tif");
        for (size_t i = 0; i < input_images.images.size(); ++i) {
            int id = db.write_approx_results(folder.filename().string(), output_band_names[i], ApproxMethod::Poisson, use_denoised_data);
            template_tiff.values = input_images.images[i];
            template_tiff.write(output_dir / fs::path(fmt::format("{}_{}.tif", output_band_names[i], id)));
        }

        logger->info("Finished folder: {}", folder);
    });
}
}
