#pragma once

#include <Eigen/Dense>
#include <fmt/format.h>

namespace utils {
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t ;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

template<typename T>
using Vec2 = Eigen::Vector2<T>;
template<typename T>
using Vec3 = Eigen::Vector3<T>;
template<typename T>
using VecX = Eigen::Vector<T, Eigen::Dynamic>;

template<typename T>
using Mat2 = Eigen::Matrix<T, 2, 2>;

template<typename T>
using MatX = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>;

using LatLng = Eigen::Vector2<f64>;
using RGB = Eigen::Vector3<f64>;

template<typename T>
std::string fmt_vector(Vec3<T> const &vec) {
    return fmt::format("({:.3f}, {:.3f}, {:.3f})", vec(0), vec(1), vec(2));
}
}