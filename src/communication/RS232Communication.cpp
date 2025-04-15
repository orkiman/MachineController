// RS232Communication.cpp
#include "communication/RS232Communication.h"
#include <iostream>
#include <thread>
#include <windows.h> // For Windows API functions

RS232Communication::RS232Communication(EventQueue<EventVariant>& eventQueue, const std::string& communicationName, const Config& config)
    : eventQueue_(&eventQueue),
      communicationName_(communicationName),
      receiving_(false),
      hSerial_(INVALID_HANDLE_VALUE),
      config_(&config) // Initialize config_ member as pointer
{
}

RS232Communication::RS232Communication(RS232Communication&& other) noexcept
    : eventQueue_(other.eventQueue_),
      communicationName_(std::move(other.communicationName_)),
      receiving_(other.receiving_.load()),
      hSerial_(other.hSerial_),
      config_(other.config_),
      port_(std::move(other.port_)),
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
        port_ = std::move(other.port_);
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
    // If already initialized, close first to ensure clean state
    if (hSerial_ != INVALID_HANDLE_VALUE) {
        getLogger()->info("Port {} already open, closing before reinitializing", communicationName_);
        close();
    }

    // Read configuration values from JSON.
    nlohmann::json commSettings = config_->getCommunicationSettings(); 
    if (commSettings.contains(communicationName_))
    {
        auto specificCommSettings = commSettings[communicationName_];
        // Try to get port first, fall back to portName for backward compatibility
        if (specificCommSettings.contains("port")) {
            port_ = specificCommSettings.value("port", "");
        } else {
            port_ = specificCommSettings.value("portName", "");
        }
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

    // Make sure receiving_ is false before opening the port
    receiving_ = false;

    // Open the serial port.
    hSerial_ = CreateFileA(port_.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0, // exclusive access
                           NULL,
                           OPEN_EXISTING,
                           0,
                           NULL);
    if (hSerial_ == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Error opening serial port: " << port_ << std::endl;
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
        CloseHandle(hSerial_);
        hSerial_ = INVALID_HANDLE_VALUE;
        return false;
    }

    // Make sure any previous thread is properly joined before starting a new one
    if (receiveThread_.joinable()) {
        getLogger()->warn("Previous receive thread still active during initialization of {}, attempting to join", communicationName_);
        try {
            receiving_ = false;
            receiveThread_.join();
        } catch (const std::exception& e) {
            getLogger()->error("Exception while joining previous receive thread: {}", e.what());
        }
    }

    // Start asynchronous reception.
    receiving_ = true;
    try {
        receiveThread_ = std::thread(&RS232Communication::receiveLoop, this);
        getLogger()->info("Started receive thread for port {}", communicationName_);
    } catch (const std::exception& e) {
        getLogger()->error("Failed to start receive thread for {}: {}", communicationName_, e.what());
        receiving_ = false;
        CloseHandle(hSerial_);
        hSerial_ = INVALID_HANDLE_VALUE;
        return false;
    }
    
    return true;
}

