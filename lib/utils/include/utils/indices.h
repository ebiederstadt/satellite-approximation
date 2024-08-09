#pragma once

#include "geotiff.h"

#include <filesystem>
#include <optional>

namespace fs = std::filesystem;

namespace utils {
template<typename T, typename U>
bool contains(std::vector<T> const& vec, U const& item)
{
    return std::find(vec.begin(), vec.end(), static_cast<T>(item)) != vec.end();
}

enum class Indices {
    NDVI,
    NDMI,
    mNDWI,
    SWI
};
std::optional<Indices> from_str(std::string_view str);

std::vector<std::string> required_files(Indices index);
bool missing_files(std::vector<std::string> const& files, Indices index);

utils::GeoTIFF<f64> compute_index(fs::path const& folder, fs::path const& template_path, Indices index, bool use_cache = true);
}