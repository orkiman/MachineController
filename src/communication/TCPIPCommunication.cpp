#include "communication/TCPIPCommunication.h"
#include <iostream>
#include <thread>

TCPIPCommunication::TCPIPCommunication(EventQueue<EventVariant>& eventQueue, const std::string& communicationName, const Config& config)
    : eventQueue_(&eventQueue),
      communicationName_(communicationName),
      receiving_(false),
      connected_(false),
      socket_(INVALID_SOCKET),
      config_(&config)
{
}

TCPIPCommunication::TCPIPCommunication(TCPIPCommunication&& other) noexcept
    : eventQueue_(other.eventQueue_),
      communicationName_(std::move(other.communicationName_)),
      receiving_(other.receiving_.load()),
      connected_(other.connected_),
      socket_(other.socket_),
      config_(other.config_),
      ipAddress_(std::move(other.ipAddress_)),
      port_(other.port_),
      timeout_ms_(other.timeout_ms_)
{
    other.socket_ = INVALID_SOCKET;
    other.receiving_ = false;
    other.connected_ = false;
}

TCPIPCommunication& TCPIPCommunication::operator=(TCPIPCommunication&& other) noexcept
{
    if (this != &other)
    {
        close(); // Close current connection if any
        
        eventQueue_ = other.eventQueue_;
        communicationName_ = std::move(other.communicationName_);
        receiving_ = other.receiving_.load();
        connected_ = other.connected_;
        socket_ = other.socket_;
        config_ = other.config_;
        ipAddress_ = std::move(other.ipAddress_);
        port_ = other.port_;
        timeout_ms_ = other.timeout_ms_;
        
        other.socket_ = INVALID_SOCKET;
        other.receiving_ = false;
        other.connected_ = false;
    }
    return *this;
}

TCPIPCommunication::~TCPIPCommunication()
{
    close();
}

bool TCPIPCommunication::initialize()
{
    // Read configuration values from JSON
    nlohmann::json commSettings = config_->getCommunicationSettings();
    if (commSettings.contains(communicationName_))
    {
        auto specificCommSettings = commSettings[communicationName_];
        
        // Check if there's a tcpip object in the settings
        if (specificCommSettings.contains("tcpip"))
        {
            auto tcpipSettings = specificCommSettings["tcpip"];
            ipAddress_ = tcpipSettings.value("ip", "127.0.0.1");
            port_ = tcpipSettings.value("port", 8080);
            timeout_ms_ = tcpipSettings.value("timeout_ms", 1000);
        }
        else
        {
            getLogger()->warn("TCPIP settings for {} not found in config. Using default values.", communicationName_);
            ipAddress_ = "127.0.0.1";
            port_ = 8080;
            timeout_ms_ = 1000;
        }
        
        // Get STX/ETX values
        stx_ = parseCharSetting(specificCommSettings, "stx", 2);
        etx_ = parseCharSetting(specificCommSettings, "etx", 3);
    }
    else
    {
        getLogger()->warn("Communication settings for {} not found in config. Using default values.", communicationName_);
        ipAddress_ = "127.0.0.1";
        port_ = 8080;
        timeout_ms_ = 1000;
        stx_ = 2; // STX
        etx_ = 3; // ETX
    }

    // Validate the settings
    if (!validateSettings())
    {
        getLogger()->warn("Communication settings validation failed for {}. Aborting initialization.", communicationName_);
        return false;
    }

    // Initialize Winsock
    if (!initializeWinsock())
    {
        getLogger()->error("Failed to initialize Winsock for {}.", communicationName_);
        return false;
    }

    // Create socket
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET)
    {
        getLogger()->error("Failed to create socket for {}. Error: {}", communicationName_, WSAGetLastError());
        WSACleanup();
        return false;
    }

    // Set socket timeout
    DWORD timeout = timeout_ms_;
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
    {
        getLogger()->warn("Failed to set receive timeout for {}. Error: {}", communicationName_, WSAGetLastError());
    }
    if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
    {
        getLogger()->warn("Failed to set send timeout for {}. Error: {}", communicationName_, WSAGetLastError());
    }

    // Connect to server
    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(port_);
    
    // Convert IP address from string to network address
    inet_pton(AF_INET, ipAddress_.c_str(), &(clientService.sin_addr));

    if (connect(socket_, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR)
    {
        getLogger()->error("Failed to connect to server for {}. Error: {}", communicationName_, WSAGetLastError());
        closesocket(socket_);
        WSACleanup();
        socket_ = INVALID_SOCKET;
        return false;
    }

    connected_ = true;
    getLogger()->debug("Successfully connected to {}:{} for {}", ipAddress_, port_, communicationName_);

    // Start asynchronous reception
    receiving_ = true;
    receiveThread_ = std::thread(&TCPIPCommunication::receiveLoop, this);
    return true;
}

bool TCPIPCommunication::validateSettings()
{
    bool valid = true;
    
    if (ipAddress_.empty())
    {
        getLogger()->warn("IP address is empty for {}. Please specify a valid IP address.", communicationName_);
        valid = false;
    }
    
    if (port_ <= 0 || port_ > 65535)
    {
        getLogger()->warn("Invalid port number ({}) for {}. Port must be between 1 and 65535.", port_, communicationName_);
        valid = false;
    }
    
    if (timeout_ms_ < 0)
    {
        getLogger()->warn("Invalid timeout value ({} ms) for {}. Timeout must be non-negative.", timeout_ms_, communicationName_);
        valid = false;
    }
    
    return valid;
}

