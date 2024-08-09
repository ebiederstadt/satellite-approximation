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

// Using https://github.com/fmtlib/fmt/issues/2865
template<>
struct fmt::formatter<std::filesystem::path> : formatter<std::string_view> {
    template<typename FormatContext>
    auto format(std::filesystem::path const& path, FormatContext& ctx)
    {
        return formatter<std::string_view>::format(path.string(), ctx);
    }
};
