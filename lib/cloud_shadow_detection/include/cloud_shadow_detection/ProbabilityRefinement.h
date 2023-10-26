#pragma once
#include <optional>

#include "CloudShadowMatching.h"
#include "types.h"

namespace ProbabilityRefinement {
// Shadow Value Map
ImageFloat AlphaMap(ImageFloat const& NIR_difference);
// Shadow Projected Probability Map
std::shared_ptr<ImageFloat> BetaMap(
    ShadowQuads shadows,
    std::map<int, CloudShadowMatching::OptimalSolution> solutions,
    ImageBool const& cloudMask,
    std::shared_ptr<ImageBool> shadowMask,
    ImageFloat const& CLP,
    float DiagonalLength);

enum class Bounds { ALPHA_MIN,
    ALPHA_MAX,
    BETA_MIN,
    BETA_MAX };

struct SurfaceRenderGeom {
    std::vector<glm::vec3> verts;
    std::vector<glm::ivec3> tris;
};

struct UniformProbabilitySurface {
    UniformProbabilitySurface();
    UniformProbabilitySurface(glm::uvec2 divs);
    float operator()(float alpha, float beta);
    float dAlpha();
    float dBeta();
    float at(int i, int j);
    void set(int i, int j, float v);
    void set(Bounds axis, float v);
    void clear(Bounds axis);
    glm::uvec2 resolution();
    SurfaceRenderGeom MeshData(int i_min, int i_max, int j_min, int j_max);

private:
    std::shared_ptr<ImageFloat> m_data;
    std::optional<float> m_alpha_min_clamp;
    std::optional<float> m_alpha_max_clamp;
    std::optional<float> m_beta_min_clamp;
    std::optional<float> m_beta_max_clamp;
};
UniformProbabilitySurface testMap();

UniformProbabilitySurface ProbabilityMap(
    std::shared_ptr<ImageBool> shadowMask,
    ImageFloat const& alphaMap,
    std::shared_ptr<ImageFloat> betaMap);
ImageBool ImprovedShadowMask(
    std::shared_ptr<ImageBool> shadowMask,
    ImageBool const& cloudMask,
    ImageFloat const& alphaMap,
    std::shared_ptr<ImageFloat> betaMap,
    UniformProbabilitySurface probabilitySurface,
    float threshold);
} // namespace ProbabilityRefinement
