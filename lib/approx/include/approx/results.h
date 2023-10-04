#pragma once

#include <filesystem>
#include <unordered_map>
#include <utils/date.h>

namespace fs = std::filesystem;

namespace approx {
struct Status;

bool write_results_to_db(fs::path const& base_folder, std::unordered_map<utils::Date, Status> const& results);
}