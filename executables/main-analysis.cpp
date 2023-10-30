#include "analysis/noise.h"
#include <analysis/db.h>
#include <analysis/utils.h>
#include <filesystem>
#include <gdal/gdal_priv.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <utils/log.h>

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::debug);
    sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
    GDALAllRegister();
    spdlog::info("Log location: {}", utils::log_location());

    fs::path base_folder = "/home/ebiederstadt/Documents/sentinel_cache/bbox-111.9314176_56.921209032_-111.6817217_57.105787570/2019-05-22";
    analysis::DataBase db(base_folder.parent_path());
    analysis::remove_noise_in_clouds_and_shadows(base_folder, 100, false, db);
}