#ifndef GLUECONTROLLER_H
#define GLUECONTROLLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Protocol constants
const char STX = 0x02;  // Start of text
const char ETX = 0x03;  // End of text

// Pin definitions
const int ENCODER_PIN_A = 2;      // Encoder channel A (interrupt pin)
const int ENCODER_PIN_B = 3;      // Encoder channel B (interrupt pin)
const int SENSOR_PIN = 4;         // Optical sensor for page detection
const int GUN_PINS[4] = {8, 9, 10, 11};  // Solenoid control pins for 4 guns
const int STATUS_LED = 13;        // Built-in LED for status indication

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

// Configuration structure
struct ControllerConfig {
  String type = "dots";           // "dots" or "line"
  bool enabled = false;
  double encoder = 1.0;           // Pulses per mm
  int sensorOffset = 10;          // mm from sensor to glue position
  double startCurrent = 1.0;      // A
  double startDuration = 500;     // ms
  double holdCurrent = 0.5;       // A
  String dotSize = "small";       // "small", "medium", "large"
};

// Global variables
extern ControllerConfig config;
extern GunConfig guns[4];
extern volatile long encoderCount;
extern volatile long lastEncoderCount;
extern int currentPosition;
extern int pageLength;
extern int pulsesPerPage;
extern bool isCalibrating;
extern unsigned long lastHeartbeat;

// Function declarations
void setup();
void loop();
void processSerial();
void handleConfig(const JsonObject& json);
void handleCalibrate(const JsonObject& json);
void handleHeartbeat(const JsonObject& json);
void sendCalibrationResult(int pulses);
void updateGuns();
void checkSensor();
void sendStatus();

// Interrupt Service Routine declarations
void encoderISR();

#endif // GLUECONTROLLER_H
