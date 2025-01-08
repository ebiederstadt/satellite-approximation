cmake_minimum_required(VERSION 3.15...3.26)

project(
        ${SKBUILD_PROJECT_NAME}
        VERSION ${SKBUILD_PROJECT_VERSION}
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(Python REQUIRED COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Packages needed for library
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)
find_package(range-v3 REQUIRED)
find_package(GDAL CONFIG REQUIRED)
find_package(magic_enum REQUIRED)
find_package(OpenCV REQUIRED)
find_package(OpenMP REQUIRED)
find_package(glm REQUIRED)
find_package(OpenCLHeaders REQUIRED)
find_package(OpenCLICDLoader REQUIRED)

include_directories(thirdparty/eigen-master)

add_subdirectory(lib/cloud_shadow_detection)
add_subdirectory(lib/approx)
add_subdirectory(lib/utils)
add_subdirectory(executables)

add_subdirectory(thirdparty/SQLiteCpp-3.3.1)

set(source_files src/main.cpp)
pybind11_add_module(_core MODULE ${source_files} WITH_SOABI)
target_link_libraries(_core PRIVATE pybind11::headers
        range-v3::range-v3
        approx
        cloud_shadow_detection
        utils
        spdlog::spdlog
        GDAL::GDAL)
target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})

install(TARGETS _core DESTINATION sentinel_data_processing)
