#include <cloud_shadow_detection/CloudMask.h>
#include <cloud_shadow_detection/ComputeEnvironment.h>
#include <cloud_shadow_detection/GaussianBlur.h>
#include <cloud_shadow_detection/ImageOperations.h>
#include <cloud_shadow_detection/PitFillAlgorithm.h>
#include <gdal/gdal_priv.h>
#include <opencv2/core/eigen.hpp>
#include <utils/geotiff.h>
#include <utils/log.h>

using namespace ImageOperations;

int main()
{
    GDALAllRegister();
    spdlog::info("Log location: {}", utils::log_location());
    ComputeEnvironment::InitMainContext();
    GaussianBlur::init();
    PitFillAlgorithm::init();

    fs::path base_folder = "/home/ebiederstadt/Documents/sentinel_cache/bbox-111.9314176_56.921209032_-111.6817217_57.105787570/2019-05-22";
    utils::GeoTIFF<u8> tiff(base_folder / "CLD.tif");
    MatX<float> cld = normalize(tiff.values);
    MatX<float> clp = normalize(utils::GeoTIFF<u8>(base_folder / "CLP.tif").values);
    MatX<u32> scl = utils::GeoTIFF<u32>(base_folder / "SCL.tif").values;

    auto new_result = CloudMask::GenerateCloudMaskIgnoreLowProbability(clp, cld, scl);

    fs::path output_dir = base_folder / "testing_cloud_detection";
    if (!fs::exists(output_dir)) {
        spdlog::info("Creating directory");
        fs::create_directory(output_dir);
    }

    tiff.write(new_result.cloudMask, output_dir / "final_result.tif");
}