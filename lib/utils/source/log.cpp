#include "utils/log.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace utils {
std::shared_ptr<spdlog::logger> create_logger(std::string const& name)
{
    auto logger = spdlog::get(name);
    if (logger) {
        return logger;
    }

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::warn);
    console_sink->set_pattern("[%^%l%$] %v");

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(fmt::format("logs/main.log", name), true);
    file_sink->set_level(spdlog::level::trace);

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(console_sink);
    sinks.push_back(file_sink);
    logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    spdlog::register_logger(logger);
    logger->set_level(spdlog::level::debug);
    logger->info("Logger has been created and registered");

    return logger;
}
}