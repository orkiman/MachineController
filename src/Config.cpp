// Config.cpp
#include "Config.h"
#include <fstream>
#include <stdexcept>
#include "Logger.h"
#include "communication/RS232Communication.h"

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

const std::unordered_map<std::string, IOChannel>& Config::getInputs() const {
    static std::unordered_map<std::string, IOChannel> inputs;
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
        inputs[channel.name] = channel;
    }
    return inputs;
}

const std::unordered_map<std::string, IOChannel>& Config::getOutputs() const {
    static std::unordered_map<std::string, IOChannel> outputs;
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
        outputs[channel.name] = channel;
    }
    return outputs;
}

nlohmann::json Config::getCommunicationSettings() const {
    return configJson_.value("communication", nlohmann::json::object());
}

nlohmann::json Config::getTimerSettings() const {
    return configJson_.value("timers", nlohmann::json::object());
}

// New function to get communication settings for a specific communication object
CommunicationSettings Config::getCommunicationSettings(const std::string& communicationName) const {
    CommunicationSettings settings;
    // Default values
    settings.baudRate = 115200;
    settings.parity = 'N';
    settings.dataBits = 8;
    settings.stopBits = 1;
    settings.stx = 0x02; // Default STX
    settings.etx = 0x03; // Default ETX

    auto commSettings = configJson_.value("communication", nlohmann::json::object());
    if (commSettings.contains(communicationName)) {
        auto specificCommSettings = commSettings[communicationName];
        settings.portName = specificCommSettings.value("portName", "");
        settings.baudRate = specificCommSettings.value("baudRate", settings.baudRate);

        std::string parityStr = specificCommSettings.value("parity", "N");
        if (parityStr == "N" || parityStr == "E" || parityStr == "O") {
            settings.parity = parityStr[0];
        } else {
            getLogger()->warn("Invalid parity setting for {}: {}. Using default 'N'.", communicationName, parityStr);
        }

        settings.dataBits = specificCommSettings.value("dataBits", settings.dataBits);
        settings.stopBits = specificCommSettings.value("stopBits", settings.stopBits);

        // Simplified STX handling
        settings.stx = parseCharSetting(specificCommSettings, "stx", settings.stx);

        // Simplified ETX handling
        settings.etx = parseCharSetting(specificCommSettings, "etx", settings.etx);
    } else {
        getLogger()->warn("Communication settings for {} not found in config. Using default values.", communicationName);
    }

    return settings;
}

// Helper function to parse char settings (STX, ETX)
char Config::parseCharSetting(const nlohmann::json& settings, const std::string& key, char defaultValue) const {
    if (!settings.contains(key)) {
        return defaultValue;
    }

    const auto& value = settings[key];
    if (value.is_number_integer()) {
        return static_cast<char>(value.get<int>());
    } else if (value.is_string()) {
        std::string strValue = value.get<std::string>();
        if (strValue.empty()) {
            return 0; // Empty string means no STX/ETX
        } else if (strValue.rfind("0x", 0) == 0) {
            return static_cast<char>(std::stoi(strValue, nullptr, 16));
        } else {
            return strValue[0]; // Take the first character
        }
    } else {
        getLogger()->warn("Invalid type for {} setting. Using default value.", key);
        return defaultValue;
    }
}

bool Config::isPci7248ConfigurationValid() const {
    bool isValid = true;

    // Define the expected mapping for PCI7248.
    // Adjust these ranges to match your actual device specification.
    std::unordered_map<std::string, std::pair<int, int>> portPinRanges = {
        {"A", {0, 7}},   // Port A covers pins 0-7
        {"B", {8, 15}},  // Port B covers pins 8-15
        {"CL", {16, 19}},
        {"CH", {20, 23}}
    };

    // Get the "io" object from the JSON.
    auto ioConfig = configJson_.value("io", nlohmann::json::object());

    // Extract ports configuration.
    auto portsConfig = ioConfig.value("portsConfiguration", nlohmann::json::object());
    // Validate each port's configuration.
    for (auto it = portsConfig.begin(); it != portsConfig.end(); ++it) {
        std::string port = it.key();
        std::string direction = it.value();
        if (direction != "input" && direction != "output") {
            getLogger()->error("Port {} has an invalid configuration: {}", port, direction);
            isValid = false;
        }
    }

    // Validate each channel in the "inputs" array.
    auto inputsArray = ioConfig.value("inputs", nlohmann::json::array());
    for (const auto &item : inputsArray) {
        int pin = item.value("pin", -1);
        std::string name = item.value("name", "");  // for error messages
        bool foundPort = false;
        for (const auto &p : portPinRanges) {
            const std::string &port = p.first;
            int start = p.second.first;
            int end = p.second.second;
            if (pin >= start && pin <= end) {
                foundPort = true;
                // Check that the port is configured as "input".
                if (portsConfig.contains(port)) {
                    std::string portDirection = portsConfig.at(port);
                    if (portDirection != "input") {
                        getLogger()->error("Input channel {} (pin {}) belongs to port {} which is not configured as input.", 
                                             name, pin, port);
                        isValid = false;
                    }
                } else {
                    getLogger()->error("Port {} is not defined in portsConfiguration.", port);
                    isValid = false;
                }
                break;
            }
        }
        if (!foundPort) {
            getLogger()->error("Input channel {} (pin {}) does not belong to any defined port.", name, pin);
            isValid = false;
        }
    }

    // Validate each channel in the "outputs" array.
    auto outputsArray = ioConfig.value("outputs", nlohmann::json::array());
    for (const auto &item : outputsArray) {
        int pin = item.value("pin", -1);
        std::string name = item.value("name", "");
        bool foundPort = false;
        for (const auto &p : portPinRanges) {
            const std::string &port = p.first;
            int start = p.second.first;
            int end = p.second.second;
            if (pin >= start && pin <= end) {
                foundPort = true;
                // Check that the port is configured as "output".
                if (portsConfig.contains(port)) {
                    std::string portDirection = portsConfig.at(port);
                    if (portDirection != "output") {
                        getLogger()->error("Output channel {} (pin {}) belongs to port {} which is not configured as output.", 
                                             name, pin, port);
                        isValid = false;
                    }
                } else {
                    getLogger()->error("Port {} is not defined in portsConfiguration.", port);
                    isValid = false;
                }
                break;
            }
        }
        if (!foundPort) {
            getLogger()->error("Output channel {} (pin {}) does not belong to any defined port.", name, pin);
            isValid = false;
        }
    }

    return isValid;
}
