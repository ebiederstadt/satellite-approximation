#pragma once

#include <fmt/format.h>
#include <filesystem>

// Using https://github.com/fmtlib/fmt/issues/2865
template<>
struct fmt::formatter<std::filesystem::path> : formatter<std::string_view> {
    template<typename FormatContext>
    auto format(std::filesystem::path const& path, FormatContext& ctx)
    {
        return formatter<std::string_view>::format(path.string(), ctx);
    }
};
