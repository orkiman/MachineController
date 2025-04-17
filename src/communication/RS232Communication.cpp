// RS232Communication.cpp
#include "communication/RS232Communication.h"
#include <iostream>
#include <thread>
#include <windows.h> // For Windows API functions

RS232Communication::RS232Communication(EventQueue<EventVariant>& eventQueue, const std::string& communicationName, const Config& config)
    : eventQueue_(&eventQueue),
      communicationName_(communicationName),
      receiving_(false),
      stopRequested_(false), // Explicitly initialize
      hSerial_(INVALID_HANDLE_VALUE),
      config_(&config) // Initialize config_ member as pointer
{
    getLogger()->debug("[{}] RS232Communication constructor for '{}'", static_cast<void*>(this), communicationName_);
}

RS232Communication::RS232Communication(RS232Communication&& other) noexcept
    : eventQueue_(other.eventQueue_),
      communicationName_(std::move(other.communicationName_)),
      receiving_(other.receiving_.load()),
      stopRequested_(other.stopRequested_.load()),
      hSerial_(other.hSerial_),
      config_(other.config_),
      port_(std::move(other.port_)),
      baudRate_(other.baudRate_)
{
    other.hSerial_ = INVALID_HANDLE_VALUE;
    other.receiving_ = false;
    other.stopRequested_ = false;
}

RS232Communication& RS232Communication::operator=(RS232Communication&& other) noexcept
{
    if (this != &other)
    {
        close(); // Close current connection if any
        
        eventQueue_ = other.eventQueue_;
        communicationName_ = std::move(other.communicationName_);
        receiving_ = other.receiving_.load();
        stopRequested_ = other.stopRequested_.load();
        hSerial_ = other.hSerial_;
        config_ = other.config_;
        port_ = std::move(other.port_);
        baudRate_ = other.baudRate_;
        
        other.hSerial_ = INVALID_HANDLE_VALUE;
        other.receiving_ = false;
        other.stopRequested_ = false;
    }
    return *this;
}

RS232Communication::~RS232Communication()
{
    getLogger()->debug("[{}] RS232Communication destructor for '{}'", __PRETTY_FUNCTION__, communicationName_);
    close();
}

