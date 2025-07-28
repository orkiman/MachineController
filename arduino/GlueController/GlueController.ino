/**
 * Arduino Glue Controller
 * 
 * This program implements a complete glue controller that receives configuration
 * and commands from the MachineController application via JSON messages.
 * 
 * Features:
 * - JSON protocol communication via Serial
 * - 4-gun glue controller support
 * - Encoder-based positioning
 * - Sensor-based triggering
 * - Real-time glue application
 * - Calibration support
 * 
 * Hardware Requirements:
 * - Arduino Mega 2560 (recommended for multiple guns and interrupts)
 * - 4x Glue gun solenoids (MOSFET drivers)
 * - Rotary encoder (quadrature)
 * - Optical sensor for page detection
 * - RS232/TTL communication module
 */

#include <ArduinoJson.h>

// Pin definitions
const int ENCODER_PIN_A = 2;      // Encoder channel A (interrupt pin)
const int ENCODER_PIN_B = 3;      // Encoder channel B (interrupt pin)
const int SENSOR_PIN = 4;         // Optical sensor for page detection
const int GUN_PINS[4] = {8, 9, 10, 11};  // Solenoid control pins for 4 guns
const int STATUS_LED = 13;        // Built-in LED for status indication

// Configuration structure
struct ControllerConfig {
  String type = "dots";           // "dots" or "line"
  bool enabled = true;
  double encoder = 1.0;           // Pulses per mm
  int sensorOffset = 10;          // mm from sensor to glue position
  double startCurrent = 1.0;      // A
  double startDuration = 500;     // ms
  double holdCurrent = 0.5;       // A
  String dotSize = "small";       // "small", "medium", "large"
};

// Glue row structure
struct GlueRow {
  int from;
  int to;
  double space;
};

// Gun configuration
struct GunConfig {
  bool enabled = true;
  std::vector<GlueRow> rows;
};

// Global variables
ControllerConfig config;
GunConfig guns[4];
volatile long encoderCount = 0;
volatile long lastEncoderCount = 0;
int currentPosition = 0;
int pageLength = 0;
int pulsesPerPage = 0;
bool isRunning = false;
bool isCalibrating = false;
unsigned long lastHeartbeat = 0;

// JSON document buffer
StaticJsonDocument<1024> doc;

// Function prototypes
void setup();
void loop();
void processSerial();
void handleConfig(const JsonObject& json);
void handlePlan(const JsonObject& json);
void handleCalibrate(const JsonObject& json);
void handleRun(const JsonObject& json);
void handleStop(const JsonObject& json);
void handleHeartbeat(const JsonObject& json);
void sendCalibrationResult(int pulses);
void updateGuns();
void encoderISR();
void checkSensor();
void fireGun(int gunIndex, int position);
void sendStatus();

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  
  for (int i = 0; i < 4; i++) {
    pinMode(GUN_PINS[i], OUTPUT);
    digitalWrite(GUN_PINS[i], LOW);
  }
  
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);
  
  // Setup encoder interrupts
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderISR, CHANGE);
  
  // Initialize guns with empty rows
  for (int i = 0; i < 4; i++) {
    guns[i].enabled = true;
    guns[i].rows.clear();
  }
  
  Serial.println("{"type":"status","message":"Arduino Glue Controller Ready"}");
  
  // Blink LED to indicate startup
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(200);
    digitalWrite(STATUS_LED, LOW);
    delay(200);
  }
}

void loop() {
  processSerial();
  checkSensor();
  
  if (isRunning && config.enabled) {
    updateGuns();
  }
  
  // Heartbeat every 5 seconds
  if (millis() - lastHeartbeat > 5000) {
    sendStatus();
    lastHeartbeat = millis();
  }
  
  delay(10); // Small delay for stability
}

void 

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() > 0) {
      DeserializationError error = deserializeJson(doc, input);
      
      if (error) {
        Serial.print("{\"type\":\"error\",\"message\":\"JSON parse error: ");
        Serial.print(error.c_str());
        Serial.println("\"}");
        return;
      }
      
      String type = doc["type"];
      
      if (type == "config") {
        handleConfig(doc.as<JsonObject>());
      } else if (type == "plan") {
        handlePlan(doc.as<JsonObject>());
      } else if (type == "calibrate") {
        handleCalibrate(doc.as<JsonObject>());
      } else if (type == "run") {
        handleRun(doc.as<JsonObject>());
      } else if (type == "stop") {
        handleStop(doc.as<JsonObject>());
      } else if (type == "heartbeat") {
        handleHeartbeat(doc.as<JsonObject>());
      } else {
        Serial.println("{\"type\":\"error\",\"message\":\"Unknown message type\"}");
      }
    }
  }
}

void handleConfig(const JsonObject& json) {
  config.type = json["controllerType"] | "dots";
  config.enabled = json["enabled"] | true;
  config.encoder = json["encoder"] | 1.0;
  config.sensorOffset = json["sensorOffset"] | 10;
  config.startCurrent = json["startCurrent"] | 1.0;
  config.startDuration = json["startDuration"] | 500;
  config.holdCurrent = json["holdCurrent"] | 0.5;
  config.dotSize = json["dotSize"] | "small";
  
  // Send confirmation
  Serial.println("{\"type\":\"config_ack\",\"status\":\"ok\"}");
  
  // Update status LED
  digitalWrite(STATUS_LED, config.enabled ? HIGH : LOW);
}

