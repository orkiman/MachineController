#ifndef EVENT_H
#define EVENT_H

#include <unordered_map>
#include <variant>
#include <string>
#include "io/IOChannel.h"

// Event for IO state changes
struct IOEvent {
    std::unordered_map<std::string, IOChannel> channels;
};

// Event for communication (TCP/IP, RS-232, etc.)
struct CommEvent {
    std::string communicationName;    // Identifier for the communication channel (e.g., "COM3")
    std::string message; // The received message
};

// #### GUI START ####

/**
 * Event for GUI updates - simplified to keyword-based approach
 * 
 * Usage examples:
 * 1. Set an output:
 *    GuiEvent event;
 *    event.keyword = "SetOutput";
 *    event.target = "o1";           // Output channel name
 *    event.intValue = 1;            // 1 = ON, 0 = OFF
 *    eventQueue.push(event);
 * 
 * 2. Toggle LED blinking:
 *    GuiEvent event;
 *    event.keyword = "SetVariable";
 *    event.target = "blinkLed0";
 *    eventQueue.push(event);
 * 
 * 3. Send a message to a communication port:
 *    GuiEvent event;
 *    event.keyword = "SendCommunicationMessage";
 *    event.target = "COM1";         // Communication port name
 *    event.data = "Hello, world!";  // Message to send
 *    eventQueue.push(event);
 * 
 * 4. Display a message in the GUI:
 *    GuiEvent event;
 *    event.keyword = "GuiMessage";
 *    event.data = "Operation completed successfully";
 *    event.target = "info";         // Message type (info, warning, error)
 *    eventQueue.push(event);
 */
struct GuiEvent {
    std::string keyword;   // Command keyword (e.g., "SetOutput", "GuiMessage")
    std::string data;      // Primary data or message content
    std::string target;    // Target identifier (output name, comm port, message type)
    int intValue = 0;      // Numeric value when needed
};
// #### GUI END ####

// Event for timers
struct TimerEvent {
    std::string timerName;    
};

// Event for termination/shutdown signal.
struct TerminationEvent {};

// Define a generic event type that can hold any of these event types
using EventVariant = std::variant<IOEvent, CommEvent, GuiEvent, TimerEvent, TerminationEvent>;

#endif // EVENT_H
