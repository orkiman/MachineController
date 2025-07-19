#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "Logic.h"
#include "Logger.h"
#include "utils/CompilerMacros.h" // Add cross-platform function name macro
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
#include <iostream>
#include <chrono>



int main(int argc, char* argv[]) {
    // Initialize the custom logger - happens automatically on first getLogger() call
    spdlog::set_level(spdlog::level::debug); // Set global log level back to debug
    getLogger()->info("Application starting..."); // First call to getLogger() initializes it

    getLogger()->debug("[{}] Application started",FUNCTION_NAME);
    // Console handler setup (if needed)
    // if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
    //     getLogger()->error("Could not set control handler");
    //     return 1;
    // }

    timeBeginPeriod(1);
    getLogger()->debug("[{}] Multimedia timer resolution set to 1ms", FUNCTION_NAME);

    EventQueue<EventVariant> eventQueue;
    getLogger()->debug("[{}] EventQueue created", FUNCTION_NAME);

    // 1. Config Setup
    Config config("config/settings.json");
    
    // 2. GUI Initialization
    getLogger()->debug("[{}] QApplication instance creating...", FUNCTION_NAME);
    QApplication app(argc, argv);
    getLogger()->debug("[{}] QApplication instance created.", FUNCTION_NAME);

    getLogger()->debug("[{}] MainWindow instance creating...", FUNCTION_NAME);
    MainWindow mainWindow(nullptr, eventQueue, config);  // Pass reference to the event queue and config
    getLogger()->debug("[{}] MainWindow instance created.", FUNCTION_NAME);

    // Connect MainWindow's windowReady signal to SettingsWindow's slot
    QObject::connect(&mainWindow, &MainWindow::windowReady,
                     mainWindow.getSettingsWindow(), &SettingsWindow::onInitialLoadComplete);

    // 3. Logic Setup
    Logic logic(eventQueue, config);
    g_Logic = &logic;

    // Connect Logic signals to MainWindow slots
    QObject::connect(&logic, &Logic::guiMessage, &mainWindow, &MainWindow::addMessage);
    
    // Connect Logic's inputStatesChanged signal to SettingsWindow's updateInputStates slot
    QObject::connect(&logic, SIGNAL(inputStatesChanged(const std::unordered_map<std::string, IOChannel>&)), 
                     mainWindow.getSettingsWindow(), SLOT(updateInputStates(const std::unordered_map<std::string, IOChannel>&)));
    
    // Connect SettingsWindow's output override signals to Logic's slots
    QObject::connect(mainWindow.getSettingsWindow(), SIGNAL(outputOverrideStateChanged(bool)), 
                     &logic, SLOT(handleOutputOverrideStateChanged(bool)));
    QObject::connect(mainWindow.getSettingsWindow(), SIGNAL(outputStateChanged(const std::unordered_map<std::string, IOChannel>&)), 
                     &logic, SLOT(handleOutputStateChanged(const std::unordered_map<std::string, IOChannel>&)));
                     
    // Connect Logic's calibration response signal to SettingsWindow's handler
    QObject::connect(&logic, SIGNAL(calibrationResponse(int, const std::string&)),
                     mainWindow.getSettingsWindow(), SLOT(onGlueEncoderCalibrationResponse(int, const std::string&)));
                     
    // 3. Start Logic in a separate thread
    std::thread logicThread([&logic]() {
        logic.run();
    });

    // 5. Show MainWindow and Start Event Loop
    getLogger()->debug("[{}] Showing MainWindow...", FUNCTION_NAME);
    mainWindow.show();
    getLogger()->debug("[{}] MainWindow show() called.", FUNCTION_NAME);

    getLogger()->debug("[{}] Starting QApplication event loop (app.exec())...", FUNCTION_NAME);
    int result = app.exec();
    getLogger()->debug("[{}] QApplication event loop finished with result: {}", FUNCTION_NAME, result);

    // 6. Cleanup
    getLogger()->debug("Application closing");

    // 5. Shutdown Sequence
    logic.stop();
    if (logicThread.joinable())
        logicThread.join();
    g_Logic = nullptr;

    timeEndPeriod(1);
    return result;
}