// Validation function for RS232 settings.
bool RS232Communication::validateSettings()
{
    bool valid = true;
    if (port_.empty())
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
    DWORD commErrors;
    COMSTAT commStatus;

    if (hSerial_ == INVALID_HANDLE_VALUE)
        return "";

    // Check if there's any data available to read
    if (!ClearCommError(hSerial_, &commErrors, &commStatus) || commStatus.cbInQue == 0) {
        // No data available or error occurred
        return "";
    }

    // Set up a non-blocking read with timeout
    COMMTIMEOUTS timeouts;
    if (GetCommTimeouts(hSerial_, &timeouts)) {
        // Save original timeouts
        COMMTIMEOUTS originalTimeouts = timeouts;
        
        // Set short timeouts for non-blocking behavior
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        
        SetCommTimeouts(hSerial_, &timeouts);
        
        // Read available data
        if (!ReadFile(hSerial_, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            getLogger()->error("Error reading from serial port: {}", GetLastError());
            // Restore original timeouts
            SetCommTimeouts(hSerial_, &originalTimeouts);
            return "";
        }
        
        // Restore original timeouts
        SetCommTimeouts(hSerial_, &originalTimeouts);
        
        if (bytesRead == 0) {
            // No data read
            return "";
        }
        
        // Process the data
        buffer[bytesRead] = '\0'; // Null-terminate the buffer
        receiveBuffer_ += buffer;
    } else {
        getLogger()->error("Failed to get comm timeouts: {}", GetLastError());
        return "";
    }

    // Process the receive buffer to extract complete messages
    size_t stxPos = std::string::npos;
    if (stx_ != 0) {
        stxPos = receiveBuffer_.find(stx_);
    }
    size_t etxPos = receiveBuffer_.find(etx_);

    if (stx_ == 0) {
        if (etxPos != std::string::npos) {
            // No STX, but ETX found. Return everything up to ETX.
            completeMessage = receiveBuffer_.substr(0, etxPos);
            receiveBuffer_.erase(0, etxPos + 1);
            return completeMessage;
        }
    } else {
        if (stxPos != std::string::npos && etxPos != std::string::npos) {
            if (etxPos > stxPos) {
                // STX and ETX found, ETX after STX. Return the message between them.
                completeMessage = receiveBuffer_.substr(stxPos + 1, etxPos - stxPos - 1);
                receiveBuffer_.erase(0, etxPos + 1);
                return completeMessage;
            } else {
                // ETX before STX. Discard everything up to STX.
                receiveBuffer_.erase(0, stxPos);
            }
        } else if (stxPos != std::string::npos && etxPos == std::string::npos) {
            // STX found, but no ETX. Discard everything before STX.
            receiveBuffer_.erase(0, stxPos);
        }
    }
    
    // If we get here, no complete message was found
    return "";
}

void RS232Communication::close()
{
    // Use a mutex to ensure thread safety during close operations
    static std::mutex closeMutex;
    std::lock_guard<std::mutex> lock(closeMutex);
    
    // Check if already closed
    if (hSerial_ == INVALID_HANDLE_VALUE && !receiving_ && !receiveThread_.joinable()) {
        getLogger()->debug("Port {} already closed, skipping close operation", communicationName_);
        return;
    }
    
    getLogger()->debug("Closing port {}", communicationName_);
    
    // Signal the receive thread to stop
    receiving_ = false;
    
    // Cancel any pending I/O operations to unblock the receive thread
    if (hSerial_ != INVALID_HANDLE_VALUE)
    {
        // Cancel any pending I/O operations
        CancelIo(hSerial_);
        
        // Add a small delay to allow the thread to react to cancellation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Now it's safe to join the thread
    if (receiveThread_.joinable())
    {
        try {
            receiveThread_.join();
        } catch (const std::exception& e) {
            getLogger()->error("Exception while joining receive thread for {}: {}", communicationName_, e.what());
        }
    }

    // Close the handle after the thread has been joined
    if (hSerial_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hSerial_);
        hSerial_ = INVALID_HANDLE_VALUE;
    }
    
    // Clear the receive buffer
    receiveBuffer_.clear();
    
    getLogger()->debug("Port {} closed successfully", communicationName_);
}

void RS232Communication::receiveLoop()
{
    // Create an overlapped structure for asynchronous event notification.
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL)
    {
        getLogger()->error("Failed to create overlapped event for {}", communicationName_);
        return;
    }

    getLogger()->debug("Started receive loop for {}", communicationName_);
    DWORD dwEventMask = 0;
    while (receiving_)
    {
        // Check if handle is valid before proceeding
        if (hSerial_ == INVALID_HANDLE_VALUE) {
            getLogger()->error("Serial handle invalid in receive loop for {}", communicationName_);
            break;
        }

        // Reset the overlapped event.
        ResetEvent(overlapped.hEvent);

        // Initiate waiting for a communication event (e.g., EV_RXCHAR) in overlapped mode.
        if (!WaitCommEvent(hSerial_, &dwEventMask, &overlapped))
        {
            DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING)
            {
                getLogger()->error("WaitCommEvent failed for {} with error {}", communicationName_, error);
                break;
            }
        }

        // Wait for the event to be signaled with a timeout to allow checking receiving_ flag
        DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 100); // 100ms timeout
        
        // Check if we should exit the loop
        if (!receiving_) {
            getLogger()->debug("Receive loop for {} terminating due to receiving_ flag", communicationName_);
            break;
        }

        // Process the result based on the wait outcome
        if (waitResult == WAIT_OBJECT_0) {
            // Event was signaled, get the result
            DWORD bytesTransferred;
            if (!GetOverlappedResult(hSerial_, &overlapped, &bytesTransferred, FALSE))
            {
                DWORD error = GetLastError();
                if (error != ERROR_OPERATION_ABORTED) { // Ignore aborted operations (normal during shutdown)
                    getLogger()->error("GetOverlappedResult failed for {} with error {}", communicationName_, error);
                }
                continue;
            }

            if (dwEventMask & EV_RXCHAR)
            {
                std::string msg = receive();
                if (!msg.empty())
                {
                    // Create a CommEvent with communication name and message.
                    CommEvent event;
                    event.communicationName = communicationName_; // Communication channel identifier
                    event.message = msg;
                    eventQueue_->push(event); // eventQueue_ should be thread-safe.
                }
            }
        }
        else if (waitResult == WAIT_TIMEOUT) {
            // Timeout occurred, just continue the loop
            continue;
        }
        else {
            // Some other error occurred
            getLogger()->error("WaitForSingleObject failed for {} with result {}", communicationName_, waitResult);
            break;
        }
    }

    CloseHandle(overlapped.hEvent);
    getLogger()->debug("Exited receive loop for {}", communicationName_);
}
