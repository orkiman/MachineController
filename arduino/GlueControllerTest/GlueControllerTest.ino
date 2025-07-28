/**
 * Arduino Glue Controller - Test Version
 * 
 * Simplified version for testing communication without hardware
 * Use this to verify JSON protocol before connecting actual hardware
 */

#include <ArduinoJson.h>

// Test variables
int encoderCount = 0;
int position = 0;
bool isRunning = false;
bool isEnabled = true;

void setup() {
  Serial.begin(115200);
  Serial.println("{\"type\":\"status\",\"message\":\"Arduino Glue Controller Test Ready\"}");
}

void loop() {
  processSerial();
  
  // Simulate encoder movement when running
  if (isRunning && isEnabled) {
    encoderCount += 2; // Simulate 2 pulses per loop
    position = encoderCount / 2; // 2 pulses per mm
    
    if (encoderCount % 100 == 0) {
      sendStatus();
    }
  }
  
  delay(100);
}

void processSerial() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() > 0) {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, input);
      
      if (error) {
        Serial.print("{\"type\":\"error\",\"message\":\"JSON parse error\"}");
        return;
      }
      
      String type = doc["type"];
      
      if (type == "heartbeat") {
        sendStatus();
      } else if (type == "config") {
        isEnabled = doc["enabled"] | true;
        Serial.println("{\"type\":\"config_ack\",\"status\":\"ok\"}");
      } else if (type == "plan") {
        Serial.println("{\"type\":\"plan_ack\",\"status\":\"ok\"}");
      } else if (type == "calibrate") {
        int pulses = doc["pageLength"] | 1000;
        sendCalibrationResult(pulses * 2); // Simulate 2 pulses per mm
      } else if (type == "run") {
        if (isEnabled) {
          isRunning = true;
          encoderCount = 0;
          position = 0;
          Serial.println("{\"type\":\"run_ack\",\"status\":\"started\"}");
        } else {
          Serial.println("{\"type\":\"error\",\"message\":\"Controller disabled\"}");
        }
      } else if (type == "stop") {
        isRunning = false;
        Serial.println("{\"type\":\"stop_ack\",\"status\":\"stopped\"}");
      }
    }
  }
}

void sendCalibrationResult(int pulses) {
  Serial.print("{\"type\":\"calibration_result\",\"pulsesPerPage\":");
  Serial.print(pulses);
  Serial.println("}");
}

void sendStatus() {
  Serial.print("{\"type\":\"status\",\"running\":");
  Serial.print(isRunning ? "true" : "false");
  Serial.print(",\"enabled\":");
  Serial.print(isEnabled ? "true" : "false");
  Serial.print(",\"position\":");
  Serial.print(position);
  Serial.print(",\"encoder\":");
  Serial.print(encoderCount);
  Serial.println("}");
}
