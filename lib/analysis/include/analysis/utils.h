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
struct UseApproximatedData { };
struct UseRealData {
    bool exclude_cloudy_pixels = false;
    bool exclude_shadow_pixels = false;
    std::optional<f64> skip_threshold;
};
using DataChoices = std::variant<UseApproximatedData, UseRealData>;

VecX<f64> selectMatrixElements(MatX<f64> const &matrix, f64 removalValue);
}
