#pragma once
#include <memory>

#include "types.h"

namespace PotentialShadowMask {
struct PotentialShadowMaskGenerated {
    ImageBool mask;
    ImageFloat difference_of_pitfill_NIR;
    ImageFloat pitfill_result;
};

PotentialShadowMaskGenerated GeneratePotentialShadowMask(
    ImageFloat const& NIR,
    ImageBool const& CloudMask,
    ImageUint const& SCL);
} // namespace PotentialShadowMask
