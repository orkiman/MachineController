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

std::unordered_map<std::string, std::string> Config::getPci7248IoPortsConfiguration() const {
    std::unordered_map<std::string, std::string> ports;
    auto portsConfig = configJson_.value("io", nlohmann::json::object())
                          .value("portsConfiguration", nlohmann::json::object());
    for (auto it = portsConfig.begin(); it != portsConfig.end(); ++it) {
        ports[it.key()] = it.value();
    }
    return ports;
}

const std::vector<IOChannel>& Config::getInputs() const {
    static std::vector<IOChannel> inputs;
    inputs.clear();
    auto inputsArray = configJson_.value("io", nlohmann::json::object())
                           .value("inputs", nlohmann::json::array());
    for (const auto& item : inputsArray) {
        IOChannel channel;
        channel.pin = item.value("pin", -1);
        channel.name = item.value("name", "");
        channel.description = item.value("description", "");
        // You can set defaults here:
        channel.type = IOType::Input;
        channel.state = 0;
        channel.eventType = IOEventType::None;
        // Optionally, if you include the port in your JSON:
        channel.ioPort = item.value("ioPort", "");
        inputs.push_back(channel);
    }
    return inputs;
}

const std::vector<IOChannel>& Config::getOutputs() const {
    static std::vector<IOChannel> outputs;
    outputs.clear();
    auto outputsArray = configJson_.value("io", nlohmann::json::object())
                            .value("outputs", nlohmann::json::array());
    for (const auto& item : outputsArray) {
        IOChannel channel;
        channel.pin = item.value("pin", -1);
        channel.name = item.value("name", "");
        channel.description = item.value("description", "");
        channel.type = IOType::Output;
        channel.state = 0;
        channel.eventType = IOEventType::None;
        channel.ioPort = item.value("ioPort", "");
        outputs.push_back(channel);
    }
    return outputs;
}

nlohmann::json Config::getCommunicationSettings() const {
    return configJson_.value("communication", nlohmann::json::object());
}

nlohmann::json Config::getTimerSettings() const {
    return configJson_.value("timers", nlohmann::json::object());
}
