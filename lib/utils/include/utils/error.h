#pragma once

#include <exception>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <string_view>

namespace fs = std::filesystem;

namespace utils {
class IOError : public std::exception {
public:
    IOError(std::string_view msg, fs::path path);
    IOError(std::string_view msg, fs::path path, spdlog::logger& logger);

    char const* what() const noexcept override { return m_message.c_str(); }
    fs::path path() const { return m_path; }

private:
    std::string m_message;
    fs::path m_path;
};

class DBError : public std::exception {
public:
    DBError(std::string_view msg, int error_code);
    DBError(std::string_view msg, int error_code, spdlog::logger& logger);

    char const* what() const noexcept override { return m_message.c_str(); }

private:
    std::string m_message;
    int m_error;
};

class GenericError : public std::exception {
public:
    GenericError(std::string_view msg);
    GenericError(std::string_view msg, spdlog::logger& logger);

    char const* what() const noexcept override { return m_message.c_str(); }

private:
    std::string m_message;
};
}