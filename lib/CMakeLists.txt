add_library(cloud_shadow_detection STATIC
        source/CloudMask.cpp
        source/CloudShadowMatching.cpp
        source/ComputeEnvironment.cpp
        source/Functions.cpp
        source/GaussianBlur.cpp
        source/Imageio.cpp
        source/ImageOperations.cpp
        source/PitFillAlgorithm.cpp
        source/PotentialShadowMask.cpp
        source/ProbabilityRefinement.cpp
        source/SceneClassificationLayer.cpp
        source/ShadowMaskEvaluation.cpp
        source/types.cpp
        source/VectorGridOperations.cpp)

target_link_libraries(
        cloud_shadow_detection PRIVATE
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
        OpenCL::OpenCL
)
target_include_directories(cloud_shadow_detection PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")