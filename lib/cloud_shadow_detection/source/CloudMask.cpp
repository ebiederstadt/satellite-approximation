#include "cloud_shadow_detection/CloudMask.h"

#include "cloud_shadow_detection/GaussianBlur.h"
#include "cloud_shadow_detection/ImageOperations.h"
#include "cloud_shadow_detection/SceneClassificationLayer.h"

#include <utils/types.h>
#include <opencv2/core/eigen.hpp>
#include <opencv2/imgproc.hpp>

using namespace utils;
using namespace ImageOperations;
using namespace GaussianBlur;
using namespace SceneClassificationLayer;

namespace CloudMask {
GeneratedCloudMask GenerateCloudMask(ImageFloat const& CLP, ImageFloat const& CLD, ImageUint const& SCL)
{
    GeneratedCloudMask ret;
    ret.blendedCloudProbability = GaussianBlurFilter(CLP, 4.f);
    // clang-format off
    Image<bool> mask = (ret.blendedCloudProbability.array() >= .5f && CLD.array() >= .2f).array()
                        || GenerateMask(SCL, CLOUD_LOW_MASK | CLOUD_MEDIUM_MASK | CLOUD_HIGH_MASK).array();
    // clang-format on
    ret.cloudMask = GaussianBlurFilter(mask.cast<float>(), 1.f).array() >= 0.1f;
    ret.cloudMaskNoProcessing = ret.cloudMask;
    return ret;
}

GeneratedCloudMask GenerateCloudMaskIgnoreLowProbability(ImageFloat const& CLP, ImageFloat const& CLD, ImageUint const& SCL)
{
    GeneratedCloudMask ret;
    ret.blendedCloudProbability = GaussianBlurFilter(CLP, 4.f);
    // clang-format off
    Image<bool> mask = (ret.blendedCloudProbability.array() >= .5f && CLD.array() >= .2f).array()
                        || GenerateMask(SCL, CLOUD_MEDIUM_MASK | CLOUD_HIGH_MASK).array();
    // clang-format on
    ret.cloudMask = mask.cast<float>().array() >= 0.1f;
    ret.cloudMaskNoProcessing = ret.cloudMask;

    // Clean up result using image processing techniques
    cv::Mat input_output;
    cv::MorphShapes morph_shape = cv::MorphShapes::MORPH_ELLIPSE;
    cv::eigen2cv(ret.cloudMask.cast<u8>().eval(), input_output);

    // dilate to take care of boundary clouds that are missed by the SCL mask
    int dilation_size = 15;
    cv::Mat kernel = cv::getStructuringElement(morph_shape, { 2 * dilation_size + 1, 2 * dilation_size + 1 });
    cv::dilate(input_output, input_output, kernel);

    // Close to remove holes in the generated mask
    int close_size = 5;
    kernel = cv::getStructuringElement(morph_shape, { 2 * close_size + 1, 2 * close_size + 1 });
    cv::morphologyEx(input_output, input_output, cv::MORPH_CLOSE, kernel);

    // Blur to clean up the edges
    cv::GaussianBlur(input_output, input_output, { 11, 11 }, 0.0);

    cv::cv2eigen(input_output, ret.cloudMask);
    return ret;
}

PartitionCloudMaskReturn PartitionCloudMask(
    ImageBool const& CloudMaskData,
    float DiagonalLength,
    unsigned int min_cloud_area)
{
    PartitionCloudMaskReturn ret;
    ret.map = std::make_shared<ImageInt>(CloudMaskData.rows(), CloudMaskData.cols());
    ret.map->fill(-1);
    std::vector<glm::uvec2> current_cloud_pixels;
    CloudQuad cloud_temp;
    for (int i = 0, CN = 0; i < ret.map->cols(); i++) {
        for (int j = 0; j < ret.map->rows(); j++) {
            if (at(CloudMaskData, i, j) && at(ret.map, i, j) < 0) { // Unassigned Cloud pixels
                current_cloud_pixels = flood(CloudMaskData, i, j);
                // Large enough to be counted as a cloud object
                if (current_cloud_pixels.size() >= min_cloud_area) {
                    int min_x = std::numeric_limits<int>::max();
                    int min_y = std::numeric_limits<int>::max();
                    int max_x = std::numeric_limits<int>::min();
                    int max_y = std::numeric_limits<int>::min();
                    for (auto& p : current_cloud_pixels) {
                        set(ret.map, p.x, p.y, CN);
                        min_x = std::min(min_x, int(p.x));
                        max_x = std::max(max_x, int(p.x));
                        min_y = std::min(min_y, int(p.y));
                        max_y = std::max(max_y, int(p.y));
                    }
                    cloud_temp.pixels.list = current_cloud_pixels;
                    cloud_temp.pixels.bounds.p0 = glm::uvec2(min_x, min_y);
                    cloud_temp.pixels.bounds.p1 = glm::uvec2(max_x, max_y);
                    cloud_temp.pixels.id = CN++;
                    cloud_temp.quad.p00
                        = pos(CloudMaskData, DiagonalLength, min_x, min_y, .1f, .1f); // p11---p10
                    cloud_temp.quad.p01
                        = pos(CloudMaskData, DiagonalLength, max_x, min_y, .9f, .1f); //  |<<<<<|
                    cloud_temp.quad.p10
                        = pos(CloudMaskData, DiagonalLength, max_x, max_y, .9f, .9f); //  |>>>>>|
                    cloud_temp.quad.p11
                        = pos(CloudMaskData, DiagonalLength, min_x, max_y, .1f, .9f); // p00---p01
                    ret.clouds.insert({ cloud_temp.pixels.id, cloud_temp });
                }
            }
        }
    }
    return ret;
}
}
