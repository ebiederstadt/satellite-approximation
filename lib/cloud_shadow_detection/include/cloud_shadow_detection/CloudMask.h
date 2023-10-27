#pragma once
#include "types.h"

#include <opencv2/opencv.hpp>

namespace CloudMask {
struct GeneratedCloudMask {
    ImageBool cloudMask;
    ImageBool cloudMaskNoProcessing;
    ImageFloat blendedCloudProbability;
};

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
