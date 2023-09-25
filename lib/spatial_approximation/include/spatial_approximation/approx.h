#pragma once

#include <filesystem>
#include <givde/types.hpp>

using namespace givde;
namespace fs = std::filesystem;

namespace spatial_approximation {
struct index_t {
    Eigen::Index row;
    Eigen::Index col;

    bool operator==(index_t other) const
    {
        return row == other.row && col == other.col;
    }
};

std::vector<index_t> flood(MatX<bool> const& invalid, Eigen::Index row, Eigen::Index col);
std::vector<index_t> valid_neighbours(MatX<bool> const& image, index_t index);

struct ConnectedComponents {
    MatX<int> matrix;
    std::unordered_map<int, std::vector<index_t>> region_map;
};

/**
 * Find the portions of an image that are connected to each other
 * @param image: a binary image with true == pixel is invalid
 */
ConnectedComponents find_connected_components(MatX<bool> const& invalid);

/** Fill a missing region of an image, assuming that boundary of the image is smooth
 * (Laplace equation with dirichlet boundary conditions)
 * @param input_image: The input image
 * @param invalid_mask: A mask identifying what portions of the image are invalid
 * @returns: A matrix representing approximation status for each region
 */
void fill_missing_portion_smooth_boundary(MatX<f64>& input_image, MatX<bool> const& invalid_pixels);

struct Status {
    f64 percent_clouds = 0.0;
    std::optional<f64> percent_shadows;
    bool clouds_computed = false;
    bool shadows_computed = false;
    std::vector<std::string> bands_computed;
};
void fill_missing_data_folder(fs::path folder, std::vector<std::string> band_names, bool use_cache, f64 skip_threshold);
}