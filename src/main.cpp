#include <iostream>
#include "MainLogic.h"
#include "Config.h"
#include "Logger.h"
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")  // Link with the winmm.lib if needed

int main() {
    timeBeginPeriod(1);  // Ensure 1ms sleep accuracy

    Config config("config/settings.json");
    EventQueue<EventVariant> eventQueue;
    MainLogic mainLogic(eventQueue, config);
    mainLogic.run();

    // Wait for user input to exit gracefully
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();  
    mainLogic.stop();
    std::cout << "Exiting..." << std::endl;
    timeEndPeriod(1);  // Restore normal timer resolution

    return 0;
}
