// RS232Communication.cpp
#include "communication/RS232Communication.h"
#include <iostream>
#include <thread>
#include <windows.h> // For Windows API functions

RS232Communication::RS232Communication(EventQueue<EventVariant>& eventQueue, const std::string& communicationName, const Config& config)
    : eventQueue_(eventQueue),
      communicationName_(communicationName),
      receiving_(false),
      hSerial_(INVALID_HANDLE_VALUE),
      config_(config) // Initialize config_ member
{
}

RS232Communication::RS232Communication(RS232Communication&& other) noexcept
    : eventQueue_(other.eventQueue_),
      communicationName_(std::move(other.communicationName_)),
      receiving_(other.receiving_.load()),
      hSerial_(other.hSerial_),
      config_(other.config_),
      portName_(std::move(other.portName_)),
      baudRate_(other.baudRate_)
{
    other.hSerial_ = INVALID_HANDLE_VALUE;
    other.receiving_ = false;
}

RS232Communication& RS232Communication::operator=(RS232Communication&& other) noexcept
{
    if (this != &other)
    {
        close(); // Close current connection if any
        
        eventQueue_ = other.eventQueue_;
        communicationName_ = std::move(other.communicationName_);
        receiving_ = other.receiving_.load();
        hSerial_ = other.hSerial_;
        config_ = other.config_;
        portName_ = std::move(other.portName_);
        baudRate_ = other.baudRate_;
        
        other.hSerial_ = INVALID_HANDLE_VALUE;
        other.receiving_ = false;
    }
    return *this;
}

RS232Communication::~RS232Communication()
{
    close();
}

bool RS232Communication::initialize()
{
    // Read configuration values from JSON.
    nlohmann::json commSettings = config_.getCommunicationSettings(); 
    if (commSettings.contains(communicationName_))
    {
        auto specificCommSettings = commSettings[communicationName_];
        portName_ = specificCommSettings.value("portName", "");
        baudRate_ = specificCommSettings.value("baudRate", 115200);
        
        // First get parity as string and then convert to char.
        std::string parityStr = specificCommSettings.value("parity", "N");
        parity_ = parityStr.empty() ? 'N' : parityStr[0];
        
        dataBits_ = specificCommSettings.value("dataBits", 8);
        stopBits_ = specificCommSettings.value("stopBits", 1);
        stx_ = parseCharSetting(specificCommSettings, "stx", 2);
        etx_ = parseCharSetting(specificCommSettings, "etx", 3);
    }
    else
    {
        getLogger()->warn("Communication settings for {} not found in config. Using default values.", communicationName_);
    }

    // Validate the settings.
    if (!validateSettings())
    {
        getLogger()->warn("Communication settings validation failed for {}. Aborting initialization.", communicationName_);
        return false;
    }

    // Open the serial port.
    hSerial_ = CreateFileA(portName_.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0, // exclusive access
                           NULL,
                           OPEN_EXISTING,
                           0,
                           NULL);
    if (hSerial_ == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Error opening serial port: " << portName_ << std::endl;
        return false;
    }

    // Configure the serial port.
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial_, &dcbSerialParams))
    {
        std::cerr << "Error getting serial state." << std::endl;
        return false;
    }

    dcbSerialParams.BaudRate = baudRate_;
    dcbSerialParams.ByteSize = dataBits_; // Use configured data bits.

    // Convert stopBits_ to the proper constant.
    if (stopBits_ == 1)
        dcbSerialParams.StopBits = ONESTOPBIT;
    else if (stopBits_ == 2)
        dcbSerialParams.StopBits = TWOSTOPBITS;
    else
    {
        getLogger()->warn("Unsupported stopBits value {}. Defaulting to 1 stop bit.", stopBits_);
        dcbSerialParams.StopBits = ONESTOPBIT;
    }

    // Convert parity char to the proper constant.
    switch (parity_)
    {
        case 'N': dcbSerialParams.Parity = NOPARITY; break;
        case 'E': dcbSerialParams.Parity = EVENPARITY; break;
        case 'O': dcbSerialParams.Parity = ODDPARITY; break;
        default:
            getLogger()->warn("Unsupported parity {}. Defaulting to NOPARITY.", parity_);
            dcbSerialParams.Parity = NOPARITY;
            break;
    }

    if (!SetCommState(hSerial_, &dcbSerialParams))
    {
        std::cerr << "Error setting serial state." << std::endl;
        return false;
    }

    // Set timeouts.
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(hSerial_, &timeouts))
    {
        std::cerr << "Error setting timeouts." << std::endl;
        return false;
    }

    // Set event mask.
    if (!SetCommMask(hSerial_, EV_RXCHAR))
    {
        std::cerr << "Error setting comm mask." << std::endl;
        return false;
    }

    // Start asynchronous reception.
    receiving_ = true;
    receiveThread_ = std::thread(&RS232Communication::receiveLoop, this);
    return true;
}

