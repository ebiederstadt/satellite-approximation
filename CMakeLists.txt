cmake_minimum_required(VERSION 3.15...3.26)

project(
        ${SKBUILD_PROJECT_NAME}
        VERSION ${SKBUILD_PROJECT_VERSION}
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

# Packages needed for library
find_package(givdetypes REQUIRED)
find_package(lyra REQUIRED)
find_package(tomlplusplus REQUIRED)
find_package(Eigen3 REQUIRED CONFIG)
find_package(spdlog REQUIRED)
find_package(imgui REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(glm REQUIRED)
find_package(TIFF REQUIRED)
find_package(Boost REQUIRED)
find_package(OpenCLHeaders REQUIRED)
find_package(OpenCLICDLoader REQUIRED)
find_package(range-v3 REQUIRED)
find_package(GDAL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(magic_enum REQUIRED)
find_package(OpenCV REQUIRED)

add_subdirectory(lib/analysis)
add_subdirectory(lib/cloud_shadow_detection)
add_subdirectory(lib/approx)
add_subdirectory(lib/utils)
add_subdirectory(executables)

add_subdirectory(thirdparty/SQLiteCpp-3.3.1)

set(source_files src/main.cpp)
python_add_library(_core MODULE ${source_files} WITH_SOABI)
target_link_libraries(_core PRIVATE pybind11::headers
        range-v3::range-v3
        cloud_shadow_detection
        approx
        analysis
        utils
        bfg::lyra
        givdetypes::givdetypes
        tomlplusplus::tomlplusplus
        Eigen3::Eigen
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        glm::glm
        glfw
        TIFF::TIFF
        Boost::headers
        Boost::boost
        OpenCL::Headers
        OpenCL::OpenCL)
target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})

install(TARGETS _core DESTINATION sentinel_data_processing)

#enable_testing()
#find_package(doctest REQUIRED)
#add_executable(test_sentinel_data_processing tests/main.cpp)
#target_link_libraries(test_sentinel_data_processing PRIVATE
#        approx
#        doctest::doctest
#        givdetypes::givdetypes)