bool TCPIPCommunication::initializeWinsock()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        getLogger()->error("WSAStartup failed for {}. Error: {}", communicationName_, result);
        return false;
    }
    return true;
}

char TCPIPCommunication::parseCharSetting(const nlohmann::json &settings, const std::string &key, char defaultValue) const
{
    if (!settings.contains(key))
    {
        return defaultValue;
    }

    const auto &value = settings[key];
    if (value.is_number_integer())
    {
        return static_cast<char>(value.get<int>());
    }
    else if (value.is_string())
    {
        std::string strValue = value.get<std::string>();
        if (strValue.empty())
        {
            return 0; // Empty string means no STX/ETX
        }
        else if (strValue.rfind("0x", 0) == 0)
        {
            return static_cast<char>(std::stoi(strValue, nullptr, 16));
        }
        else
        {
            return strValue[0]; // Take the first character
        }
    }
    else
    {
        getLogger()->warn("Invalid type for {} setting in {}. Using default value.", key, communicationName_);
        return defaultValue;
    }
}

bool TCPIPCommunication::send(const std::string &message)
{
    if (!connected_ || socket_ == INVALID_SOCKET)
    {
        getLogger()->error("Cannot send message through {}. Socket not connected.", communicationName_);
        return false;
    }
    
    int result = ::send(socket_, message.c_str(), static_cast<int>(message.size()), 0);
    if (result == SOCKET_ERROR)
    {
        getLogger()->error("Failed to send message through {}. Error: {}", communicationName_, WSAGetLastError());
        return false;
    }
    
    return true;
}

std::string TCPIPCommunication::receive()
{
    std::string completeMessage;
    char buffer[1024];
    int bytesRead;

    if (!connected_ || socket_ == INVALID_SOCKET)
    {
        return "";
    }

    // Use blocking socket with timeout (already set in initialize())
    bytesRead = recv(socket_, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT)
        {
            // Timeout is normal, just return empty string
            return "";
        }
        else
        {
            getLogger()->error("Error receiving data from {}. Error: {}", communicationName_, error);
            return "";
        }
    }
    else if (bytesRead == 0)
    {
        // Connection closed
        getLogger()->warn("Connection closed for {}", communicationName_);
        connected_ = false;
        return "";
    }
    else
    {
        buffer[bytesRead] = '\0';  // Null-terminate the buffer
        receiveBuffer_ += buffer;

        size_t stxPos = std::string::npos;
        if (stx_ != 0)
        {
            stxPos = receiveBuffer_.find(stx_);
        }
        size_t etxPos = receiveBuffer_.find(etx_);

        if (stx_ == 0)
        {
            if (etxPos != std::string::npos)
            {
                // No STX, but ETX found. Return everything up to ETX.
                completeMessage = receiveBuffer_.substr(0, etxPos);
                receiveBuffer_.erase(0, etxPos + 1);
                return completeMessage;
            }
        }
        else
        {
            if (stxPos != std::string::npos && etxPos != std::string::npos)
            {
                if (etxPos > stxPos)
                {
                    // STX and ETX found, ETX after STX. Return the message between them.
                    completeMessage = receiveBuffer_.substr(stxPos + 1, etxPos - stxPos - 1);
                    receiveBuffer_.erase(0, etxPos + 1);
                    return completeMessage;
                }
                else
                {
                    // ETX before STX. Discard everything up to STX.
                    receiveBuffer_.erase(0, stxPos);
                }
            }
            else if (stxPos != std::string::npos && etxPos == std::string::npos)
            {
                // STX found, but no ETX. Discard everything before STX.
                receiveBuffer_.erase(0, stxPos);
            }
        }
    }

    return "";
}

void TCPIPCommunication::close()
{
    receiving_ = false;
    if (receiveThread_.joinable())
    {
        receiveThread_.join();
    }

    if (connected_ && socket_ != INVALID_SOCKET)
    {
        shutdown(socket_, SD_BOTH);
        closesocket(socket_);
        WSACleanup();
        socket_ = INVALID_SOCKET;
        connected_ = false;
        getLogger()->debug("Closed connection for {}", communicationName_);
    }
}

void TCPIPCommunication::receiveLoop()
{
    // Create a socket set for select() to monitor our socket
    fd_set readSet;
    timeval timeout;
    
    while (receiving_ && connected_)
    {
        // Clear the set and add our socket
        FD_ZERO(&readSet);
        FD_SET(socket_, &readSet);
        
        // Set timeout (can be adjusted as needed)
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000; // 500ms
        
        // Wait for socket to be ready for reading
        int selectResult = select(0, &readSet, NULL, NULL, &timeout);
        
        if (selectResult == SOCKET_ERROR)
        {
            getLogger()->error("Select failed in receive loop for {}. Error: {}", 
                              communicationName_, WSAGetLastError());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Check if our socket has data
        if (selectResult > 0 && FD_ISSET(socket_, &readSet))
        {
            std::string msg = receive();
            if (!msg.empty())
            {
                // Create a CommEvent with communication name and message
                CommEvent event;
                event.communicationName = communicationName_; // Use the existing communication name
                event.message = msg;
                eventQueue_->push(event); // eventQueue_ should be thread-safe
            }
        }
    }
}
