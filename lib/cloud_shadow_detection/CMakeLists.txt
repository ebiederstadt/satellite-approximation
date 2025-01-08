set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(cloud_shadow_detection STATIC
        source/automatic_detection.cpp
        source/CloudMask.cpp
        source/CloudShadowMatching.cpp
        source/db.cpp
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
        utils
        spdlog::spdlog
        glm::glm
        glfw
        TIFF::TIFF
        Boost::headers
        Boost::boost
        OpenCL::Headers
        OpenCL::OpenCL
        SQLiteCpp
        sqlite3
        pthread
        dl
)
target_include_directories(cloud_shadow_detection PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")