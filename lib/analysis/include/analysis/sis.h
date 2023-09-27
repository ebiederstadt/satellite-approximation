#pragma once

#include <filesystem>
#include <givde/types.hpp>
#include <variant>

#include "forward.h"

namespace fs = std::filesystem;
using namespace givde;

// Single Image summary - integral analysis
namespace analysis {
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