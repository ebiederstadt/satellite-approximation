add_executable(main_cloud_detection main-cloud-detection.cpp)

set(libraries
        utils
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
        range-v3::range-v3)
target_link_libraries(main_cloud_detection
        ${libraries}
        opencv::opencv)

add_executable(main_spatial_approximation main-spatial-approximation.cpp)
target_link_libraries(main_spatial_approximation
        ${libraries}
        approx)

add_executable(main_analysis main-analysis.cpp)
target_link_libraries(main_analysis
        givdetypes::givdetypes
        analysis
        utils
        spdlog::spdlog)
