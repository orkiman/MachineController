#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <unordered_map>
#include <vector>
#include "json.hpp"  // Include the nlohmann JSON header

// Define a structure for an IO mapping
struct IOMapping {
    int pin;
    std::string name;
    std::string description;
};

// Config class for the project settings
class Config {
public:
    // Constructor that loads the configuration from a file.
    Config(const std::string& filePath);

    // Accessors for different sections
    std::string getIODevice() const;
    std::unordered_map<std::string, std::string> getPortsConfiguration() const;
    const std::vector<IOMapping>& getInputs() const;
    const std::vector<IOMapping>& getOutputs() const;
    
    // Other getters for communication and timers
    nlohmann::json getCommunicationSettings() const;
    nlohmann::json getTimerSettings() const;

private:
    nlohmann::json configJson_;
};

#endif // CONFIG_H
