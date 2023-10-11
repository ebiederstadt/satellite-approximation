set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(analysis STATIC
        source/db.cpp
        source/noise.cpp
        source/sis.cpp
        source/utils.cpp)
target_include_directories(analysis PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

target_link_libraries(analysis PRIVATE
        utils
        givdetypes::givdetypes
        magic_enum::magic_enum
        SQLite::SQLite3
        opencv::opencv)
