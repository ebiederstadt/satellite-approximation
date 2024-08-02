set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(utils
        source/date.cpp
        source/db.cpp
        source/error.cpp
        source/filesystem.cpp
        source/geotiff.cpp
        source/indices.cpp
        source/log.cpp
        include/utils/types.h)
target_include_directories(utils PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

target_link_libraries(utils
        GDAL::GDAL
        boost::boost
        spdlog::spdlog
        opencv::opencv
        magic_enum::magic_enum
        SQLiteCpp)