void handlePlan(const JsonObject& json) {
  // Clear existing plans
  for (int i = 0; i < 4; i++) {
    guns[i].rows.clear();
  }
  
  // Load new plans
  JsonArray gunsArray = json["guns"];
  for (int i = 0; i < min(4, (int)gunsArray.size()); i++) {
    JsonObject gunObj = gunsArray[i];
    guns[i].enabled = gunObj["enabled"] | true;
    
    JsonArray rowsArray = gunObj["rows"];
    for (JsonObject rowObj : rowsArray) {
      GlueRow row;
      row.from = rowObj["from"] | 0;
      row.to = rowObj["to"] | 100;
      row.space = rowObj["space"] | 5.0;
      guns[i].rows.push_back(row);
    }
  }
  
  Serial.println("{\"type\":\"plan_ack\",\"status\":\"ok\"}");
}

void handleCalibrate(const JsonObject& json) {
  if (isRunning) {
    Serial.println("{\"type\":\"error\",\"message\":\"Cannot calibrate while running\"}");
    return;
  }
  
  pageLength = json["pageLength"] | 1000;
  isCalibrating = true;
  encoderCount = 0;
  
  Serial.println("{\"type\":\"calibrate_ack\",\"status\":\"started\"}");
  
  // Wait for page to pass sensor
  while (digitalRead(SENSOR_PIN) == HIGH) {
    delay(10);
  }
  
  while (digitalRead(SENSOR_PIN) == LOW) {
    delay(10);
  }
  
  pulsesPerPage = encoderCount;
  isCalibrating = false;
  
  sendCalibrationResult(pulsesPerPage);
}

void handleRun(const JsonObject& json) {
  if (!config.enabled) {
    Serial.println("{\"type\":\"error\",\"message\":\"Controller disabled\"}");
    return;
  }
  
  if (isRunning) {
    Serial.println("{\"type\":\"error\",\"message\":\"Already running\"}");
    return;
  }
  
  isRunning = true;
  encoderCount = 0;
  Serial.println("{\"type\":\"run_ack\",\"status\":\"started\"}");
}

void handleStop(const JsonObject& json) {
  if (!isRunning) {
    Serial.println("{\"type\":\"error\",\"message\":\"Not running\"}");
    return;
  }
  
  isRunning = false;
  
  // Turn off all guns
  for (int i = 0; i < 4; i++) {
    digitalWrite(GUN_PINS[i], LOW);
  }
  
  Serial.println("{\"type\":\"stop_ack\",\"status\":\"stopped\"}");
}

void handleHeartbeat(const JsonObject& json) {
  sendStatus();
}

void sendCalibrationResult(int pulses) {
  StaticJsonDocument<256> doc;
  doc["type"] = "calibration_result";
  doc["pulsesPerPage"] = pulses;
  
  serializeJson(doc, Serial);
  Serial.println();
}

void sendStatus() {
  StaticJsonDocument<512> doc;
  doc["type"] = "status";
  doc["running"] = isRunning;
  doc["enabled"] = config.enabled;
  doc["position"] = currentPosition;
  doc["encoder"] = encoderCount;
  
  serializeJson(doc, Serial);
  Serial.println();
}

void updateGuns() {
  noInterrupts(); // Prevent encoder updates during calculation
  currentPosition = (int)(encoderCount / config.encoder);
  interrupts();
  
  for (int gunIndex = 0; gunIndex < 4; gunIndex++) {
    if (!guns[gunIndex].enabled || !config.enabled) {
      digitalWrite(GUN_PINS[gunIndex], LOW);
      continue;
    }
    
    bool shouldFire = false;
    
    for (const GlueRow& row : guns[gunIndex].rows) {
      if (currentPosition >= row.from && currentPosition <= row.to) {
        int relativePos = currentPosition - row.from;
        if (fmod(relativePos, row.space) < 1) {
          shouldFire = true;
          break;
        }
      }
    }
    
    digitalWrite(GUN_PINS[gunIndex], shouldFire ? HIGH : LOW);
  }
}

void checkSensor() {
  static bool lastSensorState = HIGH;
  bool currentSensorState = digitalRead(SENSOR_PIN);
  
  if (lastSensorState == HIGH && currentSensorState == LOW) {
    // Page detected
    if (isRunning) {
      encoderCount = 0;
      currentPosition = 0;
    }
  }
  
  lastSensorState = currentSensorState;
}

void encoderISR() {
  static int lastState = 0;
  static int state = 0;
  
  state = (digitalRead(ENCODER_PIN_B) << 1) | digitalRead(ENCODER_PIN_A);
  
  // Simple quadrature decoding
  if ((lastState == 0 && state == 1) || 
      (lastState == 1 && state == 3) || 
      (lastState == 3 && state == 2) || 
      (lastState == 2 && state == 0)) {
    encoderCount++;
  } else if ((lastState == 0 && state == 2) || 
             (lastState == 2 && state == 3) || 
             (lastState == 3 && state == 1) || 
             (lastState == 1 && state == 0)) {
    encoderCount--;
  }
  
  lastState = state;
}

// Helper function for modulo with floating point
float fmod(float x, float y) {
  return x - y * floor(x / y);
}
