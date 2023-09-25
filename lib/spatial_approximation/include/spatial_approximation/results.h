#pragma once

#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace spatial_approximation {
struct Status;

bool write_results_to_db(fs::path const& base_folder, std::unordered_map<std::string, Status> const& results);
}