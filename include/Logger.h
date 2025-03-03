// Logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <memory>

inline std::shared_ptr<spdlog::logger>& getLogger() {
    static std::shared_ptr<spdlog::logger> logger = 
        spdlog::basic_logger_mt("machineController", "logs/machine_controller.log");
    return logger;
}

#endif // LOGGER_H


// usage:
// #include "Logger.h"
// getLogger()->warn("MainLogic thread about to start.");
// getLogger()->info("info from main.cpp");
// getLogger()->warn("[{}] Warning: something happened", __PRETTY_FUNCTION__);
// getLogger()->flush(); 
