add_executable(main_cloud_detection main.cpp)
target_include_directories(main_cloud_detection PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(main_cloud_detection
        cloud_shadow_detection
        bfg::lyra
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
        OpenCL::OpenCL
)