// Validation function for RS232 settings.
bool RS232Communication::validateSettings()
{
    bool valid = true;
    if (portName_.empty())
    {
        getLogger()->warn("Port name is empty. Please specify a valid port.");
        valid = false;
    }
    // You might want to define a minimum baud rate, e.g., 9600.
    if (baudRate_ < 9600)
    {
        getLogger()->warn("Baud rate ({}) is too low; recommended minimum is 9600.", baudRate_);
        valid = false;
    }
    if (!(parity_ == 'N' || parity_ == 'E' || parity_ == 'O'))
    {
        getLogger()->warn("Invalid parity value: {}. Only 'N', 'E', or 'O' are allowed.", parity_);
        valid = false;
    }
    if (!(dataBits_ == 7 || dataBits_ == 8))
    {
        getLogger()->warn("Invalid data bits value: {}. Only 7 or 8 are allowed.", dataBits_);
        valid = false;
    }
    if (!(stopBits_ == 1 || stopBits_ == 2))
    {
        getLogger()->warn("Invalid stop bits value: {}. Only 1 or 2 are allowed.", stopBits_);
        valid = false;
    }
    return valid;
}

// Helper function to parse char settings (STX, ETX)
char RS232Communication::parseCharSetting(const nlohmann::json &settings, const std::string &key, char defaultValue) const
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
            return 0; // Empty string means no STX/ETX.
        }
        else if (strValue.rfind("0x", 0) == 0)
        {
            return static_cast<char>(std::stoi(strValue, nullptr, 16));
        }
        else
        {
            return strValue[0]; // Take the first character.
        }
    }
    else
    {
        getLogger()->warn("Invalid type for {} setting. Using default value.", key);
        return defaultValue;
    }
}

bool RS232Communication::send(const std::string &message)
{
    if (hSerial_ == INVALID_HANDLE_VALUE)
        return false;
    DWORD bytesWritten;
    if (!WriteFile(hSerial_, message.c_str(), static_cast<DWORD>(message.size()), &bytesWritten, NULL))
    {
        std::cerr << "Error writing to serial port." << std::endl;
        return false;
    }
    return bytesWritten == message.size();
}

std::string RS232Communication::receive()
{
    std::string completeMessage;
    char buffer[256];
    DWORD bytesRead;

    if (hSerial_ == INVALID_HANDLE_VALUE)
        return "";

    while (true)
    {
        if (!ReadFile(hSerial_, buffer, sizeof(buffer) - 1, &bytesRead, NULL))
        {
            std::cerr << "Error reading from serial port." << std::endl;
            return "";
        }

        if (bytesRead == 0)
        {
            // No data read, return what we have so far (if any).
            return completeMessage;
        }

        buffer[bytesRead] = '\0'; // Null-terminate the buffer.
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
}

void RS232Communication::close()
{
    // Signal the shutdown event to unblock the waiting thread.
    if (hSerial_ != INVALID_HANDLE_VALUE)
    {
        CancelIoEx(hSerial_, NULL); // Cancel all pending I/O operations.
        PurgeComm(hSerial_, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR); // Discard buffers.
    }
    receiving_ = false;
    if (receiveThread_.joinable())
    {
        receiveThread_.join();
    }

    if (hSerial_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hSerial_);
        hSerial_ = INVALID_HANDLE_VALUE;
    }
}

void RS232Communication::receiveLoop()
{
    // Create an overlapped structure for asynchronous event notification.
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL)
    {
        std::cerr << "Failed to create overlapped event." << std::endl;
        return;
    }

    DWORD dwEventMask = 0;
    while (receiving_)
    {
        // Reset the overlapped event.
        ResetEvent(overlapped.hEvent);

        // Initiate waiting for a communication event (e.g., EV_RXCHAR) in overlapped mode.
        if (!WaitCommEvent(hSerial_, &dwEventMask, &overlapped))
        {
            if (GetLastError() != ERROR_IO_PENDING)
            {
                std::cerr << "WaitCommEvent failed." << std::endl;
                break;
            }
        }
        // Wait for the event to be signaled.
        DWORD bytesTransferred;
        if (!GetOverlappedResult(hSerial_, &overlapped, &bytesTransferred, TRUE))
        {
            std::cerr << "GetOverlappedResult failed." << std::endl;
            continue;
        }

        if (dwEventMask & EV_RXCHAR)
        {
            std::string msg = receive();
            if (!msg.empty())
            {
                // Create a CommEvent with port and message.
                CommEvent event;
                event.port = portName_; // Assuming portName_ holds the identifier.
                event.message = msg;
                eventQueue_.push(event); // eventQueue_ should be thread-safe.
            }
        }
    }

    CloseHandle(overlapped.hEvent);
}
