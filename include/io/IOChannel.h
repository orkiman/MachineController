#ifndef IO_CHANNEL_H
#define IO_CHANNEL_H

enum class IOEventType {
    None,
    Rising,
    Falling
};

enum class IOType {
    Input,
    Output
};

struct IOChannel {
    int pin;                     // Physical pin number (within its port)
    std::string ioPort;          // Port identifier (e.g., "A", "B", "CL", "CH")
    std::string name;            // Logical name, e.g., "O0_motor", "I7_eye1"
    std::string description;     // Human-readable description
    IOType type;                 // Input or Output
    int state;                   // Current state: 0 or 1
    IOEventType eventType;       // Event trigger type: Rising, Falling, or None
};

#endif // IO_CHANNEL_H
