#include <doctest/doctest.h>
#include <givde/types.hpp>

#include "approx/approx.h"

using namespace givde;
using namespace approx;

TEST_CASE("Image Neighbours") {
    MatX<bool> test_image(100, 100);

    // Check the edge of the image
    auto neighbours = valid_neighbours(test_image, {0, 0});
    CHECK_EQ(neighbours.size(), 2);

    neighbours = valid_neighbours(test_image, {0, 5});
    CHECK_EQ(neighbours.size(), 3);

    neighbours = valid_neighbours(test_image, {0, 99});
    CHECK_EQ(neighbours.size(), 2);

    neighbours = valid_neighbours(test_image, {99, 0});
    CHECK_EQ(neighbours.size(), 2);

    neighbours = valid_neighbours(test_image, {99, 99});
    CHECK_EQ(neighbours.size(), 2);

    neighbours = valid_neighbours(test_image, {100, 100});
    CHECK_EQ(neighbours.size(), 0);

    neighbours = valid_neighbours(test_image, {50, 50});
    CHECK_EQ(neighbours.size(), 4);
}

TEST_CASE("Flood") {
    {
        MatX<bool> image(3, 3);
        image.setConstant(false);
        image(1, 1) = true;

        auto connected_pixels = flood(image, 1, 1);
        CHECK_EQ(connected_pixels.size(), 1);
    }

    {
        MatX<bool> image(4, 4);
        image.setConstant(false);
        image.block<2, 2>(1, 1) = Eigen::Matrix<bool, 2, 2>::Constant(true);

        auto connected_pixels = flood(image, 1, 1);
        CHECK_EQ(connected_pixels.size(), 4);
    }
}

TEST_CASE("connected components") {
    {
        MatX<bool> image(10, 10);
        image.setConstant(false);
        auto components = find_connected_components(image);
        CHECK((components.matrix.array() == 0).all());
        CHECK_EQ(components.region_map.size(), 0);
    }

    {
        MatX<bool> image(10, 10);
        image.setConstant(false);
        image.block<2, 2>(1, 1) = Eigen::Matrix<bool, 2, 2>::Constant(true);
        image.block<4, 2>(5, 5) = Eigen::Matrix<bool, 4, 2>::Constant(true);
        auto components = find_connected_components(image);
        CHECK_EQ((components.matrix.array() == 1).count(), 4);
        CHECK_EQ((components.matrix.array() == 2).count(), 8);
        CHECK_EQ(components.region_map.size(), 2);
        CHECK_EQ(components.region_map.at(1).size(), 4);
        CHECK_EQ(components.region_map.at(2).size(), 8);
    }
}