bool RS232Communication::initialize()
{
    getLogger()->debug("[{}] RS232Communication initialize() started for '{}'", __PRETTY_FUNCTION__, communicationName_);
    // If already initialized, close first to ensure clean state
    if (hSerial_ != INVALID_HANDLE_VALUE) {
        getLogger()->warn("[{}] Port {} already open in initialize(), but close() is not called here by design. This should not happen.", __PRETTY_FUNCTION__, communicationName_);

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
                           FILE_FLAG_OVERLAPPED,
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

    // Explicitly disable hardware flow control
    dcbSerialParams.fOutxCtsFlow = FALSE;
    dcbSerialParams.fRtsControl = RTS_CONTROL_ENABLE;
    dcbSerialParams.fOutxDsrFlow = FALSE;
    dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;

    // Explicitly disable software flow control (XON/XOFF)
    dcbSerialParams.fInX = FALSE;
    dcbSerialParams.fOutX = FALSE;

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
        getLogger()->debug("Started receive thread for port {}", communicationName_);
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
    if (hSerial_ == INVALID_HANDLE_VALUE) {
        getLogger()->error("[send] Invalid serial handle for port {}", port_);
        return false;
    }

    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!overlapped.hEvent) {
        getLogger()->error("[send] Failed to create overlapped event for port {}", port_);
        return false;
    }

    DWORD bytesWritten = 0;
    BOOL writeResult = WriteFile(hSerial_, message.c_str(), static_cast<DWORD>(message.size()), &bytesWritten, &overlapped);
    DWORD lastError = GetLastError();
    if (!writeResult && lastError == ERROR_IO_PENDING) {
        DWORD waitRes = WaitForSingleObject(overlapped.hEvent, 500);
        if (waitRes == WAIT_OBJECT_0) {
            if (!GetOverlappedResult(hSerial_, &overlapped, &bytesWritten, FALSE)) {
                DWORD err = GetLastError();
                getLogger()->error("[send] Overlapped write failed on port {}: {}", port_, err);
                CloseHandle(overlapped.hEvent);
                return false;
            }
        } else if (waitRes == WAIT_TIMEOUT) {
            getLogger()->error("[send] WriteFile timed out after 500 ms on port {}", port_);
            CancelIo(hSerial_);
            CloseHandle(overlapped.hEvent);
            return false;
        } else {
            DWORD err = GetLastError();
            getLogger()->error("[send] WaitForSingleObject failed on port {}: {}", port_, err);
            CloseHandle(overlapped.hEvent);
            return false;
        }
    } else if (!writeResult) {
        getLogger()->error("[send] WriteFile failed on port {}: {}", port_, lastError);
        CloseHandle(overlapped.hEvent);
        return false;
    }

    if (!FlushFileBuffers(hSerial_)) {
        DWORD err = GetLastError();
        getLogger()->error("[send] Error flushing serial port buffers for {}: {}", port_, err);
        CloseHandle(overlapped.hEvent);
        return false;
    }
    CloseHandle(overlapped.hEvent);
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
    getLogger()->debug("[{}] RS232Communication close() started for '{}'", static_cast<void*>(this), communicationName_);
    // Use a mutex to ensure thread safety during close operations
    static std::mutex closeMutex;
    std::lock_guard<std::mutex> lock(closeMutex);
    
    // --- Set stop flag early ---
    stopRequested_ = true;
    // ---

    // Check if already closed
    if (hSerial_ == INVALID_HANDLE_VALUE && !receiving_ && !receiveThread_.joinable()) {
        getLogger()->debug("[{}] Port {} already closed, skipping close operation", __PRETTY_FUNCTION__, communicationName_);
        return;
    }
    
    getLogger()->debug("[{}] Closing port {}", __PRETTY_FUNCTION__, communicationName_);
    
    // Signal the receive thread to stop
    receiving_ = false;
    
    // Attempt to cancel pending I/O operations
    if (hSerial_ != INVALID_HANDLE_VALUE) {
        if (!CancelIoEx(hSerial_, nullptr)) { // Cancel all I/O from this thread for the handle
            DWORD error = GetLastError();
            // ERROR_NOT_FOUND means there were no pending operations to cancel, which is fine.
            if (error != ERROR_NOT_FOUND) {
                getLogger()->error("[{}] Error cancelling I/O operations for {}: {}", __PRETTY_FUNCTION__, communicationName_, error);
            }
        }
    }

    // Close the handle *before* joining the thread.
    // Closing the handle should also cause blocking operations on it to fail/return.
    if (hSerial_ != INVALID_HANDLE_VALUE) {
        getLogger()->debug("[{}] close: Closing handle for {}...", __PRETTY_FUNCTION__, communicationName_);
        CloseHandle(hSerial_);
        hSerial_ = INVALID_HANDLE_VALUE; // Mark as closed
    }

    // Now it's safe to join the thread
    if (receiveThread_.joinable())
    {
        getLogger()->debug("Attempting to join receive thread for {}", communicationName_);
        getLogger()->flush(); // Explicitly flush logs before joining thread
        try {
            receiveThread_.join();
            getLogger()->debug("Successfully joined receive thread for {}", communicationName_);
        } catch (const std::exception& e) {
            getLogger()->error("Exception while joining receive thread for {}: {}", communicationName_, e.what());
        }
    }

    // Clear the receive buffer
    receiveBuffer_.clear();
    
    getLogger()->debug("[{}] RS232Communication close() finished for '{}' Port closed successfully", __PRETTY_FUNCTION__, communicationName_);
}

