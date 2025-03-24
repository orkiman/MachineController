// RS232Communication.cpp
#include "communication/RS232Communication.h"
#include <iostream>

RS232Communication::RS232Communication(EventQueue<EventVariant>& eventQueue, const std::string &portName, unsigned long baudRate, char stx, char etx)
    : eventQueue_(eventQueue),
      portName_(portName),
      baudRate_(baudRate),
      STX(stx),
      ETX(etx),
      receiving_(false),
      hSerial_(INVALID_HANDLE_VALUE)
{
    // Additional initialization code if needed.
}

RS232Communication::~RS232Communication()
{
    close();
}

bool RS232Communication::initialize()
{
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
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (!SetCommState(hSerial_, &dcbSerialParams))
    {
        std::cerr << "Error setting serial state." << std::endl;
        return false;
    }

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
        if (STX != 0) {
            stxPos = receiveBuffer_.find(STX);
        }
        size_t etxPos = receiveBuffer_.find(ETX);

        if (STX == 0)
        {
            if (etxPos != std::string::npos)
            {
                // No STX, but ETX found. Return everything up to ETX.
                completeMessage = receiveBuffer_.substr(0, etxPos);
                receiveBuffer_.erase(0, etxPos + 1); // Remove processed data.
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
                    receiveBuffer_.erase(0, etxPos + 1); // Remove processed data.
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
        // The communication event occurred.
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
