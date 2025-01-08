#include "cloud_shadow_detection/automatic_detection.h"
#include <utils/geotiff.h>

#include <boost/regex.hpp>
#include <fmt/format.h>
#include <spdlog/stopwatch.h>

#include "cloud_shadow_detection/Functions.h"
#include "cloud_shadow_detection/ProbabilityRefinement.h"
#include "cloud_shadow_detection/db.h"
#include <cloud_shadow_detection/CloudMask.h>
#include <cloud_shadow_detection/CloudShadowMatching.h>
#include <cloud_shadow_detection/ComputeEnvironment.h>
#include <cloud_shadow_detection/GaussianBlur.h>
#include <cloud_shadow_detection/ImageOperations.h>
#include <cloud_shadow_detection/Imageio.h>
#include <cloud_shadow_detection/PitFillAlgorithm.h>
#include <cloud_shadow_detection/PotentialShadowMask.h>
#include <cloud_shadow_detection/VectorGridOperations.h>
#include <fmt/std.h>
#include <utils/eigen.h>
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

CloudParams::CloudParams(fs::path const& root)
    : nir_path(root / "B08.tif")
    , clp_path(root / "CLP.tif")
    , cld_path(root / "CLD.tif")
    , scl_path(root / "SCL.tif")
    , rgb_path(root / "RGB.tif")
    , view_zenith_path(root / "viewZenithMean.tif")
    , view_azimuth_path(root / "viewAzimuthMean.tif")
    , sun_zenith_path(root / "sunZenithAngles.tif")
    , sun_azimuth_path(root / "sunAzimuthAngles.tif")
{
}

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

std::optional<Status> detect(CloudParams const& params, f32 diagonal_distance, SkipShadowDetection skipShadowDetection, bool use_cache)
{
    if (use_cache && fs::exists(params.cloud_path()) && fs::exists(params.shadow_path())) {
        logger->debug("Skipping {} because both the clouds and the shadows have been computed", params.cloud_path().parent_path());
        return {};
    }

    ComputeEnvironment::InitMainContext();
    GaussianBlur::init();
    PitFillAlgorithm::init();

    Status status;

    ImageFloat clp_data = normalize(*ReadSingleChannelUint8(params.clp_path), std::numeric_limits<u8>::max());
    ImageFloat cld_data = normalize(*ReadSingleChannelUint8(params.cld_path), 100u);
    ImageUint scl_data = *ReadSingleChannelUint8(params.scl_path);
    ImageFloat nir_data = normalize(*ReadSingleChannelUint16(params.nir_path), std::numeric_limits<u16>::max());

    logger->debug(" --- Cloud Detection...");
    auto generated_cloud_mask = GenerateCloudMaskIgnoreLowProbability(clp_data, cld_data, scl_data);

    status.clouds_computed = true;
    status.percent_clouds = utils::percent_non_zero<bool>(generated_cloud_mask.cloudMask);
    status.percent_invalid = status.percent_clouds;

    {
        auto values = std::make_shared<MatX<u8>>(generated_cloud_mask.cloudMask.cast<u8>().colwise().reverse());
        utils::GeoTiffWriter<u8> tiff_writer(values, params.nir_path);
        tiff_writer.write(params.cloud_path());
    }

    // Because shadow detection can be a slow process, we provide a way for the user to skip it if too many of the pixels contain shadows
    if (skipShadowDetection.decision) {
        f64 percent = utils::percent_non_zero<bool>(generated_cloud_mask.cloudMask);
        if (percent >= skipShadowDetection.threshold) {
            logger->debug("Skipping {} because too much of the image is clouds ({:.2f}% clouds)", params.cloud_path().parent_path(), percent * 100);
            return status;
        }
    }

    logger->debug(" --- Cloud Partitioning...");
    // Using the Cloud mask, partition it into individual clouds with collections and a map
    PartitionCloudMaskReturn PartitionCloudMask_Return
        = PartitionCloudMask(generated_cloud_mask.cloudMaskNoProcessing, diagonal_distance, MinimimumCloudSizeForRayCasting);
    CloudQuads& Clouds = PartitionCloudMask_Return.clouds;
    std::shared_ptr<ImageInt>& CloudsMap = PartitionCloudMask_Return.map;

    logger->debug(" --- Potential Shadow Mask Generation...");
    // Generate the Candidate (or Potential) Shadow Mask
    auto GeneratePotentialShadowMask_Return
        = GeneratePotentialShadowMask(nir_data, generated_cloud_mask.cloudMaskNoProcessing, scl_data);
    std::shared_ptr<ImageBool> output_PSM = std::make_shared<ImageBool>(GeneratePotentialShadowMask_Return.mask);

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
        Clouds, CloudsMap, generated_cloud_mask.cloudMaskNoProcessing, output_PSM, diagonal_distance, SunPosition, ViewPosition);
    std::map<int, OptimalSolution>& OptimalCloudCastingSolutions
        = MatchCloudsShadows_Return.solutions;
    ShadowQuads& CloudCastedShadows = MatchCloudsShadows_Return.shadows;
    std::shared_ptr<ImageBool>& output_OSM = MatchCloudsShadows_Return.shadowMask;

    logger->debug(" --- Generating Probability Function...");
    // Generate the Alpha and Beta maps to produce the probability surface
    ImageFloat output_Alpha = ProbabilityRefinement::AlphaMap(GeneratePotentialShadowMask_Return.difference_of_pitfill_NIR);
    std::shared_ptr<ImageFloat> output_Beta = ProbabilityRefinement::BetaMap(
        CloudCastedShadows,
        OptimalCloudCastingSolutions,
        generated_cloud_mask.cloudMaskNoProcessing,
        output_OSM,
        generated_cloud_mask.blendedCloudProbability,
        diagonal_distance);
    UniformProbabilitySurface ProbabilityFunction
        = ProbabilityMap(output_OSM, output_Alpha, output_Beta);

    logger->debug(" --- Final Shadow Mask Generation...");
    ImageBool output_FSM = ImprovedShadowMask(
        output_OSM,
        generated_cloud_mask.cloudMask,
        output_Alpha,
        output_Beta,
        ProbabilityFunction,
        ProbabilityFunctionThreshold);
    logger->debug("...Finished Algorithm.");

    status.shadows_computed = true;
    status.percent_shadows = utils::percent_non_zero<bool>(output_FSM);
    ImageBool total_mask = generated_cloud_mask.cloudMask.array() || output_FSM.array();
    status.percent_invalid = utils::percent_non_zero<bool>(total_mask);

    logger->debug("Saving shadow results");
    {
        auto values = std::make_shared<MatX<u8>>(output_PSM->cast<u8>().colwise().reverse());
        utils::GeoTiffWriter<u8> writer(values, params.nir_path);
        writer.write(params.shadow_potential_path());
    }

    {
        auto values = std::make_shared<MatX<u8>>(output_OSM->cast<u8>().colwise().reverse());
        utils::GeoTiffWriter<u8> writer(values, params.nir_path);
        writer.write(params.object_based_shadow_path());
    }

    {
        auto values = std::make_shared<MatX<u8>>(output_FSM.cast<u8>().colwise().reverse());
        utils::GeoTiffWriter<u8> writer(values, params.nir_path);
        writer.write(params.shadow_path());
    }

    return status;
}

