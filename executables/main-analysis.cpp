#include <analysis/sis.h>
#include <filesystem>
#include <gdal/gdal_priv.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>

namespace fs = std::filesystem;
using namespace givde;

int main(int argc, char **argv) {
    if (argc != 6) {
        spdlog::error("Usage: {} base_path start_year end_year index threshold", argv[0]);
        return -1;
    }

    sqlite3_config(SQLITE_CONFIG_MULTITHREAD);

    fs::path base_folder(argv[1]);
    int start_year = std::stoi(argv[2]);
    int end_year = std::stoi(argv[3]);
    auto index_or_none = analysis::from_str(argv[4]);
    if (!index_or_none.has_value()) {
        spdlog::error("Failed to map to provided index to a known index (tried {})", argv[4]);
        return -1;
    }
    auto index = index_or_none.value();
    f64 threshold = std::stod(argv[5]);
    analysis::DataChoices data_choices = analysis::UseRealData{};

    spdlog::set_level(spdlog::level::debug);
    GDALAllRegister();

    analysis::single_image_summary(base_folder, true, start_year, end_year, index, threshold, data_choices);
}