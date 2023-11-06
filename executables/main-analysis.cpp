#include <analysis/db.h>
#include <analysis/filters.h>
#include <analysis/utils.h>
#include <filesystem>
#include <gdal/gdal_priv.h>
#include <opencv2/highgui.hpp>
#include <spdlog/spdlog.h>
#include <utils/log.h>

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::debug);
    GDALAllRegister();
    spdlog::info("Log location: {}", utils::log_location());

    fs::path base_folder = "/home/ebiederstadt/Documents/sentinel_cache/bbox-111.9392593_56.936104843_-111.6770842_57.023933326/2019-09-13";

    utils::GeoTIFF<f64> vh(base_folder / "VH.tif");
    MatX<f64> result = analysis::frost_filter(vh.values, 7, 3.0);

    utils::GeoTIFF<f64> output(base_folder.parent_path() / "template.tif");
    output.setValues(result);
    output.write(base_folder / "frost.tif");
}