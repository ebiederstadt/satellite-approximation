#pragma once

#include <filesystem>
#include <array>

#include <cloud_shadow_detection/types.h>

namespace fs = std::filesystem;

namespace remote_sensing {
    struct CloudParams {
        std::array<f64, 4> bbox;
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
        f64 threshold;
    };

    f32 get_diagonal_distance(std::array<f64, 4> const &bbox);
    void detect(CloudParams const &params, f32 diagonal_distance, SkipShadowDetection skipShadowDetection, bool use_cache);
}
