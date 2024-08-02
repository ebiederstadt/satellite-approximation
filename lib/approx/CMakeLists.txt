set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(approx STATIC
        source/laplace.cpp
        source/poisson.cpp
        source/db.cpp
        source/utils.cpp)
target_include_directories(approx PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

target_link_libraries(approx PRIVATE
        utils
        range-v3::range-v3
        Boost::boost
        opencv::opencv)
