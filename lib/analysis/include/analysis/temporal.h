#pragma once

#include <vector>
#include <filesystem>

#include "forward.h"

namespace fs = std::filesystem;

namespace analysis {
void compute_indices_for_all_dates(std::vector<fs::path> const& folders_to_process, Indices index, DataBase &db, DataChoices choices);
}
