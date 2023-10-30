#include "utils/error.h"
#include "utils/log.h"

#include <sqlite3.h>

namespace utils {
IOError::IOError(std::string_view msg, fs::path path)
    : m_message(msg.data())
    , m_path(std::move(path))
{
}

IOError::IOError(std::string_view msg, fs::path path, spdlog::logger& logger)
    : m_message(msg.data())
    , m_path(std::move(path))
{
    logger.error("{} (path: {})", m_message, m_path);
}

DBError::DBError(std::string_view msg, int error_code)
    : m_message(msg.data())
    , m_error(error_code)
{
}

DBError::DBError(std::string_view msg, int error_code, spdlog::logger& logger)
    : m_message(msg.data())
    , m_error(error_code)
{
    logger.error("{} (Error {} ({}))", m_message, m_error, sqlite3_errstr(m_error));
}

GenericError::GenericError(std::string_view msg)
    : m_message(msg.data())
{
}

GenericError::GenericError(std::string_view msg, spdlog::logger& logger)
    : m_message(msg.data())
{
    logger.error("Error: {}", m_message);
}
}
