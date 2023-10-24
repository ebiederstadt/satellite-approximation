#pragma once
#include "types.h"

namespace SceneClassificationLayer {
static unsigned int const NO_DATA_VALUE = 0u;
static unsigned int const SATURATED_DEFECTIVE_VALUE = 1u;
static unsigned int const DARK_AREA_PIXELS_VALUE = 2u;
static unsigned int const CLOUD_SHADOWS_VALUE = 3u;
static unsigned int const VEGITATION_VALUE = 4u;
static unsigned int const BARE_SOIL_VALUE = 5u;
static unsigned int const WATER_VALUE = 6u;
static unsigned int const CLOUD_LOW_VALUE = 7u;
static unsigned int const CLOUD_MEDIUM_VALUE = 8u;
static unsigned int const CLOUD_HIGH_VALUE = 9u;
static unsigned int const CLOUD_CIRRUS_VALUE = 10u;
static unsigned int const SNOW_ICE_VALUE = 11u;

static unsigned int const NO_DATA_MASK = 1u << NO_DATA_VALUE;
static unsigned int const SATURATED_DEFECTIVE_MASK = 1u << SATURATED_DEFECTIVE_VALUE;
static unsigned int const DARK_AREA_PIXELS_MASK = 1u << DARK_AREA_PIXELS_VALUE;
static unsigned int const CLOUD_SHADOWS_MASK = 1u << CLOUD_SHADOWS_VALUE;
static unsigned int const VEGITATION_MASK = 1u << VEGITATION_VALUE;
static unsigned int const BARE_SOIL_MASK = 1u << BARE_SOIL_VALUE;
static unsigned int const WATER_MASK = 1u << WATER_VALUE;
static unsigned int const CLOUD_LOW_MASK = 1u << CLOUD_LOW_VALUE;
static unsigned int const CLOUD_MEDIUM_MASK = 1u << CLOUD_MEDIUM_VALUE;
static unsigned int const CLOUD_HIGH_MASK = 1u << CLOUD_HIGH_VALUE;
static unsigned int const CLOUD_CIRRUS_MASK = 1u << CLOUD_CIRRUS_VALUE;
static unsigned int const SNOW_ICE_MASK = 1u << SNOW_ICE_VALUE;

static unsigned int const NO_DATA_COLOUR = 0xff000000;             // BLACK
static unsigned int const SATURATED_DEFECTIVE_COLOUR = 0xff333333; // DARK GREY
static unsigned int const DARK_AREA_PIXELS_COLOUR = 0xff00ffff;    // PINK
static unsigned int const CLOUD_SHADOWS_COLOUR = 0xffff007f;       // PURPLE
static unsigned int const VEGITATION_COLOUR = 0xff00ff00;          // GREEN
static unsigned int const BARE_SOIL_COLOUR = 0xff003300;           // DARK GREEN
static unsigned int const WATER_COLOUR = 0xffff0000;               // BLUE
static unsigned int const CLOUD_LOW_COLOUR = 0xff000033;           // DARK RED
static unsigned int const CLOUD_MEDIUM_COLOUR = 0xff00007f;        // MEDIUM RED
static unsigned int const CLOUD_HIGH_COLOUR = 0xff0000ff;          // RED
static unsigned int const CLOUD_CIRRUS_COLOUR = 0xff00ffff;        // YELLOW
static unsigned int const SNOW_ICE_COLOUR = 0xffffff00;            // LIGHT BLUE

std::shared_ptr<ImageBool> GenerateMask(std::shared_ptr<ImageUint> A, unsigned int channelCode);
ImageBool GenerateMask(ImageUint const& A, unsigned int channelCodes);
std::shared_ptr<ImageUint> GenerateRGBA(std::shared_ptr<ImageUint> A);
} // namespace SceneClassificationLayer
