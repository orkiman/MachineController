#include "communication/RS232Communication.h"
#include <iostream>

RS232Communication::RS232Communication(const std::string &portName, unsigned long baudRate)
    : portName_(portName), baudRate_(baudRate), receiving_(false),
      hSerial_(INVALID_HANDLE_VALUE), shutdownEvent_(NULL) {
}

RS232Communication::~RS232Communication() {
    close();
}

bool RS232Communication::initialize() {
    // Open the serial port.
    hSerial_ = CreateFileA(portName_.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0,  // exclusive access
                           NULL,
                           OPEN_EXISTING,
                           0,
                           NULL);
    if (hSerial_ == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening serial port: " << portName_ << std::endl;
        return false;
    }

    // Configure the serial port.
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial_, &dcbSerialParams)) {
        std::cerr << "Error getting serial state." << std::endl;
        return false;
    }
    dcbSerialParams.BaudRate = baudRate_;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;
    if (!SetCommState(hSerial_, &dcbSerialParams)) {
        std::cerr << "Error setting serial state." << std::endl;
        return false;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(hSerial_, &timeouts)) {
        std::cerr << "Error setting timeouts." << std::endl;
        return false;
    }

    if (!SetCommMask(hSerial_, EV_RXCHAR)) {
        std::cerr << "Error setting comm mask." << std::endl;
        return false;
    }

    // Create a shutdown event (manual reset, initially not signaled).
    shutdownEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (shutdownEvent_ == NULL) {
        std::cerr << "Failed to create shutdown event." << std::endl;
        return false;
    }

    // Start asynchronous reception.
    receiving_ = true;
    receiveThread_ = std::thread(&RS232Communication::receiveLoop, this);
    return true;
}

bool RS232Communication::send(const std::string &message) {
    if (hSerial_ == INVALID_HANDLE_VALUE) return false;
    DWORD bytesWritten;
    if (!WriteFile(hSerial_, message.c_str(), static_cast<DWORD>(message.size()), &bytesWritten, NULL)) {
        std::cerr << "Error writing to serial port." << std::endl;
        return false;
    }
    return bytesWritten == message.size();
}

std::string RS232Communication::receive() {
    char buffer[256];
    std::string result;
    if (hSerial_ == INVALID_HANDLE_VALUE) return "";
    DWORD bytesRead;
    if (ReadFile(hSerial_, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        buffer[bytesRead] = '\0';
        result = buffer;
    }
    return result;
}

void RS232Communication::close() {
    // Signal the shutdown event to unblock the waiting thread.
    if (shutdownEvent_) {
        SetEvent(shutdownEvent_);
    }
    receiving_ = false;
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }

    if (hSerial_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial_);
        hSerial_ = INVALID_HANDLE_VALUE;
    }
    if (shutdownEvent_) {
        CloseHandle(shutdownEvent_);
        shutdownEvent_ = NULL;
    }
}

void RS232Communication::setDataReceivedCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    dataReceivedCallback_ = callback;
}

void RS232Communication::receiveLoop() {
    // Create an overlapped structure for asynchronous event notification.
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        std::cerr << "Failed to create overlapped event." << std::endl;
        return;
    }

    DWORD dwEventMask = 0;
    while (receiving_) {
        // Reset the overlapped event.
        ResetEvent(overlapped.hEvent);

        // Initiate waiting for a communication event (e.g., EV_RXCHAR) in overlapped mode.
        if (!WaitCommEvent(hSerial_, &dwEventMask, &overlapped)) {            
            if (GetLastError() != ERROR_IO_PENDING) {
                std::cerr << "WaitCommEvent failed." << std::endl;
                break;
            }
        }

        // Wait for either the overlapped event (data available) or the shutdown event.
        HANDLE events[2] = { overlapped.hEvent, shutdownEvent_ };
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0 + 1) {
            // Shutdown event was signaled.
            break;
        } else if (waitResult == WAIT_OBJECT_0) {
            // The communication event occurred.
            DWORD bytesTransferred;
            if (!GetOverlappedResult(hSerial_, &overlapped, &bytesTransferred, TRUE)) {
                std::cerr << "GetOverlappedResult failed." << std::endl;
                continue;
            }

            if (dwEventMask & EV_RXCHAR) {
                std::string msg = receive();
                if (!msg.empty()) {
                    std::lock_guard<std::mutex> lock(callbackMutex_);
                    if (dataReceivedCallback_) {
                        dataReceivedCallback_(msg);
                    }
                }
            }
        }
    }

    CloseHandle(overlapped.hEvent);
}
