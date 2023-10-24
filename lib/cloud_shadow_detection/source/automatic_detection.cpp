#include "cloud_shadow_detection/automatic_detection.h"
#include <utils/fmt_filesystem.h>
#include <utils/geotiff.h>

#include <boost/regex.hpp>
#include <fmt/format.h>
#include <spdlog/stopwatch.h>

#include "cloud_shadow_detection/Functions.h"
#include "cloud_shadow_detection/ProbabilityRefinement.h"
#include <cloud_shadow_detection/CloudMask.h>
#include <cloud_shadow_detection/CloudShadowMatching.h>
#include <cloud_shadow_detection/ComputeEnvironment.h>
#include <cloud_shadow_detection/GaussianBlur.h>
#include <cloud_shadow_detection/ImageOperations.h>
#include <cloud_shadow_detection/Imageio.h>
#include <cloud_shadow_detection/PitFillAlgorithm.h>
#include <cloud_shadow_detection/PotentialShadowMask.h>
#include <cloud_shadow_detection/VectorGridOperations.h>
#include <utils/filesystem.h>

using namespace Imageio;
using namespace ImageOperations;
using namespace CloudMask;
using namespace PotentialShadowMask;
using namespace VectorGridOperations;
using namespace CloudShadowMatching;
using namespace ProbabilityRefinement;

