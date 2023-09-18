// ---- Standard Library ---- //
#include <string>
#include <vector>

#define _USE_MATH_DEFINES

#include <cmath>

// ---- Thirdparty Libraries ---- //
#include <lyra/lyra.hpp>
#include <nlohmann/json.hpp>
#include <toml++/toml.h>
#include <spdlog/spdlog.h>

// ---- Project Files ---- //
#include "cloud_shadow_detection/CloudMask.h"
#include "cloud_shadow_detection/CloudShadowMatching.h"
#include "cloud_shadow_detection/ComputeEnvironment.h"
#include "cloud_shadow_detection/Functions.h"
#include "cloud_shadow_detection/GaussianBlur.h"
#include "cloud_shadow_detection/ImageOperations.h"
#include "cloud_shadow_detection/Imageio.h"
#include "cloud_shadow_detection/PitFillAlgorithm.h"
#include "cloud_shadow_detection/PotentialShadowMask.h"
#include "cloud_shadow_detection/ProbabilityRefinement.h"
#include "cloud_shadow_detection/ShadowMaskEvaluation.h"
#include "cloud_shadow_detection/VectorGridOperations.h"
#include "cloud_shadow_detection/SceneClassificationLayer.h"
#include "cloud_shadow_detection/types.h"

using namespace lyra;

using namespace Imageio;
using namespace ImageOperations;

using namespace SceneClassificationLayer;
using namespace CloudMask;
using namespace PotentialShadowMask;
using namespace CloudShadowMatching;
using namespace VectorGridOperations;
using namespace ProbabilityRefinement;

using namespace ShadowMaskEvaluation;

