#include "communication/ArduinoProtocol.h"
#include "Logger.h"

std::string ArduinoProtocol::createConfigMessage(double encoderResolution, int sensorOffset) {
    try {
        nlohmann::json configMsg;
        configMsg["type"] = "config";
        configMsg["encoder"] = encoderResolution;
        configMsg["sensorOffset"] = sensorOffset;
        
        return configMsg.dump();
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::createConfigMessage] Exception: {}", e.what());
        return "";
    }
}

std::string ArduinoProtocol::createPlanMessage(const std::vector<std::vector<GlueRow>>& guns) {
    try {
        nlohmann::json planMsg;
        planMsg["type"] = "plan";
        planMsg["guns"] = nlohmann::json::array();
        
        for (const auto& gun : guns) {
            nlohmann::json gunObj;
            gunObj["rows"] = nlohmann::json::array();
            for (const auto& row : gun) {
                nlohmann::json rowObj;
                rowObj["from"] = row.from;
                rowObj["to"] = row.to;
                rowObj["space"] = row.space;
                gunObj["rows"].push_back(rowObj);
            }
            planMsg["guns"].push_back(gunObj);
        }
        
        return planMsg.dump();
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::createPlanMessage] Exception: {}", e.what());
        return "";
    }
}

std::string ArduinoProtocol::createControllerSetupMessage(const std::string& controllerType,
                                                        double encoderResolution,
                                                        int sensorOffset,
                                                        bool controllerEnabled,
                                                        const std::vector<std::pair<bool, std::vector<GlueRow>>>& guns,
                                                        double startCurrent,
                                                        double startDurationMS,
                                                        double holdCurrent,
                                                        const std::string& dotSize) {
    try {
        nlohmann::json setupMsg;
        setupMsg["type"] = "controller_setup";
        setupMsg["controllerType"] = controllerType;
        setupMsg["enabled"] = controllerEnabled;
        setupMsg["encoder"] = encoderResolution;
        setupMsg["sensorOffset"] = sensorOffset;
        
        // Add new glue controller fields
        setupMsg["startCurrent"] = startCurrent;
        setupMsg["startDurationMS"] = startDurationMS;
        setupMsg["holdCurrent"] = holdCurrent;
        setupMsg["dotSize"] = dotSize;
        
        setupMsg["guns"] = nlohmann::json::array();
        
        for (size_t i = 0; i < guns.size(); ++i) {
            nlohmann::json gunObj;
            gunObj["gunId"] = i + 1;  // Gun IDs are 1-based
            gunObj["enabled"] = guns[i].first;
            gunObj["rows"] = nlohmann::json::array();
            
            for (const auto& row : guns[i].second) {
                nlohmann::json rowObj;
                rowObj["from"] = row.from;
                rowObj["to"] = row.to;
                rowObj["space"] = row.space;
                gunObj["rows"].push_back(rowObj);
            }
            
            setupMsg["guns"].push_back(gunObj);
        }
        
        return setupMsg.dump();
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::createControllerSetupMessage] Exception: {}", e.what());
        return "";
    }
}

std::string ArduinoProtocol::createCalibrateMessage(int pageLength) {
    try {
        nlohmann::json calibrateMsg;
        calibrateMsg["type"] = "calibrate";
        calibrateMsg["pageLength"] = pageLength;
        
        return calibrateMsg.dump();
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::createCalibrateMessage] Exception: {}", e.what());
        return "";
    }
}

std::string ArduinoProtocol::createHeartbeatMessage() {
    try {
        nlohmann::json heartbeatMsg;
        heartbeatMsg["type"] = "heartbeat";
        
        return heartbeatMsg.dump();
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::createHeartbeatMessage] Exception: {}", e.what());
        return "";
    }
}

std::string ArduinoProtocol::createTestMessage(int gunIndex, bool on) {
    try {
        nlohmann::json testMsg;
        testMsg["type"] = "test";
        if (gunIndex >= 1 && gunIndex <= 4) {
            testMsg["t"] = std::string("t") + std::to_string(gunIndex);
        } else {
            testMsg["t"] = "all";
        }
        testMsg["state"] = on ? "on" : "off";
        return testMsg.dump();
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::createTestMessage] Exception: {}", e.what());
        return "";
    }
}

bool ArduinoProtocol::parseCalibrationResponse(const std::string& jsonResponse, int& pulsesPerPage) {
    try {
        nlohmann::json response = nlohmann::json::parse(jsonResponse);
        
        if (!response.contains("type") || response["type"] != "calibration_result") {
            getLogger()->warn("[ArduinoProtocol::parseCalibrationResponse] Invalid response type");
            return false;
        }
        
        if (!response.contains("pulsesPerPage")) {
            getLogger()->warn("[ArduinoProtocol::parseCalibrationResponse] Missing pulsesPerPage field");
            return false;
        }
        
        pulsesPerPage = response["pulsesPerPage"].get<int>();
        return true;
        
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::parseCalibrationResponse] Exception: {}", e.what());
        return false;
    }
}

void ArduinoProtocol::sendMessage(EventQueue<EventVariant>& eventQueue, 
                                 const std::string& communicationName, 
                                 const std::string& message) {
    try {
        if (message.empty()) {
            getLogger()->warn("[ArduinoProtocol::sendMessage] Cannot send empty message to {}", communicationName);
            return;
        }
        
        GuiEvent event;
        event.keyword = "SendCommunicationMessage";
        event.data = message;
        event.target = communicationName;
        eventQueue.push(event);
        
        getLogger()->debug("[ArduinoProtocol::sendMessage] Sent to {}: {}", communicationName, message);
        
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::sendMessage] Exception: {}", e.what());
    }
}
