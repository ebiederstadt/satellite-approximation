set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(utils
        source/date.cpp
        source/db.cpp
        source/error.cpp
        source/filesystem.cpp
        source/geotiff.cpp
        source/log.cpp)
target_include_directories(utils PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

target_link_libraries(utils
        givdetypes::givdetypes
        GDAL::GDAL
        spdlog::spdlog)
