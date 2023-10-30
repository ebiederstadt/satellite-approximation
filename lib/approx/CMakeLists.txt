set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(approx STATIC
        source/laplace.cpp
        source/poisson.cpp
        source/db.cpp
        source/utils.cpp)
target_include_directories(approx PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

target_link_libraries(approx PRIVATE
        givdetypes::givdetypes
        utils
        range-v3::range-v3
        spdlog::spdlog
        Eigen3::Eigen
        SQLiteCpp
        sqlite3
        pthread
        dl
        opencv::opencv
        magic_enum::magic_enum)
