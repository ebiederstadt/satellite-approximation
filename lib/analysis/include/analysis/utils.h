#pragma once

#include <filesystem>
#include <givde/types.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <utils/geotiff.h>

namespace fs = std::filesystem;
using namespace givde;

template<class... Ts>
struct Visitor : Ts... {
    using Ts::operator()...;
};

namespace analysis {
template<typename T, typename U>
bool contains(std::vector<T> const& vec, U const& item)
{
    return std::find(vec.begin(), vec.end(), static_cast<T>(item)) != vec.end();
}

struct UseApproximatedData { };
struct UseRealData {
    bool exclude_cloudy_pixels = false;
    bool exclude_shadow_pixels = false;
    std::optional<f64> skip_threshold;
};
using DataChoices = std::variant<UseApproximatedData, UseRealData>;

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

VecX<f64> selectMatrixElements(MatX<f64> const &matrix, f64 removalValue);
}
