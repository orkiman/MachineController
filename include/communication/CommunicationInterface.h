#pragma once
#include <string>
#include <functional>

class CommunicationInterface {
public:
    virtual ~CommunicationInterface() {}

    // Initialize the communication channel.
    virtual bool initialize() = 0;

    // Send a message; returns true if successful.
    virtual bool send(const std::string& message) = 0;

    // Start the asynchronous/event-driven receive loop.
    virtual void startReceiving() = 0;

    // Close the communication channel.
    virtual void close() = 0;

    // Optional: set a callback to be invoked when new data is received.
    // virtual void setDataReceivedCallback(std::function<void(const std::string&)> callback) = 0;
};
