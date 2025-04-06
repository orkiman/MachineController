// RS232Communication.h
#pragma once

#include "CommunicationInterface.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <windows.h>
#include "Logger.h"
#include "Event.h"
#include "EventQueue.h"
#include "Config.h"


class RS232Communication : public CommunicationInterface {
public:
    // Note: A space is added between '>' and '&' for compatibility.
    RS232Communication(EventQueue<EventVariant> & eventQueue, 
                       const std::string & communicationName, 
                       const Config & config);
    
    RS232Communication(RS232Communication&& other) noexcept;
    RS232Communication& operator=(RS232Communication&& other) noexcept;
    
    RS232Communication(const RS232Communication&) = delete;
    RS232Communication& operator=(const RS232Communication&) = delete;
    
    virtual ~RS232Communication();

    virtual bool initialize() override;
    virtual bool send(const std::string & message) override;
    virtual std::string receive() override;
    virtual void close() override;

private:
    std::string communicationName_;
    std::string port_; // Previously portName_
    unsigned long baudRate_;
    char parity_; // 'N', 'E', 'O' for None, Even, Odd
    int dataBits_;
    int stopBits_;
    char stx_; // Changed to char
    char etx_;

    HANDLE hSerial_;    

    char parseCharSetting(const nlohmann::json & settings, 
                          const std::string & key, 
                          char defaultValue) const;

    // For asynchronous reception.
    std::thread receiveThread_;
    std::atomic<bool> receiving_;
    void receiveLoop();
    EventQueue<EventVariant>* eventQueue_;
    std::string receiveBuffer_; // Buffer to accumulate received data

    const Config* config_;

    bool validateSettings();

};
