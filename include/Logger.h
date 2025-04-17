#ifndef LOGGER_H
#define LOGGER_H

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <memory>
#include <vector>
#include <string>

// Note: Use the following pattern for logging with function information:
// getLogger()->debug("[{}] Your message", __PRETTY_FUNCTION__);
// This will automatically include the function name in the log message

// Implementation of getLogger function
inline std::shared_ptr<spdlog::logger>& getLogger() {
    static std::shared_ptr<spdlog::logger> logger = nullptr;
    if (!logger) {
        // Define the pattern we want to use (without logger name)
        std::string pattern = "%T [%^%l%$] %v";
        
        // Create a color console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern(pattern);
        
        // Create a basic file sink 
        constexpr bool truncate_mode = true; // false = append, true = truncate
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/machine_controller.log", truncate_mode);
        file_sink->set_pattern(pattern);
        
        // Combine sinks into a vector
        std::vector<spdlog::sink_ptr> sinks { console_sink, file_sink };
        
        // Create a logger with multiple sinks
        logger = std::make_shared<spdlog::logger>("machineController", sinks.begin(), sinks.end());
        logger->set_pattern(pattern); // Also set pattern on the logger itself
        spdlog::register_logger(logger);
        
        // Optionally set log level and flush behavior
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::debug);
    }
    return logger;
}

#endif // LOGGER_H



// usage:
// #include "Logger.h"
// getLogger()->warn("Logic thread about to start.");
// getLogger()->info("info from main.cpp");
// getLogger()->warn("[{}] Warning: something happened", __PRETTY_FUNCTION__);
// getLogger()->flush(); 
