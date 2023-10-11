#pragma once

#include "db.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace analysis {
/**
 * Remove noise in an image by removing areas that are smaller than a certain threshold.
 * Warning: This function is not thread safe!
 * @param folder: The folder which contains the images of interest
 * @param min_region_size: The minimum number of pixels that should be used for the region to count as a valid region.
 * @param use_cache: If a result exists already, do not re-compute
 * @param db: The database for checking the status of results
 */
void remove_noise_in_clouds_and_shadows(fs::path folder, int min_region_size, bool use_cache, DataBase& db);
void remove_noise_folder(fs::path base_folder, bool use_cache);
}
