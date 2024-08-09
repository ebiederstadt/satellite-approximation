#add_executable(main_cloud_detection main-cloud-detection.cpp)
#
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