namespace remote_sensing {
static constexpr int MinimimumCloudSizeForRayCasting = 3;
static constexpr float DistanceToSun = 1.5e9f;
static constexpr float DistanceToView = 785.f;
static constexpr float ProbabilityFunctionThreshold = .15f;

static auto logger = utils::create_logger("cloud_shadow_detection::automatic_detection");

fs::path CloudParams::cloud_path() const
{
    return nir_path.parent_path() / "cloud_mask.tif";
}

fs::path CloudParams::shadow_potential_path() const
{
    return nir_path.parent_path() / "potential_shadows.tif";
}

fs::path CloudParams::object_based_shadow_path() const
{
    return nir_path.parent_path() / "object_based_shadows.tif";
}

fs::path CloudParams::shadow_path() const
{
    return nir_path.parent_path() / "shadow_mask.tif";
}

f32 get_diagonal_distance(f64 min_long, f64 min_lat, f64 max_long, f64 max_lat)
{
    return Functions::distance(
        { min_long, min_lat },
        { max_long, max_lat });
}

void detect(CloudParams const& params, f32 diagonal_distance, SkipShadowDetection skipShadowDetection, bool use_cache)
{
    if (use_cache && fs::exists(params.cloud_path()) && fs::exists(params.shadow_path())) {
        logger->debug("Skipping {} because both the clouds and the shadows have been computed", params.cloud_path().parent_path());
        return;
    }

    ComputeEnvironment::InitMainContext();
    GaussianBlur::init();
    PitFillAlgorithm::init();

    std::shared_ptr<ImageFloat> data_NIR;
    try {
        data_NIR = normalize(
            ReadSingleChannelUint16(params.nir_path), std::numeric_limits<uint16_t>::max());
    } catch (...) {
        throw std::runtime_error(fmt::format("Failed to open NIR file. Provided path: {}", params.nir_path));
    }

    ImageFloat clp_data = normalize(utils::GeoTIFF<u8>(params.clp_path).values);
    ImageFloat cld_data = normalize<u8>(utils::GeoTIFF<u8>(params.cld_path).values, 100u);
    ImageUint scl_data = utils::GeoTIFF<u32>(params.scl_path).values;

    std::shared_ptr<ImageUint> data_SCL;
    try {
        data_SCL = ReadSingleChannelUint8(params.scl_path);
    } catch (std::runtime_error const& e) {
        throw std::runtime_error(fmt::format("Failed to open SCL file. Provided path: {}", params.scl_path));
    }

    logger->debug(" --- Cloud Detection...");
    auto generated_cloud_mask = GenerateCloudMaskIgnoreLowProbability(clp_data, cld_data, scl_data);

    std::shared_ptr<ImageFloat> BlendedCloudProbability
        = std::make_shared<ImageFloat>(generated_cloud_mask.blendedCloudProbability);
    std::shared_ptr<ImageBool> output_CM = std::make_shared<ImageBool>(generated_cloud_mask.cloudMask);

    utils::GeoTIFF<u8> template_geotiff(params.nir_path);
    try {
        template_geotiff.values = output_CM->cast<u8>();
        template_geotiff.values.colwise().reverseInPlace();
        template_geotiff.write(params.cloud_path());
    } catch (std::runtime_error const& e) {
        throw std::runtime_error(
            fmt::format("Failed to write cloud mask. Path: {}. Message: {}", params.cloud_path(),
                e.what()));
    }

    // Because shadow detection can be a slow process, we provide a way for the user to skip it if too many of the pixels contain shadows
    if (skipShadowDetection.decision) {
        f64 percent = static_cast<f64>(output_CM->cast<int>().sum()) / static_cast<f64>(output_CM->size());
        if (percent >= skipShadowDetection.threshold) {
            logger->debug("Skipping {} because too much of the image is cloudy ({:.2f}% cloudy)", params.cloud_path().parent_path(), percent * 100);
            return;
        }
    }

    logger->debug(" --- Cloud Partitioning...");
    // Using the Cloud mask, partition it into individual clouds with collections and a map
    PartitionCloudMaskReturn PartitionCloudMask_Return
        = PartitionCloudMask(output_CM, diagonal_distance, MinimimumCloudSizeForRayCasting);
    CloudQuads& Clouds = PartitionCloudMask_Return.clouds;
    std::shared_ptr<ImageInt>& CloudsMap = PartitionCloudMask_Return.map;

    logger->debug(" --- Potential Shadow Mask Generation...");
    // Generate the Candidate (or Potential) Shadow Mask
    PotentialShadowMaskGenerationReturn GeneratePotentialShadowMask_Return
        = GeneratePotentialShadowMask(data_NIR, output_CM, data_SCL);
    std::shared_ptr<ImageBool> output_PSM = GeneratePotentialShadowMask_Return.mask;
    std::shared_ptr<ImageFloat> DeltaNIR
        = GeneratePotentialShadowMask_Return.difference_of_pitfill_NIR;

    // Load everything that we need for
    std::shared_ptr<ImageFloat> data_SunZenith;
    try {
        data_SunZenith = ReadSingleChannelFloat(params.sun_zenith_path);
    } catch (...) {
        throw std::runtime_error(
            fmt::format("Failed to open Sun Zenith file. Provided path: {}", params.sun_zenith_path));
    }

    std::shared_ptr<ImageFloat> data_SunAzimuth;
    try {
        data_SunAzimuth = ReadSingleChannelFloat(params.sun_azimuth_path);
    } catch (...) {
        throw std::runtime_error(
            fmt::format("Failed to open Sun Azimuth file. Provided path: {}", params.sun_azimuth_path));
    }

    std::shared_ptr<ImageFloat> data_ViewZenith;
    try {
        data_ViewZenith = ReadSingleChannelFloat(params.view_zenith_path);
    } catch (...) {
        throw std::runtime_error(
            fmt::format("Failed to open View Zenith file. Provided path: {}", params.view_zenith_path));
    }

    std::shared_ptr<ImageFloat> data_ViewAzimuth;
    try {
        data_ViewAzimuth = ReadSingleChannelFloat(params.view_azimuth_path);
    } catch (...) {
        throw std::runtime_error(
            fmt::format("Failed to open View Azimuth file. Provided path: {}", params.view_azimuth_path));
    }

    logger->debug(" --- Solving for Sun and Satellite Position...");
    // Generate a Vector grid for each
    std::shared_ptr<VectorGrid> SunVectorGrid
        = GenerateVectorGrid(toRadians(data_SunZenith), toRadians(data_SunAzimuth));
    std::shared_ptr<VectorGrid> ViewVectorGrid
        = GenerateVectorGrid(toRadians(data_ViewZenith), toRadians(data_ViewAzimuth));
    LMSPointReturn SunLSPointEqualTo_Return
        = LSPointEqualTo(SunVectorGrid, diagonal_distance, DistanceToSun);
    glm::vec3& SunPosition = SunLSPointEqualTo_Return.p;
    LMSPointReturn ViewLSPointEqualTo_Return
        = LSPointEqualTo(ViewVectorGrid, diagonal_distance, DistanceToView);
    glm::vec3& ViewPosition = ViewLSPointEqualTo_Return.p;

    logger->debug(" --- Object-based Shadow Mask Generation...");
    // Solve for the optimal shadow matching results per cloud
    MatchCloudsShadowsResults MatchCloudsShadows_Return = MatchCloudsShadows(
        Clouds, CloudsMap, output_CM, output_PSM, diagonal_distance, SunPosition, ViewPosition);
    std::map<int, OptimalSolution>& OptimalCloudCastingSolutions
        = MatchCloudsShadows_Return.solutions;
    ShadowQuads& CloudCastedShadows = MatchCloudsShadows_Return.shadows;
    std::shared_ptr<ImageBool>& output_OSM = MatchCloudsShadows_Return.shadowMask;

    logger->debug(" --- Generating Probability Function...");
    // Generate the Alpha and Beta maps to produce the probability surface
    std::shared_ptr<ImageFloat> output_Alpha = ProbabilityRefinement::AlphaMap(DeltaNIR);
    std::shared_ptr<ImageFloat> output_Beta = ProbabilityRefinement::BetaMap(
        CloudCastedShadows,
        OptimalCloudCastingSolutions,
        output_CM,
        output_OSM,
        BlendedCloudProbability,
        diagonal_distance);
    UniformProbabilitySurface ProbabilityFunction
        = ProbabilityMap(output_OSM, output_Alpha, output_Beta);

    logger->debug(" --- Final Shadow Mask Generation...");
    std::shared_ptr<ImageBool> output_FSM = ImprovedShadowMask(
        output_OSM,
        output_CM,
        output_Alpha,
        output_Beta,
        ProbabilityFunction,
        ProbabilityFunctionThreshold);
    logger->debug("...Finished Algorithm.");

    logger->debug("Saving shadow results");
    try {
        template_geotiff.values = output_PSM->cast<u8>();
        template_geotiff.values.colwise().reverseInPlace();
        template_geotiff.write(params.shadow_potential_path());
    } catch (std::runtime_error const& e) {
        throw std::runtime_error(
            fmt::format("Failed to write potential shadow mask. Path: {}. Message: {}", params.cloud_path(),
                e.what()));
    }

    try {
        template_geotiff.values = output_OSM->cast<u8>();
        template_geotiff.values.colwise().reverseInPlace();
        template_geotiff.write(params.object_based_shadow_path());
    } catch (std::runtime_error const& e) {
        throw std::runtime_error(
            fmt::format("Failed to write object-based shadow mask. Path: {}. Message: {}", params.cloud_path(),
                e.what()));
    }

    try {
        template_geotiff.values = output_FSM->cast<u8>();
        template_geotiff.values.colwise().reverseInPlace();
        template_geotiff.write(params.shadow_path());
    } catch (std::runtime_error const& e) {
        throw std::runtime_error(
            fmt::format("Failed to write final shadow mask. Path: {}. Message: {}", params.cloud_path(),
                e.what()));
    }
}

void detect_single_folder(fs::path directory, f32 diagonal_distance, SkipShadowDetection skipShadowDetection, bool use_cache)
{
    logger->debug("Starting calculation");
    spdlog::stopwatch sw;
    CloudParams params;
    params.nir_path = directory / fs::path("B08.tif");
    params.clp_path = directory / fs::path("CLP.tif");
    params.cld_path = directory / fs::path("CLD.tif");
    params.scl_path = directory / fs::path("SCL.tif");
    params.rgb_path = directory / fs::path("RGB.tif");
    params.view_zenith_path = directory / fs::path("viewZenithMean.tif");
    params.view_azimuth_path = directory / fs::path("viewAzimuthMean.tif");
    params.sun_zenith_path = directory / fs::path("sunZenithAngles.tif");
    params.sun_azimuth_path = directory / fs::path("sunAzimuthAngles.tif");

    detect(params, diagonal_distance, skipShadowDetection, use_cache);

    logger->debug("Finished in {:.2f}", sw);
}

void detect_in_folder(fs::path folder_path, f32 diagonal_distance, SkipShadowDetection skipShadowDetection,
    bool use_cache)
{
    std::vector<fs::path> directories;
    for (auto const& folder : fs::directory_iterator(folder_path)) {
        if (fs::is_directory(folder) && utils::find_directory_contents(folder) == utils::DirectoryContents::MultiSpectral) {
            directories.push_back(folder);
        }
    }

    logger->debug("Starting calculation");
    spdlog::stopwatch sw;
    for (auto const& directory : directories) {
        logger->info("Calculating for {}", directory.filename());
        CloudParams params;
        params.nir_path = directory / fs::path("B08.tif");
        params.clp_path = directory / fs::path("CLP.tif");
        params.cld_path = directory / fs::path("CLD.tif");
        params.scl_path = directory / fs::path("SCL.tif");
        params.rgb_path = directory / fs::path("RGB.tif");
        params.view_zenith_path = directory / fs::path("viewZenithMean.tif");
        params.view_azimuth_path = directory / fs::path("viewAzimuthMean.tif");
        params.sun_zenith_path = directory / fs::path("sunZenithAngles.tif");
        params.sun_azimuth_path = directory / fs::path("sunAzimuthAngles.tif");

        detect(params, diagonal_distance, skipShadowDetection, use_cache);
    }
    logger->info("Finished computing");
    logger->debug("Finished in {}", sw);
}
}