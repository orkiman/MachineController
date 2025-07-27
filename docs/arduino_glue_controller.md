# Arduino Glue Controller - System Description and Implementation Plan

## Overview

The Arduino Glue Controller is designed to control a glue dispensing system based on motion feedback and configuration data received via serial communication from a PC. It supports two controller types: **dots** and **lines**. Each mode controls the behavior of up to **4 glue guns**, with control parameters tailored to each mode.

The controller is intended to operate on an **Arduino Uno**, with key I/O pins dedicated to input and output functions as specified below.

---

## Hardware Specifications

### Inputs:

- **Trigger Input**: Digital input (common to all guns)
- **Encoder Input**: Single input (may use interrupts)
- **Analog Inputs (A0-A3)**: Measure current for each gun

### Outputs:

- **Glue Guns Outputs (D8 - D11)**: Control up to 4 glue guns

---

## Serial Communication Protocol

Communication is handled via serial (USB) using structured JSON messages.

### Message Types:

1. **Controller Setup** (PC → Arduino)
2. **Calibrate Request** (PC → Arduino)
3. **Calibrate Response** (Arduino → PC)
4. **Heartbeat** (Optional, not implemented yet)

### Example Controller Setup Message:

```json
{
  "type": "controller_setup",
  "controllerType": "dots",
  "enabled": true,
  "encoder": 1.5,
  "sensorOffset": 10,
  "startCurrent": 1.0,
  "startDurationMS": 0.5,
  "holdCurrent": 0.5,
  "dotSize": "medium",
  "pageLength": 100,
  "guns": [
    {
      "enabled": true,
      "gunId": 1,
      "rows": [
        { "from": 10, "to": 50, "space": 5.0 }
      ]
    }
  ]
}
```

---

## Control Modes

### 1. Dots Mode

- **Operation**: Gun fires a pulse of glue at each point.
- **Dot Size**: Determined by pulse length (in microseconds)
- **Row Pattern**: Multiple rows defined with `from`, `to`, and `space` parameters.

### 2. Lines Mode

- **Operation**: Gun turns on with a start current (to pull the magnet) then maintains a hold current.
- **Line Activation**: Based on encoder position and gun's row configuration.

---

## Behavior and Logic

### Trigger Handling:

- Trigger signal initiates gun pattern.
- All guns start according to their respective `rows` configuration.

### Encoder Handling:

- Measures motion of substrate.
- Used to determine glue dispensing locations.
- Must support interrupts or high-frequency polling.

### Overlap Handling:

- If the pattern length exceeds the encoder pulses between triggers, the pattern will be truncated.

### Minimum Speed:

- Glue guns will not fire if movement speed is below the minimum threshold.

---

## Implementation Plan

### Arduino Firmware

1. **Serial Communication Module**:

   - JSON parsing for setup/calibration messages
   - JSON generation for response
2. **Configuration Management**:

   - Store the received configuration in memory
   - Support live updates when `enabled` state or parameters change
3. **Gun Control Module**:

   - Map gun output pins (D8-D11)
   - Calculate pulse timing for dots mode
   - Implement start/hold current profile for lines mode
   - Interface with analog inputs to verify current
4. **Encoder Handling**:

   - Use interrupts for reliable pulse counting
   - Maintain accurate position counter per trigger cycle
5. **Trigger Processing**:

   - On rising edge, start new pattern cycle
6. **Safety and Edge Conditions**:

   - Abort pattern if encoder pulses are insufficient
   - Prevent activation if controller is not enabled

### PC Software

- GUI/CLI to send JSON configuration and calibration commands
- Visual interface for designing patterns (optional)
- Calibration utility to determine pulses per mm

---

## Pin Assignment Summary (Arduino Uno)

| Function      | Pin       |
| ------------- | --------- |
| Gun 1 Output  | D8        |
| Gun 2 Output  | D9        |
| Gun 3 Output  | D10       |
| Gun 4 Output  | D11       |
| Trigger Input | D2 or D3  |
| Encoder Input | D3 (INT1) |
| Gun 1 Current | A0        |
| Gun 2 Current | A1        |
| Gun 3 Current | A2        |
| Gun 4 Current | A3        |

---

## Future Extensions

- Heartbeat / Watchdog mechanism
- Persistent storage of last config (EEPROM)
- Gun firing diagnostics
- GUI pattern editor with live preview
- Multi-trigger queuing for consecutive patterns

---

## Appendix

- All units:
  - Distance: millimeters (mm)
  - Current: Amperes (A)
  - Time: milliseconds (ms) unless otherwise noted

---

Prepared by: Or
Date: [To be filled in]
