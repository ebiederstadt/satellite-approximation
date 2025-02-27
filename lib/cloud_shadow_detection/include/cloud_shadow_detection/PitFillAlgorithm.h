#pragma once
#include <memory>

#include "types.h"

namespace PitFillAlgorithm {
void init();
std::shared_ptr<ImageFloat>
PitFillAlgorithmFilter(std::shared_ptr<ImageFloat> in, float borderValue);
ImageFloat PitFillAlgorithmFilter(ImageFloat const& in, float borderValue);
} // namespace PitFillAlgorithm
