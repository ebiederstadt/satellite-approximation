#pragma once

#include <Eigen/Sparse>
#include <givde/types.hpp>
#include <range/v3/all.hpp>

using namespace givde;

namespace approx {
using triplet_t = Eigen::Triplet<f64>;
using sparse_t = Eigen::SparseMatrix<f64>;
using cholesky_t = Eigen::SimplicialCholesky<sparse_t>;

struct index_t {
    Eigen::Index row;
    Eigen::Index col;

    bool operator==(index_t other) const
    {
        return row == other.row && col == other.col;
    }
};

template<typename T>
bool within_bounds(MatX<T> const& image, index_t index)
{
    return !(index.row < 0 || index.row >= image.rows() || index.col < 0 || index.col >= image.cols());
}

template<typename T>
std::vector<index_t> valid_neighbours(MatX<T> const& image, index_t index)
{
    // clang-format off
    std::vector<index_t> retVal = {
        { -1, 0 },
        { 1,  0 },
        { 0, -1 },
        { 0,  1 }
    };
    // clang-format on
    return retVal
        | ranges::views::transform([&index](index_t i) { return index_t { i.row + index.row, i.col + index.col }; })
        | ranges::view::remove_if([&image](index_t i) { return !within_bounds(image, i); })
        | ranges::to<std::vector>();
}
}
