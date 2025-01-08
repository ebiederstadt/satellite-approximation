#pragma once

#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>
#include <string_view>

namespace fs = std::filesystem;

namespace utils {
std::shared_ptr<spdlog::logger> create_logger(std::string const& name);
fs::path log_location();
}
