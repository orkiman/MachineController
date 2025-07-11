# Arduino Communication Protocol Examples

This document provides examples of the JSON-based communication protocol used between the MachineController and Arduino glue controllers.

## Protocol Overview

The protocol uses simple JSON messages sent over RS232 or TCP/IP communication channels. Each message has a `type` field that identifies the message purpose.

## Message Types

### 1. Configuration Message
Sent when encoder resolution or sensor offset settings change in the UI.

**Format:**
```json
{
  "type": "config",
  "encoder": 1.234567,
  "sensorOffset": 10
}
```

**Example:**
```json
{
  "type": "config",
  "encoder": 2.5,
  "sensorOffset": 15
}
```

### 2. Plan Message
Sent when glue plan settings are modified in the UI.

**Format:**
```json
{
  "type": "plan",
  "rows": [
    {
      "from": 10,
      "to": 50,
      "space": 5.0
    },
    {
      "from": 60,
      "to": 90,
      "space": 3.5
    }
  ]
}
```

**Example:**
```json
{
  "type": "plan",
  "rows": [
    {
      "from": 0,
      "to": 100,
      "space": 2.5
    },
    {
      "from": 150,
      "to": 200,
      "space": 4.0
    }
  ]
}
```

### 3. Calibration Command
Sent when the "Calibrate Encoder" button is clicked.

**Format:**
```json
{
  "type": "calibrate",
  "pageLength": 100
}
```

**Example:**
```json
{
  "type": "calibrate",
  "pageLength": 150
}
```

### 4. Calibration Response
Expected response from Arduino after calibration is complete.

**Format:**
```json
{
  "type": "calibration_result",
  "pulsesPerPage": 12345
}
```

**Example:**
```json
{
  "type": "calibration_result",
  "pulsesPerPage": 8750
}
```

### 5. Run Command
Sent to start glue application on enabled controllers.

**Format:**
```json
{
  "type": "run"
}
```

### 6. Stop Command
Sent to stop glue application on enabled controllers.

**Format:**
```json
{
  "type": "stop"
}
```

### 7. Heartbeat
Periodic message to check communication status.

**Format:**
```json
{
  "type": "heartbeat"
}
```

## Communication Flow Examples

### Typical Setup Sequence

1. **Configuration Update:**
   ```
   PC → Arduino: {"type": "config", "encoder": 2.5, "sensorOffset": 10}
   ```

2. **Plan Update:**
   ```
   PC → Arduino: {"type": "plan", "rows": [{"from": 0, "to": 100, "space": 3.0}]}
   ```

3. **Encoder Calibration:**
   ```
   PC → Arduino: {"type": "calibrate", "pageLength": 100}
   Arduino → PC: {"type": "calibration_result", "pulsesPerPage": 8000}
   ```

4. **Start Operation:**
   ```
   PC → Arduino: {"type": "run"}
   ```

5. **Stop Operation:**
   ```
   PC → Arduino: {"type": "stop"}
   ```

### Error Handling

- If Arduino receives an unknown message type, it should ignore the message
- If Arduino cannot parse JSON, it should ignore the message
- Communication timeouts should be handled gracefully
- Invalid parameter values should use safe defaults

## Arduino Implementation Notes

### Parsing JSON
Use a lightweight JSON library like ArduinoJson:

```cpp
#include <ArduinoJson.h>

void handleMessage(String message) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.println("JSON parse error");
        return;
    }
    
    const char* type = doc["type"];
    
    if (strcmp(type, "config") == 0) {
        handleConfig(doc);
    } else if (strcmp(type, "plan") == 0) {
        handlePlan(doc);
    } else if (strcmp(type, "calibrate") == 0) {
        handleCalibrate(doc);
    } else if (strcmp(type, "run") == 0) {
        handleRun();
    } else if (strcmp(type, "stop") == 0) {
        handleStop();
    }
}
```

### Sending Responses
For calibration results:

```cpp
void sendCalibrationResult(int pulses) {
    StaticJsonDocument<100> doc;
    doc["type"] = "calibration_result";
    doc["pulsesPerPage"] = pulses;
    
    String output;
    serializeJson(doc, output);
    Serial.println(output);
}
```

## Communication Settings

### RS232 Settings
- **Baud Rate:** 9600, 19200, 38400, or 115200
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1
- **Flow Control:** None

### TCP/IP Settings
- **Port:** Configurable (default: 23)
- **Protocol:** TCP
- **Timeout:** 5000ms

## Testing

### Manual Testing
Use the communication tab in MachineController to send test messages:

1. Select the communication channel
2. Enter JSON message in the trigger field
3. Click "Send" button
4. Monitor responses in the communication log

### Example Test Messages
```json
{"type": "config", "encoder": 1.0, "sensorOffset": 5}
{"type": "plan", "rows": [{"from": 0, "to": 50, "space": 2.0}]}
{"type": "calibrate", "pageLength": 100}
{"type": "run"}
{"type": "stop"}
{"type": "heartbeat"}
```

## Troubleshooting

### Common Issues

1. **"Communication port not found"**
   - Ensure communication channel is active in communication tab
   - Check that glue controller uses communication name (e.g., "communication1") not COM port name

2. **Messages not being sent**
   - Verify controller is enabled in glue tab
   - Check communication port selection in glue controller settings

3. **Arduino not responding**
   - Verify baud rate and serial settings match
   - Check Arduino serial monitor for received messages
   - Ensure Arduino JSON parsing is working correctly

### Debug Logging
Enable debug logging in MachineController to see all sent messages:
- Check application logs for message sending confirmations
- Look for communication errors or timeouts
- Verify JSON message format in logs
