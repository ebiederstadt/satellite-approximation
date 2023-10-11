#pragma once

#include <givde/types.hpp>

using namespace givde;

namespace utils {
// Calculate the number of pixels that are "valid" in a binary mask
template<typename T>
f64 percent_valid(MatX<T> const& matrix)
{
    return static_cast<f64>(matrix.template cast<int>().sum()) / static_cast<f64>(matrix.size());
}
}
