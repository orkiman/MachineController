#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")  // Link with winmm.lib if needed

#include "MainLogic.h"
#include "Logger.h"
#include <iostream>

// Global pointer to your MainLogic instance.
MainLogic* g_mainLogic = nullptr;

// Console control handler to catch Ctrl+C, closing the terminal, etc.
BOOL WINAPI ConsoleHandler(DWORD signal) {
    switch (signal) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            // Signal the shutdown but avoid blocking operations here.
            if (g_mainLogic) {
                g_mainLogic->emergencyShutdown();
            }
            return TRUE;
        default:
            return FALSE;
    }
}

int main() {
    // Set up the console control handler.
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        getLogger()->error("Error: Could not set control handler");
        return 1;
    }

    timeBeginPeriod(1);  // Ensure 1ms sleep accuracy

    // Initialize configuration, event queue, and MainLogic.
    Config config("config/settings.json");
    EventQueue<EventVariant> eventQueue;
    MainLogic mainLogic(eventQueue, config);
    g_mainLogic = &mainLogic;  // Set global pointer so the handler can access it.

    mainLogic.run();

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    // Stop the MainLogic (which will, in turn, stop polling and perform cleanup).
    mainLogic.stop();
    g_mainLogic = nullptr;  // Prevent dangling pointer in the handler.

    std::cout << "Exiting..." << std::endl;
    timeEndPeriod(1);  // Restore normal timer resolution

    return 0;
}
