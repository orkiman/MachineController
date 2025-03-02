#include <iostream>
#include "MainLogic.h"
#include "Config.h"

int main() {
    Config config("config/settings.json");
    EventQueue<EventVariant> eventQueue;
    MainLogic mainLogic(eventQueue, config);
    
    mainLogic.run();

    // Wait for user input to exit gracefully
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();  

    mainLogic.stop();
    std::cout << "Exiting..." << std::endl;
    return 0;
}
