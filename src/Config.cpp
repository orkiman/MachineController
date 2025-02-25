#include "Config.h"
#include <fstream>
#include <stdexcept>

Config::Config(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file) {
        throw std::runtime_error("Unable to open configuration file: " + filePath);
    }
    file >> configJson_;
}

std::string Config::getIODevice() const {
    return configJson_.value("io", nlohmann::json::object()).value("device", "unknown");
}

std::unordered_map<std::string, std::string> Config::getPortsConfiguration() const {
    std::unordered_map<std::string, std::string> ports;
    auto portsConfig = configJson_.value("io", nlohmann::json::object()).value("portsConfiguration", nlohmann::json::object());
    for (auto it = portsConfig.begin(); it != portsConfig.end(); ++it) {
        ports[it.key()] = it.value();
    }
    return ports;
}

const std::vector<IOMapping>& Config::getInputs() const {
    static std::vector<IOMapping> inputs;
    inputs.clear();
    auto inputsArray = configJson_.value("io", nlohmann::json::object()).value("inputs", nlohmann::json::array());
    for (const auto& item : inputsArray) {
        IOMapping map;
        map.pin = item.value("pin", -1);
        map.name = item.value("name", "");
        map.description = item.value("description", "");
        inputs.push_back(map);
    }
    return inputs;
}

const std::vector<IOMapping>& Config::getOutputs() const {
    static std::vector<IOMapping> outputs;
    outputs.clear();
    auto outputsArray = configJson_.value("io", nlohmann::json::object()).value("outputs", nlohmann::json::array());
    for (const auto& item : outputsArray) {
        IOMapping map;
        map.pin = item.value("pin", -1);
        map.name = item.value("name", "");
        map.description = item.value("description", "");
        outputs.push_back(map);
    }
    return outputs;
}

nlohmann::json Config::getCommunicationSettings() const {
    return configJson_.value("communication", nlohmann::json::object());
}

nlohmann::json Config::getTimerSettings() const {
    return configJson_.value("timers", nlohmann::json::object());
}
