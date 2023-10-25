#include "cloud_shadow_detection/PotentialShadowMask.h"

#include "cloud_shadow_detection/Functions.h"
#include "cloud_shadow_detection/GaussianBlur.h"
#include "cloud_shadow_detection/ImageOperations.h"
#include "cloud_shadow_detection/PitFillAlgorithm.h"
#include "cloud_shadow_detection/SceneClassificationLayer.h"

using namespace ImageOperations;
using namespace GaussianBlur;
using namespace SceneClassificationLayer;
using namespace PitFillAlgorithm;
using namespace Functions;

PotentialShadowMask::PotentialShadowMaskGenerationReturn
PotentialShadowMask::GeneratePotentialShadowMask(
    std::shared_ptr<ImageFloat> NIR,
    ImageBool const& CloudMask,
    std::shared_ptr<ImageUint> SCL)
{
    ImageBool SCL_SHADOW_DARK
        = GenerateMask(*SCL, CLOUD_SHADOWS_MASK | DARK_AREA_PIXELS_MASK);
    ImageBool SCL_SHADOW_DARK_WATER
        = GenerateMask(*SCL, CLOUD_SHADOWS_MASK | DARK_AREA_PIXELS_MASK | WATER_MASK);
    std::vector<float> ClearSky_NIR_Values
        = partitionUnobscuredObscured(*NIR, CloudMask.array() || SCL_SHADOW_DARK_WATER.array()).first;
    float CloudCover_percent = CoverPercentage(CloudMask);
    float ClearSky_NIR_percent = linearStep(CloudCover_percent, { .07f, .2f }, { .4f, .7f });
    float Outside_value = percentile(ClearSky_NIR_Values, ClearSky_NIR_percent);
    std::shared_ptr<ImageFloat> NIR_pitfilled = PitFillAlgorithmFilter(NIR, Outside_value);
    std::shared_ptr<ImageFloat> NIR_difference = SUBTRACT(NIR_pitfilled, NIR);
    std::shared_ptr<ImageBool> NIR_prelim_mask = Threshold(NIR_difference, .12f);
    ImageBool Result_prelim_mask = GaussianBlurFilter((NIR_prelim_mask->array() || SCL_SHADOW_DARK.array()).cast<float>(), 1.f).array() >= 0.1f;
    ImageBool Result_mask = !CloudMask.array() && Result_prelim_mask.array();
    return { std::make_shared<ImageBool>(Result_mask), NIR_difference };
}