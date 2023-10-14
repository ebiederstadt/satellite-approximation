#pragma once

#include <utils/date.h>
#include <utils/db.h>

namespace fs = std::filesystem;
using namespace givde;

namespace remote_sensing {
struct Status {
    f64 percent_clouds = 0.0;
    std::optional<f64> percent_shadows;
    f64 percent_invalid = 0.0;
    bool clouds_computed = false;
    bool shadows_computed = false;
};

struct DataBase : utils::DataBase {
    explicit DataBase(fs::path path);
    void write_detection_results(std::unordered_map<utils::Date, Status> const& results);
};

std::unordered_map<utils::Date, Status> get_detection_results(fs::path base_folder);
}