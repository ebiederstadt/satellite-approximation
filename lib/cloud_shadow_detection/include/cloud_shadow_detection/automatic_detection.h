#pragma once

#include <array>
#include <filesystem>
#include <givde/types.hpp>

#include "db.h"

using namespace givde;
namespace fs = std::filesystem;

namespace remote_sensing {
struct CloudParams {
    fs::path nir_path;
    fs::path clp_path;
    fs::path cld_path;
    fs::path scl_path;
    fs::path rgb_path;
    fs::path view_zenith_path;
    fs::path view_azimuth_path;
    fs::path sun_zenith_path;
    fs::path sun_azimuth_path;

    fs::path cloud_path() const;
    fs::path shadow_potential_path() const;
    fs::path object_based_shadow_path() const;
    fs::path shadow_path() const;
};

struct SkipShadowDetection {
    bool decision = false;
    f64 threshold = 0.0;
};

f32 get_diagonal_distance(f64 min_long, f64 min_lat, f64 max_long, f64 max_lat);
std::optional<Status> detect(CloudParams const& params, f32 diagonal_distance, SkipShadowDetection skipShadowDetection, bool use_cache);
void detect_single_folder(fs::path directory, f32 diagonal_distance, SkipShadowDetection skipShadowDetection, bool use_cache);
void detect_in_folder(fs::path folder_path, f32 diagonal_distance, SkipShadowDetection skipShadowDetection, bool use_cache);

void detect_clouds(fs::path folder, DataBase const &db);
}
