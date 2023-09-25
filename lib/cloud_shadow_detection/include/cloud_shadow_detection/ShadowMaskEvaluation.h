#pragma once
#include "types.h"

namespace ShadowMaskEvaluation {
static unsigned int const NO_DATA_COLOUR = 0xff000000;        // BLACK
static unsigned int const TRUE_NEGATIVE_COLOUR = 0xff00ff00;  // GREEN
static unsigned int const TRUE_POSITIVE_COLOUR = 0xffff0000;  // BLUE
static unsigned int const FALSE_NEGATIVE_COLOUR = 0xff0000ff; // RED
static unsigned int const FALSE_POSITIVE_COLOUR = 0xffff00ff; // PINK
static unsigned int const CLOUD_COLOUR = 0xffffffff;          // WHITE

struct Results {
    static unsigned int const unknown_class_value = 0u;
    static unsigned int const true_negative_class_value = 1u;
    static unsigned int const true_positive_class_value = 2u;
    static unsigned int const false_negative_class_value = 3u;
    static unsigned int const false_positive_class_value = 4u;
    static unsigned int const clouds_class_value = 5u;

    std::shared_ptr<ImageUint> pixel_classes = nullptr;
    float positive_error_total = 0.f;
    float negative_error_total = 0.f;
    float error_total = 0.f;
    float positive_error_relative = 0.f;
    float negative_error_relative = 0.f;
    float error_relative = 0.f;
    float producers_accuracy = 0.f;
    float users_accuracy = 0.f;
};

Results Evaluate(
    std::shared_ptr<ImageBool> shadow_mask,
    std::shared_ptr<ImageBool> cloud_mask,
    std::shared_ptr<ImageBool> shadow_baseline,
    ImageBounds evaluation_bounds);
std::shared_ptr<ImageUint> GenerateRGBA(std::shared_ptr<ImageUint> A);

ImageBounds CastedImageBounds(
    std::shared_ptr<ImageBool> mask,
    float DiagonalLength,
    glm::vec3 sun_pos,
    glm::vec3 view_pos,
    float height);
}; // namespace ShadowMaskEvaluation
