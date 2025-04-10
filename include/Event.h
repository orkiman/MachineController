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
    std::string port;    // Identifier for the communication port (e.g., "COM3")
    std::string message; // The received message
};

// #### GUI START ####

// Event for GUI updates ###
enum class GuiEventType {
    ButtonPress,
    SetOutput,
    SetVariable,
    ParameterChange,
    StatusRequest,
    StatusUpdate,
    ErrorMessage,
    SendCommunicationMessage,
    SendTestMessage,
    GuiMessage
};

struct GuiEvent {
    GuiEventType type;
    std::string data; // For logging or display
    std::string identifier;  // Identifier for the output to set (if needed)
    int intValue = 0;      // For numeric parameters
};
// #### GUI END ####

// Event for timers
struct TimerEvent {
    int timerId;
};

// Event for termination/shutdown signal.
struct TerminationEvent {};

// Define a generic event type that can hold any of these event types
using EventVariant = std::variant<IOEvent, CommEvent, GuiEvent, TimerEvent, TerminationEvent>;

#endif // EVENT_H
