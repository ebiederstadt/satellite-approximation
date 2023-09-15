cmake_minimum_required(VERSION 3.15...3.26)

project(
        ${SKBUILD_PROJECT_NAME}
        VERSION ${SKBUILD_PROJECT_VERSION}
        LANGUAGES CXX)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

# Packages needed for library
find_package(lyra REQUIRED)
find_package(tomlplusplus REQUIRED)
find_package(Eigen3 REQUIRED CONFIG)
find_package(spdlog REQUIRED)
find_package(imgui REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 REQUIRED)
find_package(TIFF REQUIRED)
find_package(Boost REQUIRED)
find_package(OpenCLHeaders REQUIRED)
find_package(OpenCLICDLoader REQUIRED)
find_package(OpenGL REQUIRED)

add_subdirectory(lib)
add_subdirectory(executables)

set(source_files
        src/main.cpp
        src/detectCloudsShadows.cpp)
python_add_library(_core MODULE ${source_files} WITH_SOABI)
target_link_libraries(_core PRIVATE pybind11::headers cloud_shadow_detection
        bfg::lyra
        tomlplusplus::tomlplusplus
        Eigen3::Eigen
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        glm::glm
        glfw
        opengl::opengl
        TIFF::TIFF
        Boost::headers
        Boost::boost
        OpenCL::Headers
        OpenCL::OpenCL)
target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})

install(TARGETS _core DESTINATION sentinel_cloud_detection)
