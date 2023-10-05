#pragma once

#include "fmt_filesystem.h"

#include <exception>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <string_view>

namespace fs = std::filesystem;

namespace utils {
class IOError : public std::exception {
public:
    IOError(std::string_view msg, fs::path path)
        : m_message(msg.data())
        , m_path(path)
    {
    }

    IOError(std::string_view msg, fs::path path, spdlog::logger& logger)
        : m_message(msg.data())
        , m_path(path)
    {
        logger.error("{} (path: {})", m_message, m_path);
    }

    char const* what() const noexcept override
    {
        return m_message.c_str();
    }

    fs::path path() const
    {
        return m_path;
    }

private:
    std::string m_message;
    fs::path m_path;
};
}