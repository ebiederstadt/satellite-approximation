#pragma once

#include <filesystem>
#include <givde/types.hpp>
#include <variant>

namespace fs = std::filesystem;
using namespace givde;

// Single Image summary - integral analysis
namespace analysis {
enum class Indices {
    NDVI,
    NDMI,
    mNDWI,
    SWI
};
std::optional<Indices> from_str(std::string_view str);

// How to handle missing data from clouds and shadows
struct UseApproximatedData { };
struct UseRealData {
    bool exclude_cloudy_pixels = false;
    bool exclude_shadow_pixels = false;
    std::optional<f64> skip_threshold;
};
using DataChoices = std::variant<UseApproximatedData, UseRealData>;

constexpr f64 NO_DATA_INDICATOR = -500;

std::filesystem::path cache_string(int start_year, int end_year, Indices index, f64 threshold, DataChoices choices);
void single_image_summary(
    std::filesystem::path const& base_path,
    bool use_cache,
    int start_year,
    int end_year,
    Indices index,
    f64 threshold,
    DataChoices choices);
}