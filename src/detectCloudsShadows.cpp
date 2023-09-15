#include "detectCloudsShadows.h"
#include "fmt_filesystem.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <boost/regex.hpp>

#include "cloud_shadow_detection/Functions.h"
#include "cloud_shadow_detection/ProbabilityRefinement.h"
#include <cloud_shadow_detection/CloudMask.h>
#include <cloud_shadow_detection/Imageio.h>
#include <cloud_shadow_detection/ImageOperations.h>
#include <cloud_shadow_detection/ComputeEnvironment.h>
#include <cloud_shadow_detection/GaussianBlur.h>
#include <cloud_shadow_detection/PitFillAlgorithm.h>
#include <cloud_shadow_detection/PotentialShadowMask.h>
#include <cloud_shadow_detection/VectorGridOperations.h>
#include <cloud_shadow_detection/CloudShadowMatching.h>

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

    fs::path CloudParams::cloud_path() const {
        return nir_path.parent_path() / "cloud_mask.tif";
    }

    fs::path CloudParams::shadow_potential_path() const {
        return nir_path.parent_path() / "potential_shadows.tif";
    }

    fs::path CloudParams::object_based_shadow_path() const {
        return nir_path.parent_path() / "object_based_shadows.tif";
    }

    fs::path CloudParams::shadow_path() const {
        return nir_path.parent_path() / "shadow_mask.tif";
    }

    f32 get_diagonal_distance(f64 min_long, f64 min_lat, f64 max_long, f64 max_lat) {
        return Functions::distance(
                {min_long, min_lat},
                {max_long, max_lat}
        );
    }

    void
    detect(const CloudParams &params, f32 diagonal_distance, SkipShadowDetection skipShadowDetection, bool use_cache) {
        if (use_cache && fs::exists(params.cloud_path())) {
            return;
        }

        ComputeEnvironment::InitMainContext();
        GaussianBlur::init();
        PitFillAlgorithm::init();

        std::shared_ptr<ImageFloat> data_NIR;
        try {
            data_NIR = normalize(
                    ReadSingleChannelUint16(params.nir_path), std::numeric_limits<uint16_t>::max()
            );
        } catch (...) {
            throw std::runtime_error(fmt::format("Failed to open NIR file. Provided path: {}", params.nir_path));
        }

        std::shared_ptr<ImageFloat> data_CLP;
        try {
            data_CLP = normalize(
                    Imageio::ReadSingleChannelUint8(params.clp_path),
                    std::numeric_limits<u8>::max());
        } catch (std::runtime_error const &e) {
            throw std::runtime_error(fmt::format("Failed to open CLP file. Provided path: {}", params.clp_path));
        }

        std::shared_ptr<ImageFloat> data_CLD;
        try {
            data_CLD = normalize(ReadSingleChannelUint8(params.cld_path), 100u);
        } catch (std::runtime_error const &e) {
            throw std::runtime_error(fmt::format("Failed to open CLD file. Provided path: {}", params.cld_path));
        }

        std::shared_ptr<ImageUint> data_SCL;
        try {
            data_SCL = ReadSingleChannelUint8(params.scl_path);
        } catch (std::runtime_error const &e) {
            throw std::runtime_error(fmt::format("Failed to open SCL file. Provided path: {}", params.scl_path));
        }

        spdlog::debug(" --- Cloud Detection...");
        GenerateCloudMaskReturn generated_cloud_mask
                = GenerateCloudMask(data_CLP, data_CLD, data_SCL);
        std::shared_ptr<ImageFloat> &BlendedCloudProbability
                = generated_cloud_mask.blendedCloudProbability;
        std::shared_ptr<ImageBool> &output_CM = generated_cloud_mask.cloudMask;

        try {
            WriteSingleChannelUint8(params.cloud_path(), cast<unsigned int>(output_CM, 1u, 0u));
        } catch (std::runtime_error const &e) {
            throw std::runtime_error(
                    fmt::format("Failed to write cloud mask. Path: {}. Message: {}", params.cloud_path(),
                                e.what()));
        }

        // Because shadow detection can be a slow process, we provide a way for the user to skip it if too many of the pixels contain shadows
        if (skipShadowDetection.decision) {
            f64 percent = static_cast<f64>(output_CM->cast<int>().sum()) / static_cast<f64>(output_CM->size());
            if (percent >= skipShadowDetection.threshold) {
                return;
            }
        }

        spdlog::debug(" --- Cloud Partitioning...");
        // Using the Cloud mask, partition it into individual clouds with collections and a map
        PartitionCloudMaskReturn PartitionCloudMask_Return
                = PartitionCloudMask(output_CM, diagonal_distance, MinimimumCloudSizeForRayCasting);
        CloudQuads &Clouds = PartitionCloudMask_Return.clouds;
        std::shared_ptr<ImageInt> &CloudsMap = PartitionCloudMask_Return.map;

        spdlog::debug(" --- Potential Shadow Mask Generation...");
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

        spdlog::debug(" --- Solving for Sun and Satellite Position...");
        // Generate a Vector grid for each
        std::shared_ptr<VectorGrid> SunVectorGrid
                = GenerateVectorGrid(toRadians(data_SunZenith), toRadians(data_SunAzimuth));
        std::shared_ptr<VectorGrid> ViewVectorGrid
                = GenerateVectorGrid(toRadians(data_ViewZenith), toRadians(data_ViewAzimuth));
        LMSPointReturn SunLSPointEqualTo_Return
                = LSPointEqualTo(SunVectorGrid, diagonal_distance, DistanceToSun);
        glm::vec3 &SunPosition = SunLSPointEqualTo_Return.p;
        LMSPointReturn ViewLSPointEqualTo_Return
                = LSPointEqualTo(ViewVectorGrid, diagonal_distance, DistanceToView);
        glm::vec3 &ViewPosition = ViewLSPointEqualTo_Return.p;
        float output_MDPSun = AverageDotProduct(SunVectorGrid, diagonal_distance, SunPosition);
        float output_MDPView = AverageDotProduct(ViewVectorGrid, diagonal_distance, ViewPosition);

        spdlog::debug(" --- Object-based Shadow Mask Generation...");
        // Solve for the optimal shadow matching results per cloud
        MatchCloudsShadowsResults MatchCloudsShadows_Return = MatchCloudsShadows(
                Clouds, CloudsMap, output_CM, output_PSM, diagonal_distance, SunPosition, ViewPosition
        );
        std::map<int, OptimalSolution> &OptimalCloudCastingSolutions
                = MatchCloudsShadows_Return.solutions;
        ShadowQuads &CloudCastedShadows = MatchCloudsShadows_Return.shadows;
        std::shared_ptr<ImageBool> &output_OSM = MatchCloudsShadows_Return.shadowMask;
        float &TrimmedMeanCloudHeight = MatchCloudsShadows_Return.trimmedMeanHeight;

        spdlog::debug(" --- Generating Probability Function...");
        // Generate the Alpha and Beta maps to produce the probability surface
        std::shared_ptr<ImageFloat> output_Alpha = ProbabilityRefinement::AlphaMap(DeltaNIR);
        std::shared_ptr<ImageFloat> output_Beta = ProbabilityRefinement::BetaMap(
                CloudCastedShadows,
                OptimalCloudCastingSolutions,
                output_CM,
                output_OSM,
                BlendedCloudProbability,
                diagonal_distance
        );
        UniformProbabilitySurface ProbabilityFunction
                = ProbabilityMap(output_OSM, output_Alpha, output_Beta);

        spdlog::debug(" --- Final Shadow Mask Generation...");
        std::shared_ptr<ImageBool> output_FSM = ImprovedShadowMask(
                output_OSM,
                output_CM,
                output_Alpha,
                output_Beta,
                ProbabilityFunction,
                ProbabilityFunctionThreshold
        );
        spdlog::debug("...Finished Algorithm.");

        spdlog::debug("Saving shadow results");
        try {
            WriteSingleChannelUint8(params.shadow_potential_path(), cast<u32>(output_PSM, 1u, 0u));
        } catch (std::runtime_error const &e) {
            throw std::runtime_error(
                    fmt::format("Failed to write potential shadow mask. Path: {}. Message: {}", params.cloud_path(),
                                e.what()));
        }

        try {
            WriteSingleChannelUint8(params.shadow_potential_path(), cast<u32>(output_PSM, 1u, 0u));
        } catch (std::runtime_error const &e) {
            throw std::runtime_error(
                    fmt::format("Failed to write potential shadow mask. Path: {}. Message: {}", params.cloud_path(),
                                e.what()));
        }

        try {
            WriteSingleChannelUint8(params.object_based_shadow_path(), cast<u32>(output_OSM, 1u, 0u));
        } catch (std::runtime_error const &e) {
            throw std::runtime_error(
                    fmt::format("Failed to write object-based shadow mask. Path: {}. Message: {}", params.cloud_path(),
                                e.what()));
        }

        try {
            WriteSingleChannelUint8(params.shadow_path(), cast<u32>(output_FSM, 1u, 0u));
        } catch (std::runtime_error const &e) {
            throw std::runtime_error(
                    fmt::format("Failed to write potential shadow mask. Path: {}. Message: {}", params.cloud_path(),
                                e.what()));
        }
    }

    enum DirectoryContents {
        NoSatelliteData,
        MultiSpectral,
        Radar
    };

    DirectoryContents find_directory_contents(fs::path const &path) {
        boost::regex expr{R"(\d{4}-\d{2}-\d{2})"};
        if (!boost::regex_match(path.filename().string(), expr)) {
            return DirectoryContents::NoSatelliteData;
        }
        fs::path candidate_path = path / fs::path("B04.tif");
        if (fs::exists(candidate_path)) {
            return DirectoryContents::MultiSpectral;
        } else {
            return DirectoryContents::Radar;
        }
    }

    void detect_in_folder(fs::path folder_path, f32 diagonal_distance, SkipShadowDetection skipShadowDetection,
                          bool use_cache) {
        std::vector<fs::path> directories;
        for (auto const &folder: fs::directory_iterator(folder_path)) {
            if (fs::is_directory(folder) && find_directory_contents(folder) == DirectoryContents::MultiSpectral) {
                directories.push_back(folder);
            }
        }

        spdlog::debug("Starting calculation");
        spdlog::stopwatch sw;
        for (size_t i = 0; i < directories.size(); ++i) {
            CloudParams params;
            params.nir_path = directories[i] / fs::path("B08.tif");
            params.clp_path = directories[i] / fs::path("CLP.tif");
            params.cld_path = directories[i] / fs::path("CLD.tif");
            params.scl_path = directories[i] / fs::path("SCL.tif");
            params.rgb_path = directories[i] / fs::path("RGB.tif");
            params.view_zenith_path = directories[i] / fs::path("viewZenithMean.tif");
            params.view_azimuth_path = directories[i] / fs::path("viewAzimuthMean.tif");
            params.sun_zenith_path = directories[i] / fs::path("sunZenithAngles.tif");
            params.sun_azimuth_path = directories[i] / fs::path("sunAzimuthAngles.tif");

            detect(params, diagonal_distance, skipShadowDetection, use_cache);
        }
        spdlog::info("Finished computing");
        spdlog::debug("Finished in {}", sw);
    }
}