int main(int argc, char **argv) {
    spdlog::debug("Program Started...");
    bool help_ = false;
    Path data_path;
    Path output_path;
    bool use_gui = false;

    // Define the command line parser
    cli cli = help(help_) | opt(data_path, "data_path")["--data_path"]("Input Specs TOML file")
              | opt(output_path, "output_path")["--output_path"]("Output Specs TOML file")
              | opt(use_gui)["-g"]("Run the GUI");

    std::ostringstream helpMessage;
    helpMessage << cli;

    spdlog::debug("Parsing CLI...");
    parse_result result = cli.parse({argc, argv});

    spdlog::debug("Interpreting and loading CLI Input...");
    if (!result) {
        spdlog::error("Error in command line: {}", result.message());
        spdlog::error("CLI: {}", helpMessage.str());
        return EXIT_FAILURE;
    }
    if (help_) {
        spdlog::info("CLI: {}", helpMessage.str());
        return EXIT_SUCCESS;
    }

    if (!exists(data_path)) {
        spdlog::error("Input path does not exist");
        return EXIT_FAILURE;
    }
    if (!is_regular_file(data_path)) {
        spdlog::error(
                "The provided data path must be a folder or a TOML file: {}", data_path.string()
        );
        spdlog::error("CLI: {}", helpMessage.str());
        return EXIT_FAILURE;
    }
    if (!data_path.has_extension()) {
        spdlog::error(
                "The provided data path must be a folder or a TOML file: {}", data_path.string()
        );
        spdlog::error("CLI: {}", helpMessage.str());
        return EXIT_FAILURE;
    }
    if (data_path.extension().compare(".toml") != 0) {
        spdlog::error(
                "The provided data path must be a folder or a TOML file: {}", data_path.string()
        );
        spdlog::error("CLI: {}", helpMessage.str());
        return EXIT_FAILURE;
    }

    SupressLibTIFF();

    toml::table data_file_table = toml::parse_file(data_path.string());
    toml::table *data_table_ptr;
    try {
        data_table_ptr = data_file_table.get_as<toml::table>("Data");
        if (!data_table_ptr) throw std::runtime_error("Invalid input");
        spdlog::info("Loaded input data: {}", data_path.string());
    } catch (...) {
        spdlog::error("Input data toml file error occured: {}", data_path.string());
        spdlog::error("CLI: {}", helpMessage.str());
        return EXIT_FAILURE;
    }

    toml::table output_file_table;
    toml::table *output_table_ptr = nullptr;
    try {
        if (output_path.empty()) throw std::runtime_error("Fail");
        if (!exists(output_path)) throw std::runtime_error("Fail");
        if (!is_regular_file(output_path)) throw std::runtime_error("Fail");
        if (!output_path.has_extension()) throw std::runtime_error("Fail");
        if (output_path.extension().compare(".toml") != 0) throw std::runtime_error("Fail");
        output_file_table = toml::parse_file(output_path.string());
        output_table_ptr = output_file_table.get_as<toml::table>("Output");
        if (!output_table_ptr) throw std::runtime_error("Fail");
        spdlog::info("Loaded output data {}: ", output_path.string());
    } catch (...) {
        spdlog::warn("Failure Occured when loading output, will attempt to use data file.");
        output_table_ptr = output_file_table.get_as<toml::table>("Output");
        if (output_table_ptr) {
            spdlog::info("Loaded output data {}: ", data_path.string());
        } else {
            spdlog::warn("Failed to load an output file, no output will be generated.");
        }
    }

    spdlog::debug("Reading Input TOML file for Data...");
    toml::table &data_table = *data_table_ptr;
    // Get the data set ID
    std::string data_id = data_table["ID"].value_or<std::string>("");
    if (data_id.empty()) {
        spdlog::error("No data ID provided");
        return EXIT_FAILURE;
    }

    // Get the 'bbox' array from the nested table.
    auto tomlArray = data_table.get_as<toml::array>("bbox");
    if (!tomlArray) {
        spdlog::error("Bounding Box not supplied");
        return EXIT_FAILURE;
    }
    if (tomlArray->size() != 4) {
        spdlog::error("Bounding Box improperly specified");
        return EXIT_FAILURE;
    }
    float data_diagonal_distance;
    try {
        data_diagonal_distance = Functions::distance(
                {tomlArray->get_as<double>(0)->get(), tomlArray->get_as<double>(1)->get()},
                {tomlArray->get_as<double>(2)->get(), tomlArray->get_as<double>(3)->get()}
        );
    } catch (...) {
        spdlog::error("Bounding Box improperly specified");
        return EXIT_FAILURE;
    }

    // Load the NIR band of the data set
    Path data_NIR_path = Path(data_table["NIR_path"].value_or<std::string>(""));
    if (data_NIR_path.empty()) {
        spdlog::error("No NIR path provided");
        return EXIT_FAILURE;
    }
    std::shared_ptr<ImageFloat> data_NIR;
    try {
        data_NIR = normalize(
                ReadSingleChannelUint16(data_NIR_path), std::numeric_limits<uint16_t>::max()
        );
    } catch (...) {
        spdlog::error("Error reading NIR band from path: {}", data_NIR_path.string());
        return EXIT_FAILURE;
    }
    size_t data_Size = data_NIR->size();

    // Load the CLP band of the data set
    Path data_CLP_path = Path(data_table["CLP_path"].value_or<std::string>(""));
    if (data_CLP_path.empty()) {
        spdlog::error("No CLP path provided");
        return EXIT_FAILURE;
    }
    std::shared_ptr<ImageFloat> data_CLP;
    try {
        data_CLP
                = normalize(ReadSingleChannelUint8(data_CLP_path), std::numeric_limits<uint8_t>::max());
    } catch (...) {
        spdlog::error("Error reading CLP band from path: {}", data_CLP_path.string());
        return EXIT_FAILURE;
    }

    // Load the CLD band of the data set
    Path data_CLD_path = Path(data_table["CLD_path"].value_or<std::string>(""));
    if (data_CLD_path.empty()) {
        spdlog::error("No CLD path provided");
        return EXIT_FAILURE;
    }
    std::shared_ptr<ImageFloat> data_CLD;
    try {
        data_CLD = normalize(ReadSingleChannelUint8(data_CLD_path), 100u);
    } catch (...) {
        spdlog::error("Error reading CLD band from path: {}", data_CLD_path.string());
        return EXIT_FAILURE;
    }

    // Load the View_zenith band of the data set
    Path data_ViewZenith_path = Path(data_table["ViewZenith_path"].value_or<std::string>(""));
    if (data_ViewZenith_path.empty()) {
        spdlog::error("No ViewZenith_path path provided");
        return EXIT_FAILURE;
    }
    std::shared_ptr<ImageFloat> data_ViewZenith;
    try {
        data_ViewZenith = ReadSingleChannelFloat(data_ViewZenith_path);
    } catch (...) {
        spdlog::error("Error reading ViewZenith band from path: {}", data_ViewZenith_path.string());
        return EXIT_FAILURE;
    }

    // Load the View_azimuth band of the data set
    Path data_ViewAzimuth_path = Path(data_table["ViewAzimuth_path"].value_or<std::string>(""));
    if (data_ViewAzimuth_path.empty()) {
        spdlog::error("No ViewAzimuth path provided");
        return EXIT_FAILURE;
    }
    std::shared_ptr<ImageFloat> data_ViewAzimuth;
    try {
        data_ViewAzimuth = ReadSingleChannelFloat(data_ViewAzimuth_path);
    } catch (...) {
        spdlog::error("Error reading ViewAzimuth band from path: {}", data_ViewAzimuth_path.string());
        return EXIT_FAILURE;
    }

    // Load the Sun_zenith band of the data set
    Path data_SunZenith_path = Path(data_table["SunZenith_path"].value_or<std::string>(""));
    if (data_SunZenith_path.empty()) {
        spdlog::error("No SunZenith_path path provided");
        return EXIT_FAILURE;
    }
    std::shared_ptr<ImageFloat> data_SunZenith;
    try {
        data_SunZenith = ReadSingleChannelFloat(data_SunZenith_path);
    } catch (...) {
        spdlog::error("Error reading SunZenith band from path: {}", data_SunZenith_path.string());
        return EXIT_FAILURE;
    }

    // Load the Sun_azimuth band of the data set
    Path data_SunAzimuth_path = Path(data_table["SunAzimuth_path"].value_or<std::string>(""));
    if (data_SunAzimuth_path.empty()) {
        spdlog::error("No SunAzimuth path provided");
        return EXIT_FAILURE;
    }
    std::shared_ptr<ImageFloat> data_SunAzimuth;
    try {
        data_SunAzimuth = ReadSingleChannelFloat(data_SunAzimuth_path);
    } catch (...) {
        spdlog::error("Error reading SunAzimuth band from path: {}", data_SunAzimuth_path.string());
        return EXIT_FAILURE;
    }

    // Load the SCL band of the data set
    Path data_SCL_path = Path(data_table["SCL_path"].value_or<std::string>(""));
    if (data_SCL_path.empty()) {
        spdlog::error("No SCL path provided");
        return EXIT_FAILURE;
    }
    std::shared_ptr<ImageUint> data_SCL;
    try {
        data_SCL = ReadSingleChannelUint8(data_SCL_path);
    } catch (...) {
        spdlog::error("Error reading SCL band from path: {}", data_SCL_path.string());
        return EXIT_FAILURE;
    }

    // Load the RBGA band of the data set
    Path data_RBGA_path = Path(data_table["RBGA_path"].value_or<std::string>(""));
    std::shared_ptr<ImageUint> data_RBGA
            = std::make_shared<ImageUint>(data_NIR->rows(), data_NIR->cols());
    if (data_RBGA_path.empty()) {
        spdlog::warn("No RBGA path provided");
    } else {
        try {
            data_RBGA = ReadRGBA(data_RBGA_path);
        } catch (...) {
            spdlog::warn("Error reading RBGA band from path: {}", data_RBGA_path.string());
        }
    }

    // Load the Shadowbaseline band of the data set
    Path data_ShadowBaseline_path
            = Path(data_table["ShadowBaseline_path"].value_or<std::string>(""));
    std::shared_ptr<ImageBool> data_ShadowBaseline
            = std::make_shared<ImageBool>(data_NIR->rows(), data_NIR->cols());
    if (data_ShadowBaseline_path.empty()) {
        spdlog::warn("No Baseline path provided");
    } else {
        try {
            std::vector<float> data_ShadowBaseline_raw
                    = ImageOperations::decomposeRBGA(ReadRGBA(data_ShadowBaseline_path));
            glm::vec3 true_value = {0.f, 1.f, 0.f};
            for (int i = 0; i < data_ShadowBaseline->size(); i++) {
                data_ShadowBaseline->data()[i]
                        = Functions::equal(true_value.x, data_ShadowBaseline_raw[4 * i + 0], 1e-8)
                          && Functions::equal(true_value.y, data_ShadowBaseline_raw[4 * i + 1], 1e-8)
                          && Functions::equal(true_value.z, data_ShadowBaseline_raw[4 * i + 2], 1e-8);
            }
        } catch (...) {
            spdlog::warn(
                    "Error reading RBGA band from path: {}", data_ShadowBaseline_path.string()
            );
            data_ShadowBaseline_path = "";
            data_ShadowBaseline = nullptr;
        }
    }

    //---------------------------------------------------------------------------------------------------
    Path output_CM_path, output_PSM_path, output_OSM_path, output_FSM_path, output_Alpha_path,
            output_Beta_path, output_PSME_path, output_OSME_path, output_FSME_path,
            output_EvaluationMetric_path;
    if (output_table_ptr) {
        toml::table &output_table = *output_table_ptr;
        spdlog::debug("Reading Output TOML file for Data...");
        output_CM_path = Path(output_table["CM_path"].value_or<std::string>(""));
        if (output_CM_path.empty()) {
            spdlog::warn("No CM path provided");
        } else {
            try {
                if (output_CM_path.extension().compare(".tif") != 0) {
                    spdlog::warn("CM path provided is invalid: {}", output_CM_path.string());
                    output_CM_path = "";
                }
            } catch (...) {
                spdlog::warn("CM path provided is invalid: {}", output_CM_path.string());
                output_CM_path = "";
            }
        }

        output_PSM_path = Path(output_table["PSM_path"].value_or<std::string>(""));
        if (output_PSM_path.empty()) {
            spdlog::warn("No PSM path provided");
        } else {
            try {
                if (output_PSM_path.extension().compare(".tif") != 0) {
                    spdlog::warn("PSM path provided is invalid: {}", output_PSM_path.string());
                    output_PSM_path = "";
                }
            } catch (...) {
                spdlog::warn("PSM path provided is invalid: {}", output_PSM_path.string());
                output_PSM_path = "";
            }
        }

        output_OSM_path = Path(output_table["OSM_path"].value_or<std::string>(""));
        if (output_OSM_path.empty()) {
            spdlog::warn("No OSM path provided");
        } else {
            try {
                if (output_OSM_path.extension().compare(".tif") != 0) {
                    spdlog::warn("OSM path provided is invalid: {}", output_OSM_path.string());
                    output_OSM_path = "";
                }
            } catch (...) {
                spdlog::warn("OSM path provided is invalid: {}", output_OSM_path.string());
                output_OSM_path = "";
            }
        }

        output_FSM_path = Path(output_table["FSM_path"].value_or<std::string>(""));
        if (output_FSM_path.empty()) {
            spdlog::warn("No FSM path provided");
        } else {
            try {
                if (output_FSM_path.extension().compare(".tif") != 0) {
                    spdlog::warn("FSM path provided is invalid: {}", output_FSM_path.string());
                    output_FSM_path = "";
                }
            } catch (...) {
                spdlog::warn("FSM path provided is invalid: {}", output_FSM_path.string());
                output_FSM_path = "";
            }
        }

        output_Alpha_path = Path(output_table["Alpha_path"].value_or<std::string>(""));
        if (output_Alpha_path.empty()) {
            spdlog::warn("No Alpha path provided");
        } else {
            try {
                if (output_Alpha_path.extension().compare(".tif") != 0) {
                    spdlog::warn("Alpha path provided is invalid: {}", output_Alpha_path.string());
                    output_Alpha_path = "";
                }
            } catch (...) {
                spdlog::warn("Alpha path provided is invalid: {}", output_Alpha_path.string());
                output_Alpha_path = "";
            }
        }

        output_Beta_path = Path(output_table["Beta_path"].value_or<std::string>(""));
        if (output_Beta_path.empty()) {
            spdlog::warn("No Beta path provided");
        } else {
            try {
                if (output_Beta_path.extension().compare(".tif") != 0) {
                    spdlog::warn("Beta path provided is invalid: {}", output_Beta_path.string());
                    output_Beta_path = "";
                }
            } catch (...) {
                spdlog::warn("Beta path provided is invalid: {}", output_Beta_path.string());
                output_Beta_path = "";
            }
        }

        output_PSME_path = Path(output_table["PSME_path"].value_or<std::string>(""));
        if (output_PSME_path.empty()) {
            spdlog::warn("No PSME path provided");
        } else {
            try {
                if (output_PSME_path.extension().compare(".tif") != 0) {
                    spdlog::warn("PSME path provided is invalid: {}", output_PSME_path.string());
                    output_PSME_path = "";
                }
            } catch (...) {
                spdlog::warn("PSME path provided is invalid: {}", output_PSME_path.string());
                output_PSME_path = "";
            }
        }

        output_OSME_path = Path(output_table["OSME_path"].value_or<std::string>(""));
        if (output_OSME_path.empty()) {
            spdlog::warn("No OSME path provided");
        } else {
            try {
                if (output_OSME_path.extension().compare(".tif") != 0) {
                    spdlog::warn("OSME path provided is invalid: {}", output_OSME_path.string());
                    output_OSME_path = "";
                }
            } catch (...) {
                spdlog::warn("OSME path provided is invalid: {}", output_OSME_path.string());
                output_OSME_path = "";
            }
        }

        output_FSME_path = Path(output_table["FSME_path"].value_or<std::string>(""));
        if (output_FSME_path.empty()) {
            spdlog::warn("No FSME path provided");
        } else {
            try {
                if (output_FSME_path.extension().compare(".tif") != 0) {
                    spdlog::warn("FSME path provided is invalid: {}", output_FSME_path.string());
                    output_FSME_path = "";
                }
            } catch (...) {
                spdlog::warn("FSME path provided is invalid: {}", output_FSME_path.string());
                output_FSME_path = "";
            }
        }

        output_EvaluationMetric_path
                = Path(output_table["EvaluationMetric_path"].value_or<std::string>(""));
        if (output_EvaluationMetric_path.empty()) {
            spdlog::warn("No EvaluationMetric path provided");
        } else {
            try {
                if (output_EvaluationMetric_path.extension().compare(".json") != 0) {
                    spdlog::warn(
                            "EvaluationMetric path provided is invalid: {}",
                            output_EvaluationMetric_path.string()
                    );
                    output_EvaluationMetric_path = "";
                }
            } catch (...) {
                spdlog::warn(
                        "EvaluationMetric path provided is invalid: {}",
                        output_EvaluationMetric_path.string()
                );
                output_EvaluationMetric_path = "";
            }
        }
    }

    spdlog::debug("Initialing Computing Context...");

    ComputeEnvironment::InitMainContext();
    GaussianBlur::init();
    PitFillAlgorithm::init();

    spdlog::debug("Running Algorithm...");

    // The process calculations ----------------------------------

    const int MinimimumCloudSizeForRayCasting = 3;
    const float DistanceToSun = 1.5e9f;
    const float DistanceToView = 785.f;
    const float ProbabilityFunctionThreshold = .15f;

    spdlog::debug(" --- Cloud Detection...");
    // Generate the Cloud mask along with the intermediate result of the blended cloud probability
    GenerateCloudMaskReturn GenerateCloudMask_Return
            = GenerateCloudMask(data_CLP, data_CLD, data_SCL);
    std::shared_ptr<ImageFloat> &BlendedCloudProbability
            = GenerateCloudMask_Return.blendedCloudProbability;
    std::shared_ptr<ImageBool> &output_CM = GenerateCloudMask_Return.cloudMask;

    spdlog::debug(" --- Cloud Partitioning...");
    // Using the Cloud mask, partition it into individual clouds with collections and a map
    PartitionCloudMaskReturn PartitionCloudMask_Return
            = PartitionCloudMask(output_CM, data_diagonal_distance, MinimimumCloudSizeForRayCasting);
    CloudQuads &Clouds = PartitionCloudMask_Return.clouds;
    std::shared_ptr<ImageInt> &CloudsMap = PartitionCloudMask_Return.map;

    spdlog::debug(" --- Potential Shadow Mask Generation...");
    // Generate the Candidate (or Potential) Shadow Mask
    PotentialShadowMaskGenerationReturn GeneratePotentialShadowMask_Return
            = GeneratePotentialShadowMask(data_NIR, output_CM, data_SCL);
    std::shared_ptr<ImageBool> output_PSM = GeneratePotentialShadowMask_Return.mask;
    std::shared_ptr<ImageFloat> DeltaNIR
            = GeneratePotentialShadowMask_Return.difference_of_pitfill_NIR;

    spdlog::debug(" --- Solving for Sun and Satellite Position...");
    // Generate a Vector grid for each
    std::shared_ptr<VectorGrid> SunVectorGrid
            = GenerateVectorGrid(toRadians(data_SunZenith), toRadians(data_SunAzimuth));
    std::shared_ptr<VectorGrid> ViewVectorGrid
            = GenerateVectorGrid(toRadians(data_ViewZenith), toRadians(data_ViewAzimuth));
    LMSPointReturn SunLSPointEqualTo_Return
            = LSPointEqualTo(SunVectorGrid, data_diagonal_distance, DistanceToSun);
    glm::vec3 &SunPosition = SunLSPointEqualTo_Return.p;
    LMSPointReturn ViewLSPointEqualTo_Return
            = LSPointEqualTo(ViewVectorGrid, data_diagonal_distance, DistanceToView);
    glm::vec3 &ViewPosition = ViewLSPointEqualTo_Return.p;
    float output_MDPSun = AverageDotProduct(SunVectorGrid, data_diagonal_distance, SunPosition);
    float output_MDPView = AverageDotProduct(ViewVectorGrid, data_diagonal_distance, ViewPosition);

    spdlog::debug(" --- Object-based Shadow Mask Generation...");
    // Solve for the optimal shadow matching results per cloud
    MatchCloudsShadowsResults MatchCloudsShadows_Return = MatchCloudsShadows(
            Clouds, CloudsMap, output_CM, output_PSM, data_diagonal_distance, SunPosition, ViewPosition
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
            data_diagonal_distance
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
    spdlog::debug("Evaluating data...");
    ImageBounds output_EvaluationBounds = CastedImageBounds(
            output_PSM, data_diagonal_distance, SunPosition, ViewPosition, TrimmedMeanCloudHeight
    );
    Results PSM_results
            = Evaluate(output_PSM, output_CM, data_ShadowBaseline, output_EvaluationBounds);
    std::shared_ptr<ImageUint> &output_PSME = PSM_results.pixel_classes;
    Results OSM_results
            = Evaluate(output_OSM, output_CM, data_ShadowBaseline, output_EvaluationBounds);
    std::shared_ptr<ImageUint> &output_OSME = OSM_results.pixel_classes;
    Results FSM_results
            = Evaluate(output_FSM, output_CM, data_ShadowBaseline, output_EvaluationBounds);
    std::shared_ptr<ImageUint> &output_FSME = FSM_results.pixel_classes;

    spdlog::debug("Writing Output According to Output TOML file...");
    if (!output_CM_path.empty()) {
        try {
            WriteSingleChannelUint8(output_CM_path, cast<unsigned int>(output_CM, 1u, 0u));
        } catch (...) { spdlog::error("Failed to Write CM tif"); }
    }
    if (!output_PSM_path.empty()) {
        try {
            WriteSingleChannelUint8(output_PSM_path, cast<unsigned int>(output_PSM, 1u, 0u));
        } catch (...) { spdlog::error("Failed to Write PSM tif"); }
    }
    if (!output_OSM_path.empty()) {
        try {
            WriteSingleChannelUint8(output_OSM_path, cast<unsigned int>(output_OSM, 1u, 0u));
        } catch (...) { spdlog::error("Failed to Write OSM tif"); }
    }
    if (!output_FSM_path.empty()) {
        try {
            WriteSingleChannelUint8(output_FSM_path, cast<unsigned int>(output_FSM, 1u, 0u));
        } catch (...) { spdlog::error("Failed to Write FSM tif"); }
    }
    if (!output_Alpha_path.empty()) {
        try {
            WriteSingleChannelFloat(output_Alpha_path, output_Alpha);
        } catch (...) { spdlog::error("Failed to Write Alpha tif"); }
    }
    if (!output_Beta_path.empty()) {
        try {
            WriteSingleChannelFloat(output_Beta_path, output_Beta);
        } catch (...) { spdlog::error("Failed to Write Beta tif"); }
    }
    if (!output_PSME_path.empty()) {
        try {
            WriteSingleChannelUint8(output_PSME_path, output_PSME);
        } catch (...) { spdlog::error("Failed to Write PSME tif"); }
    }
    if (!output_OSME_path.empty()) {
        try {
            WriteSingleChannelUint8(output_OSME_path, output_OSME);
        } catch (...) { spdlog::error("Failed to Write OSME tif"); }
    }
    if (!output_FSME_path.empty()) {
        try {
            WriteSingleChannelUint8(output_FSME_path, output_FSME);
        } catch (...) { spdlog::error("Failed to Write FSME tif"); }
    }
    if (!output_EvaluationMetric_path.empty()) {
        nlohmann::json evaluation_json;
        evaluation_json["ID"] = data_id;
        evaluation_json["Baselined"] = !data_ShadowBaseline_path.empty();
        evaluation_json["Sun"]["Average Dot Product"] = output_MDPSun;
        evaluation_json["View"]["Average Dot Product"] = output_MDPView;
        evaluation_json["Bounds"]["x"]["min"] = output_EvaluationBounds.p0.x;
        evaluation_json["Bounds"]["x"]["max"] = output_EvaluationBounds.p1.x;
        evaluation_json["Bounds"]["y"]["min"] = output_EvaluationBounds.p0.y;
        evaluation_json["Bounds"]["y"]["max"] = output_EvaluationBounds.p1.y;

        evaluation_json["Potential Shadow Mask"]["Users Accuracy"] = PSM_results.users_accuracy;
        evaluation_json["Potential Shadow Mask"]["Producers Accuracy"]
                = PSM_results.producers_accuracy;
        evaluation_json["Potential Shadow Mask"]["False Positives Relative to Total Pixels"]
                = PSM_results.positive_error_total;
        evaluation_json["Potential Shadow Mask"]["False Negatives Relative to Total Pixels"]
                = PSM_results.negative_error_total;
        evaluation_json["Potential Shadow Mask"]["False Pixels Relative to Total Pixels"]
                = PSM_results.error_total;
        evaluation_json["Potential Shadow Mask"]["False Positives Relative to Shadow Pixels"]
                = PSM_results.positive_error_relative;
        evaluation_json["Potential Shadow Mask"]["False Negatives Relative to Shadow Pixels"]
                = PSM_results.negative_error_relative;
        evaluation_json["Potential Shadow Mask"]["False Pixels Relative to Shadow Pixels"]
                = PSM_results.error_relative;

        evaluation_json["Object-based Shadow Mask"]["Users Accuracy"] = OSM_results.users_accuracy;
        evaluation_json["Object-based Shadow Mask"]["Producers Accuracy"]
                = OSM_results.producers_accuracy;
        evaluation_json["Object-based Shadow Mask"]["False Positives Relative to Total Pixels"]
                = OSM_results.positive_error_total;
        evaluation_json["Object-based Shadow Mask"]["False Negatives Relative to Total Pixels"]
                = OSM_results.negative_error_total;
        evaluation_json["Object-based Shadow Mask"]["False Pixels Relative to Total Pixels"]
                = OSM_results.error_total;
        evaluation_json["Object-based Shadow Mask"]["False Positives Relative to Shadow Pixels"]
                = OSM_results.positive_error_relative;
        evaluation_json["Object-based Shadow Mask"]["False Negatives Relative to Shadow Pixels"]
                = OSM_results.negative_error_relative;
        evaluation_json["Object-based Shadow Mask"]["False Pixels Relative to Shadow Pixels"]
                = OSM_results.error_relative;

        evaluation_json["Final Shadow Mask"]["Users Accuracy"] = FSM_results.users_accuracy;
        evaluation_json["Final Shadow Mask"]["Producers Accuracy"] = FSM_results.producers_accuracy;
        evaluation_json["Final Shadow Mask"]["False Positives Relative to Total Pixels"]
                = FSM_results.positive_error_total;
        evaluation_json["Final Shadow Mask"]["False Negatives Relative to Total Pixels"]
                = FSM_results.negative_error_total;
        evaluation_json["Final Shadow Mask"]["False Pixels Relative to Total Pixels"]
                = FSM_results.error_total;
        evaluation_json["Final Shadow Mask"]["False Positives Relative to Shadow Pixels"]
                = FSM_results.positive_error_relative;
        evaluation_json["Final Shadow Mask"]["False Negatives Relative to Shadow Pixels"]
                = FSM_results.negative_error_relative;
        evaluation_json["Final Shadow Mask"]["False Pixels Relative to Shadow Pixels"]
                = FSM_results.error_relative;
        try {
            std::ofstream outputFile(output_EvaluationMetric_path);
            outputFile << evaluation_json;
            outputFile.close();
        } catch (...) { spdlog::error("Failed to Write Evaluation JSON"); }
    }
    return EXIT_SUCCESS;
}