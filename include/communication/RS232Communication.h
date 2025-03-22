#pragma once

#include "CommunicationInterface.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <windows.h>

class RS232Communication : public CommunicationInterface {
public:
    // Constructor takes the port name (e.g., "COM3") and baud rate.
    RS232Communication(const std::string &portName, unsigned long baudRate);
    virtual ~RS232Communication();

    // CommunicationInterface methods.
    virtual bool initialize() override;
    virtual bool send(const std::string &message) override;
    virtual std::string receive() override;
    virtual void close() override;
    virtual void setDataReceivedCallback(std::function<void(const std::string&)> callback) override;

private:
    HANDLE hSerial_;
    std::string portName_;
    unsigned long baudRate_;

    // For asynchronous reception.
    std::function<void(const std::string&)> dataReceivedCallback_;
    std::thread receiveThread_;
    std::atomic<bool> receiving_;
    void receiveLoop();
    std::mutex callbackMutex_;

    // For graceful shutdown.
    HANDLE shutdownEvent_;
};
