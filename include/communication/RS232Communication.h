// RS232Communication.h
#pragma once

#include "CommunicationInterface.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <windows.h>
#include "Event.h"
#include "EventQueue.h"

// Structure to hold communication settings (now inside RS232Communication.h)
struct CommunicationSettings {
    std::string portName;
    unsigned long baudRate;
    char parity; // 'N', 'E', 'O' for None, Even, Odd
    int dataBits;
    int stopBits;
    char stx; // Changed to char
    char etx;
};

class RS232Communication : public CommunicationInterface {
public:
    RS232Communication(EventQueue<EventVariant>& eventQueue, const std::string &portName, unsigned long baudRate, char stx = 0, char etx = 0x03);
    
    virtual ~RS232Communication();

    virtual bool initialize() override;
    virtual bool send(const std::string &message) override;
    virtual std::string receive() override;
    virtual void close() override;

private:
    const char STX; // Start of Text (can be empty)
    const char ETX; // End of Text

    HANDLE hSerial_;
    std::string portName_;
    unsigned long baudRate_;

    // For asynchronous reception.
    std::thread receiveThread_;
    std::atomic<bool> receiving_;
    void receiveLoop();
    EventQueue<EventVariant>& eventQueue_;
    std::string receiveBuffer_; // Buffer to accumulate received data
};
