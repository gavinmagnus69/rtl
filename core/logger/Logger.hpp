#pragma once


#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>
#include <vector>

namespace core::logger {

static const std::filesystem::path g_defaultLogsFilePath = "logs/log.txt";

class Logger {
public:
    Logger() {
        if (!std::filesystem::exists(g_defaultLogsFilePath.parent_path())) {
            std::filesystem::create_directories(g_defaultLogsFilePath.parent_path());
        }
        initLogger(g_defaultLogsFilePath);
    };
    Logger(const std::filesystem::path& logsFilePath) {
        if (!std::filesystem::exists(logsFilePath.parent_path())) {
            std::filesystem::create_directories(logsFilePath.parent_path());
        }
        initLogger(logsFilePath);
    };
    ~Logger() = default;
private:
    void initLogger(const std::filesystem::path& logsFilePath) {
        try {
            static constexpr const char* kLoggerName = "core_file_logger";

            if (auto existing = spdlog::get(kLoggerName)) {
                spdlog::drop(kLoggerName);
            }
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logsFilePath.string(), true);
            std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
            auto logger = std::make_shared<spdlog::logger>(kLoggerName, sinks.begin(), sinks.end());
            logger->set_level(spdlog::level::info);
            logger->flush_on(spdlog::level::warn);
            logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
            spdlog::register_logger(logger);
            spdlog::set_default_logger(logger);
            spdlog::info("Logger initialized with file: {}", logsFilePath.string());
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        }
    };
};
}; // namespace core::logger
