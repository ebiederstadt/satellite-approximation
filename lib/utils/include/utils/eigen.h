#pragma once

#include <fmt/format.h>
#include <utils/types.h>

namespace utils {
// Calculate the number of pixels that are "valid" in a binary mask
template<typename T>
f64 percent_non_zero(MatX<T> const& matrix)
{
    return static_cast<f64>(matrix.template cast<int>().sum()) / static_cast<f64>(matrix.size());
}

template<typename T>
int count_non_zero(MatX<T> const& matrix)
{
    return static_cast<int>(matrix.template cast<int>().sum());
}

template<typename T>
std::string printable_stats(MatX<T> const& matrix)
{
    return fmt::format("Mean: {}, Max: {}, Min: {}", matrix.mean(), matrix.maxCoeff(), matrix.minCoeff());
}
}
