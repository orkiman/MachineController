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

int main(int argc, char* argv[]) {
    // Console handler setup
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        getLogger()->error("Could not set control handler");
        return 1;
    }

    timeBeginPeriod(1);

    // Qt Application setup
    QApplication app(argc, argv);
    MainWindow mainWindow;
    mainWindow.show();

    // Logic initialization clearly happens here
    Config config("config/settings.json");
    EventQueue<EventVariant> eventQueue;
    Logic logic(eventQueue, config);
    g_Logic = &logic;

    // Start Logic in a separate thread clearly
    std::thread logicThread([&logic]() {
        logic.run();
    });

    // GUI event loop (main thread)
    int result = app.exec();

    // GUI closed, initiate Logic shutdown
    logic.stop();

    if (logicThread.joinable())
        logicThread.join();

    g_Logic = nullptr;
    timeEndPeriod(1);

    return result;
}
