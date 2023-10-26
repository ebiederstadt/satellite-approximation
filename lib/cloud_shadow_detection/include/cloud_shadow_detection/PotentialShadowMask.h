#pragma once
#include <memory>

#include "types.h"

namespace PotentialShadowMask {
struct PotentialShadowMaskGenerationReturn {
    std::shared_ptr<ImageBool> mask;
    std::shared_ptr<ImageFloat> difference_of_pitfill_NIR;
};
struct PotentialShadowMaskGenerated {
    ImageBool mask;
    ImageFloat difference_of_pitfill_NIR;
    ImageFloat pitfill_result;
};

PotentialShadowMaskGenerationReturn GeneratePotentialShadowMask(
    std::shared_ptr<ImageFloat> NIR,
    ImageBool const& CloudMask,
    std::shared_ptr<ImageUint> SCL);

PotentialShadowMaskGenerated GeneratePotentialShadowMask(
    ImageFloat const& NIR,
    ImageBool const& CloudMask,
    ImageUint const& SCL);
} // namespace PotentialShadowMask
