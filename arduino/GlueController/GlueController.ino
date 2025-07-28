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

// Protocol constants
const char STX = 0x02;  // Start of text
const char ETX = 0x03;  // End of text

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
void handleCalibrate(const JsonObject& json);
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
  
  // Indicate startup with LED
  digitalWrite(STATUS_LED, HIGH);
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
}

void processSerial() {
  static String inputBuffer = "";
  
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == STX) {
      inputBuffer = "";
    } else if (c == ETX) {
      if (inputBuffer.length() > 0) {
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
      }
      inputBuffer = "";
    } else if (c >= 32 && c <= 126) {  // Only printable ASCII
      inputBuffer += c;
    }
    
    // Prevent buffer overflow
    if (inputBuffer.length() > 1023) {
      inputBuffer = "";
    }
  }
}

void handleConfig(const JsonObject& json) {
  config.type = json["controllerType"] | "dots";
  config.enabled = json["enabled"] | true;
  config.encoder = json["encoder"] | 1.0;
  config.sensorOffset = json["sensorOffset"] | 10;
  config.startCurrent = json["startCurrent"] | 1.0;
  config.startDuration = json["startDuration"] | 0.5;  // Changed to ms as per protocol
  config.holdCurrent = json["holdCurrent"] | 0.5;
  config.dotSize = json["dotSize"] | "small";
  
  // Update running state and status LED
  isRunning = config.enabled;
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
  bool wasRunning = isRunning;
  isRunning = false;  // Stop any running operation during calibration
  
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
  isRunning = wasRunning;  // Restore previous running state
  
  sendCalibrationResult(pulsesPerPage);
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
  Serial.write(STX);
  Serial.print(output);
  Serial.write(ETX);
}

void sendStatus() {
  StaticJsonDocument<256> doc;
  doc["type"] = "heartbeat";
  doc["position"] = currentPosition;
  
  String output;
  serializeJson(doc, output);
  Serial.write(STX);
  Serial.print(output);
  Serial.write(ETX);
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
