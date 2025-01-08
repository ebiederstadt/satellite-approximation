set(libraries
        utils
        OpenMP::OpenMP_CXX
        opencv::opencv
        range-v3::range-v3
        GDAL::GDAL)

add_executable(poisson_main poisson-main.cpp)
target_link_libraries(poisson_main
        ${libraries}
        approx)

add_executable(laplace_main laplace-main.cpp)
target_link_libraries(laplace_main
        ${libraries}
        approx)

add_executable(main_cloud_detection main-cloud-detection.cpp)
target_link_libraries(main_cloud_detection
        GDAL::GDAL
        spdlog::spdlog
        utils
        glm::glm
        OpenCL::Headers
        OpenCL::OpenCL
        cloud_shadow_detection)
