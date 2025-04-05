// Config.cpp
#include "Config.h"
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include "Logger.h"
#include "io/IOChannel.h"

Config::Config(const std::string &filePath)
{
    filePath_ = filePath;
    std::ifstream file(filePath);
    if (!file)
    {
        throw std::runtime_error("Unable to open configuration file: " + filePath);
    }
    file >> configJson_;
    
    // Ensure default communication settings exist
    ensureDefaultCommunicationSettings();
}

std::string Config::getIODevice() const
{
    std::lock_guard<std::mutex> lock(configMutex_);
    return configJson_.value("io", nlohmann::json::object()).value("device", "unknown");
}

std::unordered_map<std::string, std::string> Config::getPci7248IoPortsConfiguration() const
{
    std::lock_guard<std::mutex> lock(configMutex_);
    std::unordered_map<std::string, std::string> ports;
    auto portsConfig = configJson_.value("io", nlohmann::json::object())
                           .value("portsConfiguration", nlohmann::json::object());
    for (auto it = portsConfig.begin(); it != portsConfig.end(); ++it)
    {
        ports[it.key()] = it.value();
    }
    return ports;
}

std::unordered_map<std::string, IOChannel> Config::getInputs() const
{
    std::lock_guard<std::mutex> lock(configMutex_);
    std::unordered_map<std::string, IOChannel> inputs;
    auto inputsArray = configJson_.value("io", nlohmann::json::object())
                           .value("inputs", nlohmann::json::array());
    for (const auto &item : inputsArray)
    {
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

std::unordered_map<std::string, IOChannel> Config::getOutputs() const
{
    std::lock_guard<std::mutex> lock(configMutex_);
    std::unordered_map<std::string, IOChannel> outputs;
    auto outputsArray = configJson_.value("io", nlohmann::json::object())
                            .value("outputs", nlohmann::json::array());
    for (const auto &item : outputsArray)
    {
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

nlohmann::json Config::getCommunicationSettings() const
{
    std::lock_guard<std::mutex> lock(configMutex_);
    return configJson_.value("communication", nlohmann::json::object());
}

nlohmann::json Config::getTimerSettings() const
{
    std::lock_guard<std::mutex> lock(configMutex_);
    return configJson_.value("timers", nlohmann::json::object());
}

void Config::updateCommunicationSettings(const nlohmann::json& commSettings)
{
    try {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        // Update communication settings
        if (commSettings.is_object()) {
            configJson_["communication"] = commSettings;
            getLogger()->info("Communication settings updated");
        } else {
            getLogger()->error("Invalid communication settings format");
        }
    } catch (const std::exception& e) {
        getLogger()->error("Error updating communication settings: {}", e.what());
    }
}

void Config::ensureDefaultCommunicationSettings()
{
    try {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        // Create communication object if it doesn't exist
        if (!configJson_.contains("communication") || !configJson_["communication"].is_object()) {
            configJson_["communication"] = nlohmann::json::object();
        }
        
        // Ensure trigger field exists
        if (!configJson_["communication"].contains("trigger")) {
            configJson_["communication"]["trigger"] = "t";
        }
        
        // Default settings for communication1
        if (!configJson_["communication"].contains("communication1") || !configJson_["communication"]["communication1"].is_object()) {
            configJson_["communication"]["communication1"] = {
                {"portName", "COM1"},
                {"baudRate", 115200},
                {"parity", "N"},
                {"dataBits", 8},
                {"stopBits", 1},
                {"stx", 2},
                {"etx", "0x03"}
            };
        }
        
        // Default settings for communication2
        if (!configJson_["communication"].contains("communication2") || !configJson_["communication"]["communication2"].is_object()) {
            configJson_["communication"]["communication2"] = {
                {"portName", "COM2"},
                {"baudRate", 115200},
                {"parity", "N"},
                {"dataBits", 8},
                {"stopBits", 1},
                {"stx", ""},
                {"etx", 3}
            };
        }
        
        getLogger()->info("Default communication settings ensured");
    } catch (const std::exception& e) {
        getLogger()->error("Error ensuring default communication settings: {}", e.what());
    }
}


bool Config::saveToFile(const std::string& filePath) const
{
    try {
        std::lock_guard<std::mutex> lock(configMutex_);
        
        // Use provided path or original path if empty
        std::string targetPath = filePath.empty() ? filePath_ : filePath;
        
        if (targetPath.empty()) {
            getLogger()->error("Cannot save configuration: No file path specified");
            return false;
        }
        
        // Create output file stream
        std::ofstream file(targetPath, std::ios::out | std::ios::trunc);
        if (!file) {
            getLogger()->error("Failed to open file for writing: {}", targetPath);
            return false;
        }
        
        // Write formatted JSON to file with indentation
        file << std::setw(2) << configJson_ << std::endl;
        
        if (file.bad()) {
            getLogger()->error("Error writing to file: {}", targetPath);
            return false;
        }
        
        getLogger()->info("Configuration successfully saved to: {}", targetPath);
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while saving configuration: {}", e.what());
        return false;
    }
}

bool Config::isPci7248ConfigurationValid() const
{
    std::lock_guard<std::mutex> lock(configMutex_);
    bool isValid = true;

    // Define the expected mapping for PCI7248.
    // Adjust these ranges to match your actual device specification.
    std::unordered_map<std::string, std::pair<int, int>> portPinRanges = {
        {"A", {0, 7}},  // Port A covers pins 0-7
        {"B", {8, 15}}, // Port B covers pins 8-15
        {"CL", {16, 19}},
        {"CH", {20, 23}}};

    // Get the "io" object from the JSON.
    auto ioConfig = configJson_.value("io", nlohmann::json::object());

    // Extract ports configuration.
    auto portsConfig = ioConfig.value("portsConfiguration", nlohmann::json::object());
    // Validate each port's configuration.
    for (auto it = portsConfig.begin(); it != portsConfig.end(); ++it)
    {
        std::string port = it.key();
        std::string direction = it.value();
        if (direction != "input" && direction != "output")
        {
            getLogger()->error("Port {} has an invalid configuration: {}", port, direction);
            isValid = false;
        }
    }

    // Validate each channel in the "inputs" array.
    auto inputsArray = ioConfig.value("inputs", nlohmann::json::array());
    for (const auto &item : inputsArray)
    {
        int pin = item.value("pin", -1);
        std::string name = item.value("name", ""); // for error messages
        bool foundPort = false;
        for (const auto &p : portPinRanges)
        {
            const std::string &port = p.first;
            int start = p.second.first;
            int end = p.second.second;
            if (pin >= start && pin <= end)
            {
                foundPort = true;
                // Check that the port is configured as "input".
                if (portsConfig.contains(port))
                {
                    std::string portDirection = portsConfig.at(port);
                    if (portDirection != "input")
                    {
                        getLogger()->error("Input channel {} (pin {}) belongs to port {} which is not configured as input.",
                                           name, pin, port);
                        isValid = false;
                    }
                }
                else
                {
                    getLogger()->error("Port {} is not defined in portsConfiguration.", port);
                    isValid = false;
                }
                break;
            }
        }
        if (!foundPort)
        {
            getLogger()->error("Input channel {} (pin {}) does not belong to any defined port.", name, pin);
            isValid = false;
        }
    }

    // Validate each channel in the "outputs" array.
    auto outputsArray = ioConfig.value("outputs", nlohmann::json::array());
    for (const auto &item : outputsArray)
    {
        int pin = item.value("pin", -1);
        std::string name = item.value("name", "");
        bool foundPort = false;
        for (const auto &p : portPinRanges)
        {
            const std::string &port = p.first;
            int start = p.second.first;
            int end = p.second.second;
            if (pin >= start && pin <= end)
            {
                foundPort = true;
                // Check that the port is configured as "output".
                if (portsConfig.contains(port))
                {
                    std::string portDirection = portsConfig.at(port);
                    if (portDirection != "output")
                    {
                        getLogger()->error("Output channel {} (pin {}) belongs to port {} which is not configured as output.",
                                           name, pin, port);
                        isValid = false;
                    }
                }
                else
                {
                    getLogger()->error("Port {} is not defined in portsConfiguration.", port);
                    isValid = false;
                }
                break;
            }
        }
        if (!foundPort)
        {
            getLogger()->error("Output channel {} (pin {}) does not belong to any defined port.", name, pin);
            isValid = false;
        }
    }

    return isValid;
}
