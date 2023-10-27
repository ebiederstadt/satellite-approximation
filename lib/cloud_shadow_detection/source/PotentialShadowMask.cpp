#include "cloud_shadow_detection/PotentialShadowMask.h"

#include "cloud_shadow_detection/Functions.h"
#include "cloud_shadow_detection/GaussianBlur.h"
#include "cloud_shadow_detection/ImageOperations.h"
#include "cloud_shadow_detection/PitFillAlgorithm.h"
#include "cloud_shadow_detection/SceneClassificationLayer.h"

#include <utils/eigen.h>
#include <utils/log.h>

using namespace ImageOperations;
using namespace GaussianBlur;
using namespace SceneClassificationLayer;
using namespace PitFillAlgorithm;
using namespace Functions;

namespace PotentialShadowMask {
static auto logger = utils::create_logger("clouds_shadows::ShadowMask");

PotentialShadowMaskGenerated GeneratePotentialShadowMask(
    ImageFloat const& NIR,
    ImageBool const& CloudMask,
    ImageUint const& SCL)
{
    ImageBool SCL_SHADOW_DARK
        = GenerateMask(SCL, CLOUD_SHADOWS_MASK | DARK_AREA_PIXELS_MASK);
    ImageBool SCL_SHADOW_DARK_WATER
        = GenerateMask(SCL, CLOUD_SHADOWS_MASK | DARK_AREA_PIXELS_MASK | WATER_MASK);
    std::vector<float> ClearSky_NIR_Values
        = partitionUnobscuredObscured(NIR, CloudMask.array() || SCL_SHADOW_DARK_WATER.array());
    float CloudCover_percent = CoverPercentage(CloudMask);
    float ClearSky_NIR_percent = linearStep(CloudCover_percent, { .07f, .2f }, { .4f, .7f });
    float Outside_value = percentile(ClearSky_NIR_Values, ClearSky_NIR_percent);
    ImageFloat NIR_pitfilled = PitFillAlgorithmFilter(NIR, Outside_value);
    ImageFloat NIR_difference = NIR_pitfilled.array() - NIR.array();
    ImageBool NIR_prelim_mask = NIR_difference.array() >= .02f;
    ImageBool Result_prelim_mask = GaussianBlurFilter((NIR_prelim_mask.array() || SCL_SHADOW_DARK.array()).cast<float>(), 1.f).array() >= 0.1f;
    ImageBool Result_mask = !CloudMask.array() && Result_prelim_mask.array();

    // Check to see if the pixel is likely to belong to a water source
    // 1. Look through a sample of non-cloudy images (say, 15 of the most recent images relative to the current date)
    //  to see what NIR value is. If it has low variance and is below some threshold, then it is likely to be water
    // 2. Use the water detection from Zhu & Woodcock over the past bit. If most of the non-cloudy days indicate a water pixel, likely to be water
    // 3. Use sentinel 1 data to learn about which pixels are likely to contain water
    // 4. Use DEM data to learn about where the depressions are in the landscape, which give additional support for potential water pixels

    return { Result_mask, NIR_difference, NIR_pitfilled };
}
}