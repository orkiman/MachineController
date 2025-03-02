#ifndef EVENT_TYPES_H
#define EVENT_TYPES_H
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
    std::string message;
};

// Event for GUI updates
struct GUIEvent {
    std::string uiMessage;
};

// Event for timers
struct TimerEvent {
    int timerId;
};

// Define a generic event type that can hold any of these event types
using EventVariant = std::variant<IOEvent, CommEvent, GUIEvent, TimerEvent>;

#endif // EVENT_TYPES_H
