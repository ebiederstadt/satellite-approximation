set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(spatial_approximation STATIC
        source/approx.cpp
        source/results.cpp)
target_include_directories(spatial_approximation PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

target_link_libraries(spatial_approximation PRIVATE
        givdetypes::givdetypes
        utils
        range-v3::range-v3
        spdlog::spdlog
        Eigen3::Eigen
        SQLite::SQLite3)
