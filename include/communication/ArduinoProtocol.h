#ifndef ARDUINOPROTOCOL_H
#define ARDUINOPROTOCOL_H

#include <string>
#include <vector>
#include "json.hpp"
#include "EventQueue.h"
#include "Event.h"

/**
 * @brief Helper class for Arduino communication protocol
 * 
 * Handles JSON message formatting for glue controller communication:
 * - Config messages: {"type": "config", "encoder": 1.234567, "sensorOffset": 10}
 * - Plan messages: {"type": "plan", "rows": [{"from": 10, "to": 50, "space": 5.0}]}
 * - Calibration: {"type": "calibrate", "pageLength": 100}
 * - Control: {"type": "run"} / {"type": "stop"}
 * - Heartbeat: {"type": "heartbeat"}
 */
class ArduinoProtocol {
public:
    struct GlueRow {
        int from;
        int to;
        double space;
    };
    
    /**
     * @brief Create config message for Arduino
     * @param encoderResolution Encoder resolution in pulses per mm
     * @param sensorOffset Sensor offset in mm
     * @return JSON string for config message
     */
    static std::string createConfigMessage(double encoderResolution, int sensorOffset);
    
    /**
     * @brief Create plan message for Arduino
     * @param rows Vector of glue rows with from/to/space values
     * @return JSON string for plan message
     */
    static std::string createPlanMessage(const std::vector<GlueRow>& rows);
    
    /**
     * @brief Create calibration message for Arduino
     * @param pageLength Page length in mm for calibration
     * @return JSON string for calibration message
     */
    static std::string createCalibrateMessage(int pageLength);
    
    /**
     * @brief Create run command message
     * @return JSON string for run command
     */
    static std::string createRunMessage();
    
    /**
     * @brief Create stop command message
     * @return JSON string for stop command
     */
    static std::string createStopMessage();
    
    /**
     * @brief Create heartbeat message
     * @return JSON string for heartbeat
     */
    static std::string createHeartbeatMessage();
    
    /**
     * @brief Parse calibration response from Arduino
     * @param jsonResponse JSON response string from Arduino
     * @param pulsesPerPage Output parameter for pulses per page value
     * @return true if parsing successful, false otherwise
     */
    static bool parseCalibrationResponse(const std::string& jsonResponse, int& pulsesPerPage);
    
    /**
     * @brief Send message to Arduino via communication port
     * @param eventQueue Event queue for sending messages
     * @param communicationName Name of communication port (e.g., "communication1")
     * @param message JSON message to send
     */
    static void sendMessage(EventQueue<EventVariant>& eventQueue, 
                           const std::string& communicationName, 
                           const std::string& message);
};

#endif // ARDUINOPROTOCOL_H
