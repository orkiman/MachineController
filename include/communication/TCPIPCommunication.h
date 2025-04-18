#pragma once

#include "CommunicationInterface.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Logger.h"
#include "Event.h"
#include "EventQueue.h"
#include "Config.h"

#pragma comment(lib, "Ws2_32.lib")

class TCPIPCommunication : public CommunicationInterface {
public:
    TCPIPCommunication(EventQueue<EventVariant>& eventQueue, 
                      const std::string& communicationName, 
                      const Config& config);
    
    TCPIPCommunication(TCPIPCommunication&& other) noexcept;
    TCPIPCommunication& operator=(TCPIPCommunication&& other) noexcept;
    
    TCPIPCommunication(const TCPIPCommunication&) = delete;
    TCPIPCommunication& operator=(const TCPIPCommunication&) = delete;
    
    virtual ~TCPIPCommunication();

    virtual bool initialize() override;
    virtual bool send(const std::string& message) override;
    virtual void startReceiving() override;
    virtual void close() override;

private:
    std::string communicationName_;
    std::string ipAddress_;
    int port_;
    int timeout_ms_;
    char stx_; // Start of text character
    char etx_; // End of text character
    int offset_; // Offset within the received message for data placement

    SOCKET socket_;
    bool connected_;

    char parseCharSetting(const nlohmann::json& settings, 
                          const std::string& key, 
                          char defaultValue) const;

    // For asynchronous reception
    std::thread receiveThread_;
    std::atomic<bool> receiving_;
    void receiveLoop();
    EventQueue<EventVariant>* eventQueue_;
    std::string receiveBuffer_; // Buffer to accumulate received data

    const Config* config_;

    bool validateSettings();
    bool initializeWinsock();
};
