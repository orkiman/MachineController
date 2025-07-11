#include "communication/ArduinoProtocol.h"
#include "Logger.h"
#include <stdexcept>

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

std::string ArduinoProtocol::createPlanMessage(const std::vector<GlueRow>& rows) {
    try {
        nlohmann::json planMsg;
        planMsg["type"] = "plan";
        planMsg["rows"] = nlohmann::json::array();
        
        for (const auto& row : rows) {
            nlohmann::json rowObj;
            rowObj["from"] = row.from;
            rowObj["to"] = row.to;
            rowObj["space"] = row.space;
            planMsg["rows"].push_back(rowObj);
        }
        
        return planMsg.dump();
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::createPlanMessage] Exception: {}", e.what());
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

std::string ArduinoProtocol::createRunMessage() {
    try {
        nlohmann::json runMsg;
        runMsg["type"] = "run";
        
        return runMsg.dump();
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::createRunMessage] Exception: {}", e.what());
        return "";
    }
}

std::string ArduinoProtocol::createStopMessage() {
    try {
        nlohmann::json stopMsg;
        stopMsg["type"] = "stop";
        
        return stopMsg.dump();
    } catch (const std::exception& e) {
        getLogger()->error("[ArduinoProtocol::createStopMessage] Exception: {}", e.what());
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
