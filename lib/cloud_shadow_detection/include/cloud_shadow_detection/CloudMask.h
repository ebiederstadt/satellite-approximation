#pragma once
#include "types.h"

#include <opencv2/opencv.hpp>

namespace CloudMask {
struct GenerateCloudMaskReturn {
    std::shared_ptr<ImageBool> cloudMask;
    std::shared_ptr<ImageFloat> blendedCloudProbability;
};

struct GeneratedCloudMask {
    ImageBool cloudMask;
    ImageBool cloudMaskNoProcessing;
    ImageFloat blendedCloudProbability;
};

GenerateCloudMaskReturn GenerateCloudMask(
    std::shared_ptr<ImageFloat> CLP,
    std::shared_ptr<ImageFloat> CLD,
    std::shared_ptr<ImageUint> SCL);

GeneratedCloudMask GenerateCloudMask(ImageFloat const& CLP, ImageFloat const& CLD, ImageUint const& SCL);
GeneratedCloudMask GenerateCloudMaskIgnoreLowProbability(ImageFloat const& CLP, ImageFloat const& CLD, ImageUint const& SCL);

struct PartitionCloudMaskReturn {
    CloudQuads clouds;
    std::shared_ptr<ImageInt> map;
};
PartitionCloudMaskReturn PartitionCloudMask(
    ImageBool const& CloudMaskData,
    float DiagonalLength,
    unsigned int min_cloud_area);
} // namespace CloudMask
