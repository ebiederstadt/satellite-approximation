#include <cloud_shadow_detection/CloudMask.h>
#include <cloud_shadow_detection/ComputeEnvironment.h>
#include <cloud_shadow_detection/GaussianBlur.h>
#include <cloud_shadow_detection/ImageOperations.h>
#include <cloud_shadow_detection/Imageio.h>
#include <cloud_shadow_detection/PitFillAlgorithm.h>
#include <cloud_shadow_detection/PotentialShadowMask.h>
#include <cloud_shadow_detection/automatic_detection.h>
#include <gdal_priv.h>
#include <cloud_shadow_detection/PotentialShadowMask.h>
#include <cloud_shadow_detection/automatic_detection.h>
#include <gdal/gdal_priv.h>
#include <utils/eigen.h>
#include <utils/geotiff.h>

using namespace CloudMask;
using namespace ImageOperations;
using namespace PotentialShadowMask;
using namespace Imageio;
using namespace PotentialShadowMask;

int main()
{
    ComputeEnvironment::InitMainContext();
    GaussianBlur::init();
    PitFillAlgorithm::init();

    GDALAllRegister();
    spdlog::info("Log location: {}", utils::log_location());

    // clang-format off
    std::array<f64, 4> bbox = { 56.92120903, -111.93141764,
                                57.10578757, -111.6817218 };
    // clang-format on

    fs::path base_folder = "/home/ebiederstadt/Documents/sentinel_cache/bbox-111.9314176_56.921209032_-111.6817217_57.105787570/2019-05-22";
    f32 diagonal_distance = remote_sensing::get_diagonal_distance(bbox[1], bbox[0], bbox[3], bbox[2]);
    remote_sensing::detect_single_folder(base_folder, diagonal_distance, {}, false);

    //    ImageFloat clp_data = normalize(utils::GeoTIFF<u8>(base_folder / "CLP.tif").values);
    //    ImageFloat cld_data = normalize<u8>(utils::GeoTIFF<u8>(base_folder / "CLD.tif").values, 100u);
    //    ImageUint scl_data = utils::GeoTIFF<u32>(base_folder / "SCL.tif").values;
    //    auto generated_cloud_mask = GenerateCloudMaskIgnoreLowProbability(clp_data, cld_data, scl_data);
    //
    //    ImageFloat nir_data = normalize(utils::GeoTIFF<u16>(base_folder / "B08.tif").values);
    //    auto potential_shadows = GeneratePotentialShadowMask(nir_data, generated_cloud_mask.cloudMaskNoProcessing, scl_data);
    //
    //    spdlog::info(utils::printable_stats(potential_shadows.difference_of_pitfill_NIR));
    //
    //    fs::path output_dir = base_folder / "testing_shadow_detection";
    //    utils::GeoTIFF<u16> output_tiff(base_folder / "B08.tif");
    //
    //    output_tiff.values = generated_cloud_mask.cloudMask.cast<u16>();
    //    output_tiff.write(output_dir / "cloud_mask.tif");
    //
    //    output_tiff.values = generated_cloud_mask.cloudMaskNoProcessing.cast<u16>();
    //    output_tiff.write(output_dir / "cloud_mask_simple.tif");
    //
    //    output_tiff.values = potential_shadows.mask.cast<u16>();
    //    output_tiff.write(output_dir / "potential_shadows.tif");
    //
    //    utils::GeoTIFF<f32> output(base_folder / "viewAzimuthMean.tif");
    //    output.values = potential_shadows.difference_of_pitfill_NIR;
    //    output.write(output_dir / "shadow_difference.tif");
    //
    //    output.values = potential_shadows.pitfill_result;
    //    output.write(output_dir / "nir_pitfill.tif");
}