void RS232Communication::receiveLoop()
{
    getLogger()->debug("[{}] RS232Communication receiveLoop() started for '{}'", __PRETTY_FUNCTION__, communicationName_);

    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        getLogger()->error("[{}] Failed to create overlapped event for {}: {}", __PRETTY_FUNCTION__, communicationName_, GetLastError());
        return;
    }

    // Set the event mask to wait for data arrival (EV_RXCHAR)
    if (!SetCommMask(hSerial_, EV_RXCHAR)) {
        getLogger()->error("[{}] Failed to set comm mask for {}: {}", __PRETTY_FUNCTION__, communicationName_, GetLastError());
        CloseHandle(overlapped.hEvent);
        return;
    }

    DWORD dwCommEvent;
    DWORD dwRead;
    char buffer[1024]; // Local buffer for reading

    // Use stopRequested_ as main loop condition
    while (!stopRequested_) {
        // Wait for a communication event (e.g., data arrival)
        if (!WaitCommEvent(hSerial_, &dwCommEvent, &overlapped)) {
            DWORD error = GetLastError();
            if (error == ERROR_IO_PENDING) {
                // Use non-blocking GetOverlappedResult and manual wait
                DWORD bytesTransferred = 0;
                bool operationCompleted = false;

                // Check initial status without waiting
                if (GetOverlappedResult(hSerial_, &overlapped, &bytesTransferred, FALSE)) {
                     operationCompleted = true; // Completed synchronously or already finished
                } else {
                    error = GetLastError();
                    if (error == ERROR_IO_INCOMPLETE) {
                        // Operation is pending, wait manually
                        while (!stopRequested_) {
                            DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 100); // 100ms timeout
                             if (waitResult == WAIT_OBJECT_0) {
                                 operationCompleted = true;
                                 break; // Event signaled, operation finished or cancelled
                            } else if (waitResult == WAIT_TIMEOUT) {
                                 continue;
                            } else { // WAIT_FAILED or other error
                                 error = GetLastError();
                                 break; // Exit wait loop on error
                            }
                        }

                        // If we finished waiting (or were stopped), check final status
                        if (operationCompleted || stopRequested_) {
                             // Call GetOverlappedResult again non-blockingly to get final status & error code
                            if (!GetOverlappedResult(hSerial_, &overlapped, &bytesTransferred, FALSE)) {
                                 error = GetLastError(); // Get the actual error (e.g., ABORTED)
                                 // Error occurred during overlapped operation
                                if (error == ERROR_OPERATION_ABORTED || error == ERROR_INVALID_HANDLE) {
                                     getLogger()->debug("[{}] receiveLoop: WaitCommEvent aborted/cancelled for {}", __PRETTY_FUNCTION__, communicationName_);
                                } else {
                                     getLogger()->error("[{}] Error in final GetOverlappedResult for WaitCommEvent on {}: {}", __PRETTY_FUNCTION__, communicationName_, error);
                                }
                            } else {
                                  // Operation completed successfully after wait
                            }
                        }
                    } else { // GetOverlappedResult failed for reason other than PENDING
                         if (error == ERROR_OPERATION_ABORTED || error == ERROR_INVALID_HANDLE) {
                             getLogger()->debug("[{}] receiveLoop: WaitCommEvent aborted/cancelled (initial check) for {}", __PRETTY_FUNCTION__, communicationName_);
                         } else {
                             getLogger()->error("[{}] Error in initial GetOverlappedResult for WaitCommEvent on {}: {}", __PRETTY_FUNCTION__, communicationName_, error);
                         }
                    }
                }

                // Check stop flag after attempting to get result
                if (stopRequested_) {
                    getLogger()->debug("[{}] receiveLoop: Breaking after WaitCommEvent processing due to stop requested.", __PRETTY_FUNCTION__);
                    break;
                }

                // If the operation failed with an error code other than ABORTED/INVALID_HANDLE,
                // reset the event and continue the outer loop.
                DWORD finalError = GetLastError(); // Check error status *after* potential wait
                 if (!operationCompleted && finalError != ERROR_OPERATION_ABORTED && finalError != ERROR_INVALID_HANDLE && finalError != ERROR_IO_INCOMPLETE) {
                     ResetEvent(overlapped.hEvent);
                     continue;
                 }
                 // Reset event if operation completed or aborted/invalid handle
                 ResetEvent(overlapped.hEvent);

            } else {
                 // Error in WaitCommEvent (not pending)
                 if (error == ERROR_OPERATION_ABORTED || error == ERROR_INVALID_HANDLE) {
                     getLogger()->debug("[{}] receiveLoop: WaitCommEvent aborted/cancelled (sync error check) for {}", __PRETTY_FUNCTION__, communicationName_);
                 } else {
                     getLogger()->error("[{}] Error in WaitCommEvent for {}: {}", __PRETTY_FUNCTION__, communicationName_, error);
                 }
                  // Check stop flag even on error/abort
                  if(stopRequested_) {
                     getLogger()->debug("[{}] receiveLoop: Breaking after WaitCommEvent error due to stop requested.", __PRETTY_FUNCTION__);
                     break;
                  }
                 std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Avoid busy-waiting on error
                 ResetEvent(overlapped.hEvent); // Reset event before continuing
                 continue;
             }
        } else {
              // WaitCommEvent completed synchronously
              // Check stop flag after synchronous completion
              if(stopRequested_) {
                  getLogger()->debug("[{}] receiveLoop: Breaking after synchronous WaitCommEvent due to stop requested.", __PRETTY_FUNCTION__);
                  break;
              }
         }

        // If data has arrived (EV_RXCHAR event bit is set in the result of WaitCommEvent)
        if (dwCommEvent & EV_RXCHAR) {
            do {
                  // Check stop flag before reading
                  if (stopRequested_) {
                     getLogger()->debug("[{}] receiveLoop: Breaking before ReadFile due to stop requested.", __PRETTY_FUNCTION__);
                     break;
                  }

                 // Reset overlapped struct for ReadFile
                 ResetEvent(overlapped.hEvent);
                 dwRead = 0;

                // Read the data
                 if (!ReadFile(hSerial_, buffer, sizeof(buffer), &dwRead, &overlapped)) {
                     DWORD error = GetLastError();
                     if (error == ERROR_IO_PENDING) {
                         // Use non-blocking GetOverlappedResult and manual wait
                         DWORD bytesRead = 0;
                         bool operationCompleted = false;

                         // Check initial status without waiting
                         if (GetOverlappedResult(hSerial_, &overlapped, &bytesRead, FALSE)) {
                             operationCompleted = true;
                             dwRead = bytesRead;
                         } else {
                             error = GetLastError();
                             if (error == ERROR_IO_INCOMPLETE) {
                                 // Operation is pending, wait manually
                                 while (!stopRequested_) {
                                     DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 100); // 100ms timeout
                                     if (waitResult == WAIT_OBJECT_0) {
                                         operationCompleted = true;
                                         break; // Event signaled
                                     } else if (waitResult == WAIT_TIMEOUT) {
                                         continue;
                                     } else { // WAIT_FAILED or other error
                                         error = GetLastError();
                                         break; // Exit wait loop on error
                                     }
                                 }

                                 // If we finished waiting (or were stopped), check final status
                                 if (operationCompleted || stopRequested_) {
                                      // Call GetOverlappedResult again non-blockingly to get final status & error code
                                     if (!GetOverlappedResult(hSerial_, &overlapped, &bytesRead, FALSE)) {
                                         error = GetLastError(); // Get the actual error (e.g., ABORTED)
                                         if (error == ERROR_OPERATION_ABORTED || error == ERROR_INVALID_HANDLE) {
                                             getLogger()->debug("[{}] receiveLoop: ReadFile aborted/cancelled for {}", __PRETTY_FUNCTION__, communicationName_);
                                         } else {
                                             getLogger()->error("[{}] Error in final GetOverlappedResult (ReadFile) for {}: {}", __PRETTY_FUNCTION__, communicationName_, error);
                                         }
                                         dwRead = 0; // Ensure dwRead is 0 on error/abort
                                     } else {
                                         dwRead = bytesRead; // Success, update dwRead
                                     }
                                 }
                             } else { // GetOverlappedResult failed for reason other than PENDING
                                 if (error == ERROR_OPERATION_ABORTED || error == ERROR_INVALID_HANDLE) {
                                    getLogger()->debug("[{}] receiveLoop: ReadFile aborted/cancelled (initial check) for {}", __PRETTY_FUNCTION__, communicationName_);
                                 } else {
                                     getLogger()->error("[{}] Error in initial GetOverlappedResult (ReadFile) for {}: {}", __PRETTY_FUNCTION__, communicationName_, error);
                                 }
                                 dwRead = 0; // Ensure dwRead is 0 on error
                             }
                         }

                          // Check stop flag after ReadFile's overlapped result processing
                          if (stopRequested_) {
                             getLogger()->debug("[{}] receiveLoop: Breaking after GetOverlappedResult(ReadFile) processing due to stop requested.", __PRETTY_FUNCTION__);
                             break;
                          }

                     } else {
                         // Error in ReadFile (not pending)
                          if (error == ERROR_OPERATION_ABORTED || error == ERROR_INVALID_HANDLE) {
                             getLogger()->debug("[{}] receiveLoop: ReadFile aborted/cancelled (sync error check) for {}", __PRETTY_FUNCTION__, communicationName_);
                          } else {
                             getLogger()->error("[{}] Error in ReadFile for {}: {}", __PRETTY_FUNCTION__, communicationName_, error);
                          }
                         dwRead = 0; // Ensure dwRead is 0 on error
                          // Check stop flag after non-pending ReadFile error
                          if (stopRequested_) {
                             getLogger()->debug("[{}] receiveLoop: Breaking after ReadFile error due to stop requested.", __PRETTY_FUNCTION__);
                             break;
                          }
                     }
                 }

                 // Check stop flag after ReadFile attempt completes (sync or async)
                 if (stopRequested_) {
                    getLogger()->debug("[{}] receiveLoop: Breaking after ReadFile attempt due to stop requested.", __PRETTY_FUNCTION__);
                    break;
                 }

                // If data was read, process it
                if (dwRead > 0) {
                    std::lock_guard<std::mutex> lock{bufferMutex_}; // Use uniform initialization
                    receiveBuffer_.insert(receiveBuffer_.end(), buffer, buffer + dwRead);
                    // Notify Logic (or whichever component) that data is available
                    eventQueue_->push(CommEvent{communicationName_, "data_received"});
                }
            } while (dwRead > 0 && !stopRequested_); // Continue reading if more data might be available and not stopped

             // Check stop flag after inner read loop finishes
             if (stopRequested_) {
                 getLogger()->debug("[{}] receiveLoop: Breaking after inner read loop due to stop requested.", __PRETTY_FUNCTION__);
                 break;
             }
         }
         // Reset the event after processing EV_RXCHAR or other events
         ResetEvent(overlapped.hEvent);

          // Check stop flag at end of outer loop iteration
          if (stopRequested_) {
             getLogger()->debug("[{}] receiveLoop: Breaking at end of outer loop due to stop requested.", __PRETTY_FUNCTION__);
             break;
          }
     }
    CloseHandle(overlapped.hEvent);
    getLogger()->debug("[{}] RS232Communication receiveLoop() exited for '{}'", __PRETTY_FUNCTION__, communicationName_);
}
