#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")  // Link with winmm.lib if needed

#include "Logic.h"
#include "Logger.h"
#include <iostream>

// Global pointer to your Logic instance.
Logic* g_Logic = nullptr;

// Console control handler to catch Ctrl+C, closing the terminal, etc.
BOOL WINAPI ConsoleHandler(DWORD signal) {
    switch (signal) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            // Signal the shutdown but avoid blocking operations here.
            if (g_Logic) {
                g_Logic->emergencyShutdown();
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

    // Initialize configuration, event queue, and Logic.
    Config config("config/settings.json");
    EventQueue<EventVariant> eventQueue;
    Logic Logic(eventQueue, config);
    g_Logic = &Logic;  // Set global pointer so the handler can access it.

    Logic.run();

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    // Stop the Logic (which will, in turn, stop polling and perform cleanup).
    Logic.stop();
    g_Logic = nullptr;  // Prevent dangling pointer in the handler.

    std::cout << "Exiting..." << std::endl;
    timeEndPeriod(1);  // Restore normal timer resolution

    return 0;
}