void detect_clouds(fs::path folder, DataBase const& db)
{
    Status status;

    ImageFloat clp_data = normalize(*ReadSingleChannelUint8(folder / "CLP.tif"), std::numeric_limits<u8>::max());
    ImageFloat cld_data = normalize(*ReadSingleChannelUint8(folder / "CLD.tif"), 100u);
    ImageUint scl_data = *ReadSingleChannelUint8(folder / "SCL.tif");

    auto generated_cloud_mask = GenerateCloudMaskIgnoreLowProbability(clp_data, cld_data, scl_data);

    status.clouds_computed = true;
    status.percent_clouds = utils::percent_non_zero<bool>(generated_cloud_mask.cloudMask);
    status.percent_invalid = status.percent_clouds;

    // Write results
    auto values = std::make_shared<MatX<u8>>(generated_cloud_mask.cloudMask.cast<u8>().colwise().reverse());
    utils::GeoTiffWriter<u8> writer(values, folder / "B08.tif");
    writer.write(folder / "cloud_mask.tif");

    db.write_detection_result(utils::Date(folder.filename().string()), status);
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

    auto status = detect(params, diagonal_distance, skipShadowDetection, use_cache);

    // The result DB should be in the parent folder
    DataBase db(directory.parent_path());
    if (status.has_value()) {
        db.write_detection_result(utils::Date(directory.filename().string()), *status);
    }

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

    std::unordered_map<utils::Date, Status> results;

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

        auto status = detect(params, diagonal_distance, skipShadowDetection, use_cache);
        if (status.has_value()) {
            results.emplace(utils::Date(directory.filename().string()), *status);
        }
    }

    DataBase db(folder_path);
    db.write_detection_results(results);

    logger->info("Finished computing");
    logger->debug("Finished in {}", sw);
}
}
