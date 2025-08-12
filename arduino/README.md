# Arduino Glue Controller Implementation

## Overview

Glue controller firmware for MachineController. Communicates via framed JSON over Serial.

## Quick Facts

- Board: Arduino Uno R4 (Minima/WiFi) recommended
- Serial: 115200 baud
- Framing: STX (0x02) JSON ETX (0x03)
- Guns: up to 4

## Pins (Uno R4)

- 2: Encoder A
- 4: Optical sensor
- 8..11: Gun 1..4 outputs
- 13: Status LED

## Install

1) Arduino IDE + board package for Uno R4
2) Library: ArduinoJson v6+
3) Open and upload `arduino/GlueController/GlueController.ino`

## Protocol (Inbound)

Supported message types: `controller_setup` (alias: `config`), `test`, `calibrate`, `heartbeat`

Framing example:
```
[STX]{"type":"controller_setup",...}[ETX]
```

Controller setup (preferred):
```
[STX]{
  "type":"controller_setup",
  "enabled":true,
  "encoder":100.0,
  "sensorOffset":12,
  "startCurrent":1.0,
  "startDuration":500,
  "holdCurrent":0.5,
  "minimumSpeed":0.0,
  "dotSize":"medium",
  "guns":[
    {"gunId":0,"enabled":true,"rows":[{"from":10.0,"to":50.0,"space":0.0}]}
  ]
}[ETX]
```

Test:
```
[STX]{"type":"test","state":"on"}[ETX]
[STX]{"type":"test","gun":2,"state":"off"}[ETX]
```

Calibrate:
```
[STX]{"type":"calibrate","pageLength":1000}[ETX]
```

Heartbeat (no-op):
```
[STX]{"type":"heartbeat"}[ETX]
```

Protocol (Outbound):
```
{"type":"calibration_result","pulsesPerPage":12345}
```

Notes:
- `guns[].rows` are in mm; firmware converts to pulses using `encoder` and `sensorOffset`. Overlaps are merged.
- STATUS_LED follows `enabled` from setup.
- Removed/unsupported: `plan`, `run`, `stop`.

## Quick Test

Serial Monitor @ 115200, send (remember to add STX/ETX framing if your tool doesn’t):
- `{"type":"heartbeat"}`
- `{"type":"config","enabled":true,"encoder":100}`
