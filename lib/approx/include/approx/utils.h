#pragma once

#include <Eigen/Sparse>
#include <filesystem>
#include <opencv2/core/mat.hpp>
#include <optional>
#include <range/v3/all.hpp>

#include "utils/types.h"

namespace fs = std::filesystem;
using namespace utils;

namespace approx {
using triplet_t = Eigen::Triplet<f64>;
using sparse_t = Eigen::SparseMatrix<f64>;
using SparseSolver = Eigen::ConjugateGradient<sparse_t, Eigen::Lower | Eigen::Upper>;

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
    return index.row >= 0 && index.row < image.rows() && index.col >= 0 && index.col < image.cols();
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
        | ranges::views::remove_if([&image](index_t i) { return !within_bounds(image, i); })
        | ranges::to<std::vector>();
}

struct MultiChannelImage {
    explicit MultiChannelImage(std::vector<MatX<f64>> images)
        : images(std::move(images))
    {
    }
    MultiChannelImage(std::initializer_list<MatX<f64>> images)
        : images(images)
    {
    }
    MultiChannelImage(size_t channels, Eigen::Index rows, Eigen::Index cols);
    MultiChannelImage() = default;

    std::vector<MatX<f64>> images;

    f64 const& operator()(size_t c, Eigen::Index row, Eigen::Index col) const
    {
        return images.at(c)(row, col);
    }

    f64& operator()(size_t c, Eigen::Index row, Eigen::Index col)
    {
        return images.at(c)(row, col);
    }

    MatX<f64> const& operator[](size_t c) const
    {
        return images[c];
    }

    MatX<f64>& operator[](size_t c)
    {
        return images[c];
    }

    [[nodiscard]] Eigen::Index size() const
    {
        return images[0].size();
    }

    [[nodiscard]] Eigen::Index rows() const
    {
        return images[0].rows();
    }

    [[nodiscard]] Eigen::Index cols() const
    {
        return images[0].cols();
    }

    [[nodiscard]] bool valid_pixel(Eigen::Index row, Eigen::Index col) const
    {
        bool invalid = static_cast<int>(images[0](row, col)) == 1 && static_cast<int>(images[1](row, col)) == 1 && static_cast<int>(images[2](row, col)) == 1;
        return !invalid;
    }
};

MultiChannelImage read_image(fs::path path);
std::optional<cv::Mat> image_list_to_cv(std::vector<MatX<f64>> const& channels);
void write_image(std::vector<MatX<f64>> const& channels, fs::path const& output_path);
}
