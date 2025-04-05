#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "Logic.h"
#include "Logger.h"
#include "gui/MainWindow.h"
#include <QApplication>
#include <thread>

// Global pointer to Logic instance for emergency shutdown handling.
Logic* g_Logic = nullptr;

// Console control handler (Ctrl+C handling)
BOOL WINAPI ConsoleHandler(DWORD signal) {
    getLogger()->debug("Console signal received: {}", signal);
    switch (signal) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (g_Logic) {
                g_Logic->emergencyShutdown();
            }
            QApplication::quit(); // Gracefully close GUI
            return TRUE;
        default:
            return FALSE;
    }
}

// only for communication test : 
#include "communication/RS232Communication.h"
#include <iostream>
#include <chrono>



int main(int argc, char* argv[]) {
    getLogger()->debug("Application started");
    // Console handler setup (if needed)
    // if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
    //     getLogger()->error("Could not set control handler");
    //     return 1;
    // }

    timeBeginPeriod(1);

    EventQueue<EventVariant> eventQueue;

    // 1. Config Setup
    Config config("config/settings.json");
    
    // 2. GUI Initialization
    QApplication app(argc, argv);
    MainWindow mainWindow(nullptr, eventQueue, config);  // Pass reference to the event queue and config
    mainWindow.show();
    
    // 3. Logic Setup
    Logic logic(eventQueue, config);
    g_Logic = &logic;

    // Connect Logic signals to MainWindow slots
    QObject::connect(&logic, &Logic::guiMessage, &mainWindow, &MainWindow::addMessage);

    // 3. Start Logic in a separate thread
    std::thread logicThread([&logic]() {
        logic.run();
    });

/*
    // communication example : *******************************

    // Replace "COM3" with the appropriate port for your system, and set the desired baud rate.
    //RS232Communication comm(eventQueue, "COM1", 9600); // no stx, default etx
    //RS232Communication comm(eventQueue, "COM1", 9600, "\x02"); // with stx, default etx
    RS232Communication comm(eventQueue, "COM4", 9600, 2, 0x13); // with stx, etx = 0x13

    // Initialize the serial port.
    if (!comm.initialize()) {
        std::cerr << "Failed to initialize communication." << std::endl;
        return -1;
    }

    // // Set a callback that will be invoked when new data is received.
    // comm.setDataReceivedCallback([](const std::string &data) {
    //     std::cout << "Received: " << data << std::endl;
    // });

    // Send a test message.
    if (comm.send("Hello, RS232!")) {
        std::cout << "Message sent successfully." << std::endl;
    } else {
        std::cerr << "Failed to send message." << std::endl;
    }

*/
    
    // 4. Start the GUI event loop (this blocks until the GUI closes)
    int result = app.exec();
    // Close the communication channel gracefully.

    // comm.close();           //************************************  */
    getLogger()->debug("Application closing");

    // 5. Shutdown Sequence
    logic.stop();
    if (logicThread.joinable())
        logicThread.join();
    g_Logic = nullptr;

    timeEndPeriod(1);
    return result;
}

