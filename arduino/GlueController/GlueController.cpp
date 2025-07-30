#include "GlueController.h"
#include <Arduino.h>

// Global variables
ControllerConfig config;
GunConfig guns[4];
volatile long encoderCount = 0;
volatile long lastEncoderCount = 0;
int currentPosition = 0;
int pageLength = 0;
int pulsesPerPage = 0;
bool isCalibrating = false;
unsigned long lastHeartbeat = 0;

// Interrupt Service Routine for encoder
void encoderISR() {
  if (digitalRead(ENCODER_PIN_A) == digitalRead(ENCODER_PIN_B)) {
    encoderCount++;
  } else {
    encoderCount--;
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  
  for (int i = 0; i < 4; i++) {
    pinMode(GUN_PINS[i], OUTPUT);
    digitalWrite(GUN_PINS[i], LOW);
  }
  
  // Attach encoder interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderISR, CHANGE);
  
  // Initialize gun configurations
  for (int i = 0; i < 4; i++) {
    guns[i].enabled = true;
    guns[i].rows.clear();
  }
  
  // Indicate startup with LED
  digitalWrite(STATUS_LED, HIGH);
}

void loop() {
  processSerial();
  checkSensor();
  
  if (config.enabled) {
    updateGuns();
  }
  
  // Heartbeat every 5 seconds
  if (millis() - lastHeartbeat > 5000) {
    sendStatus();
    lastHeartbeat = millis();
  }
}

void processSerial() {
  static String inputBuffer = "";
  
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == STX) {
      inputBuffer = "";
    } else if (c == ETX) {
      if (inputBuffer.length() > 0) {
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, inputBuffer);
        
        if (!error) {
          String type = doc["type"];
          
          if (type == "controller_setup") {
            handleConfig(doc.as<JsonObject>());
          } else if (type == "calibrate") {
            handleCalibrate(doc.as<JsonObject>());
          } else if (type == "heartbeat") {
            handleHeartbeat(doc.as<JsonObject>());
          }
        }
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
}

void handleConfig(const JsonObject& json) {
  config.type = json["controllerType"] | "dots";
  config.enabled = json["enabled"] | false;
  config.encoder = json["encoder"] | 1.0;
  config.sensorOffset = json["sensorOffset"] | 10;
  config.startCurrent = json["startCurrent"] | 1.0;
  config.startDuration = json["startDuration"] | 0.5;
  config.holdCurrent = json["holdCurrent"] | 0.5;
  config.dotSize = json["dotSize"] | "small";
  
  // Update status LED
  digitalWrite(STATUS_LED, config.enabled ? HIGH : LOW);
  
  // Process gun configurations if present
  if (json.containsKey("guns")) {
    JsonArray gunsArray = json["guns"];
    for (JsonObject gunConfig : gunsArray) {
      int gunId = gunConfig["gunId"];
      if (gunId >= 0 && gunId <= 3) {
        int index = gunId;
        guns[index].enabled = gunConfig["enabled"] | true;
        guns[index].rows.clear();
        
        if (gunConfig.containsKey("rows")) {
          JsonArray rows = gunConfig["rows"];
          for (JsonObject row : rows) {
            GlueRow newRow = {
              .from = row["from"],
              .to = row["to"],
              .space = row["space"]
            };
            guns[index].rows.push_back(newRow);
          }
        }
      }
    }
  }
}

void handleCalibrate(const JsonObject& json) {
  // Only process if enabled and not calibrating
  if (config.enabled && !isCalibrating) {
    bool wasEnabled = config.enabled;
    config.enabled = false;  // Disable controller during calibration
    digitalWrite(STATUS_LED, LOW);  // Turn off LED during calibration
  
    pageLength = json["pageLength"] | 1000;
    isCalibrating = true;
    encoderCount = 0;
  
    // Wait for page to pass sensor
    while (digitalRead(SENSOR_PIN) == HIGH) {
      // Non-blocking wait
      if (Serial.available()) {
        char c = Serial.read();
        if (c == STX) {
          // Handle potential new message even during calibration
          processSerial();
        }
      }
    }
    
    while (digitalRead(SENSOR_PIN) == LOW) {
      // Non-blocking wait
      if (Serial.available()) {
        char c = Serial.read();
        if (c == STX) {
          // Handle potential new message even during calibration
          processSerial();
        }
      }
    }
    
    pulsesPerPage = encoderCount;
    isCalibrating = false;
    config.enabled = wasEnabled;  // Restore previous enabled state
    digitalWrite(STATUS_LED, config.enabled ? HIGH : LOW);  // Update LED
    
    sendCalibrationResult(pulsesPerPage);
  }
}

void handleHeartbeat(const JsonObject& json) {
  sendStatus();
}

void sendCalibrationResult(int pulses) {
  StaticJsonDocument<128> doc;
  doc["type"] = "calibration_result";
  doc["pulsesPerPage"] = pulses;
  
  String output;
  serializeJson(doc, output);
  Serial.print(STX);
  Serial.print(output);
  Serial.println(ETX);
}

void updateGuns() {
  // Only process if not calibrating
  if (isCalibrating) return;
  
  // Calculate current position in mm
  currentPosition = (encoderCount * 1000) / (config.encoder * pulsesPerPage / pageLength);
  
  // Update each gun
  for (int i = 0; i < 4; i++) {
    if (!guns[i].enabled) {
      digitalWrite(GUN_PINS[i], LOW);
      continue;
    }
    
    bool shouldFire = false;
    
    // Check all rows for this gun
    for (const GlueRow& row : guns[i].rows) {
      if (currentPosition >= row.from && currentPosition <= row.to) {
        // Calculate if we should fire based on spacing
        int positionInRow = currentPosition - row.from;
        if (row.space > 0 && positionInRow % static_cast<int>(row.space * 10) < 5) {
          shouldFire = true;
          break;
        }
      }
    }
    
    digitalWrite(GUN_PINS[i], shouldFire ? HIGH : LOW);
  }
}

void checkSensor() {
  static bool lastSensorState = digitalRead(SENSOR_PIN);
  bool currentState = digitalRead(SENSOR_PIN);
  
  if (currentState != lastSensorState) {
    if (currentState == LOW) {
      // Sensor triggered (active low)
      if (config.enabled) {
        // Reset position counter when sensor is triggered
        encoderCount = 0;
      }
    }
    lastSensorState = currentState;
  }
}

void sendStatus() {
  StaticJsonDocument<256> doc;
  doc["type"] = "status";
  doc["enabled"] = config.enabled;
  doc["calibrating"] = isCalibrating;
  doc["position"] = currentPosition;
  doc["encoder"] = encoderCount;
  
  String output;
  serializeJson(doc, output);
  Serial.print(STX);
  Serial.print(output);
  Serial.println(ETX);
}
