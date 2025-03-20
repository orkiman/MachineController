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

int main(int argc, char* argv[]) {
    getLogger()->debug("Application started");
    // Console handler setup (if needed)
    // if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
    //     getLogger()->error("Could not set control handler");
    //     return 1;
    // }

    timeBeginPeriod(1);

    EventQueue<EventVariant> eventQueue;

    // 1. GUI Initialization
    QApplication app(argc, argv);
    MainWindow mainWindow(nullptr, eventQueue);  // Pass pointer to the event queue
    mainWindow.show();
    
    // 2. Logic Setup
    Config config("config/settings.json");
    Logic logic(eventQueue, config);
    g_Logic = &logic;

    QObject::connect(&logic, &Logic::guiUpdate, &mainWindow, &MainWindow::onGuiUpdate);


    // 3. Start Logic in a separate thread
    std::thread logicThread([&logic]() {
        logic.run();
    });

    // 4. Start the GUI event loop (this blocks until the GUI closes)
    int result = app.exec();
    getLogger()->debug("Application closing");

    // 5. Shutdown Sequence
    logic.stop();
    if (logicThread.joinable())
        logicThread.join();
    g_Logic = nullptr;

    timeEndPeriod(1);
    return result;
}
