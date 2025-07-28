# Arduino Glue Controller Implementation

## Overview

This Arduino program implements a complete glue controller that communicates with the MachineController application via JSON messages.

## Features

- **JSON Protocol Communication** - Full bidirectional communication with the PC
- **4-Gun Support** - Individual control of up to 4 glue guns
- **Encoder Positioning** - Precise positioning using rotary encoder
- **Sensor Integration** - Optical sensor for page detection
- **Real-time Application** - Dynamic glue application based on configuration

## Hardware Setup

### Pin Assignments
- Pin 2: Encoder A (interrupt)
- Pin 3: Encoder B (interrupt)  
- Pin 4: Optical sensor
- Pins 8-11: Gun 1-4 solenoids
- Pin 13: Status LED

## Installation

1. Install Arduino IDE
2. Install ArduinoJson library (v6.x+)
3. Upload GlueController.ino to Arduino Mega 2560

## Testing

Send test messages via Serial Monitor (115200 baud):
- `{"type":"heartbeat"}` - Get status
- `{"type":"config","encoder":1.5,"sensorOffset":10}` - Set config
- `{"type":"run"}` - Start operation
- `{"type":"stop"}` - Stop operation
