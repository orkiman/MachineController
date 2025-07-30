/**
 * Arduino Glue Controller - Main Entry Point
 * 
 * This file contains the main Arduino setup() and loop() functions.
 * The actual implementation is in GlueController.cpp
 */

#include "GlueController.h"

// The setup() function runs once when the board starts up
void setup() {
  ::setup();  // Call the setup function from GlueController
}

// The loop() function runs continuously after setup()
void loop() {
  ::loop();  // Call the loop function from GlueController
}
