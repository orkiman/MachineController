#ifndef MAIN_LOGIC_H
#define MAIN_LOGIC_H

#include <thread>
#include <atomic>
#include "io/PCI7248IO.h"
#include "EventQueue.h"
#include "Event.h"
#include "Config.h"

class MainLogic {
public:
    MainLogic(EventQueue<EventVariant>& eventQueue, const Config& config);
    ~MainLogic();

    void run();  // Main loop for event handling
    void stop() ;

private:
    void handleEvent(const IOEvent& event);
    void handleEvent(const CommEvent& event);
    void handleEvent(const GUIEvent& event);
    void handleEvent(const TimerEvent& event);

    Config config_;
    EventQueue<EventVariant>& eventQueue_;
    PCI7248IO io_;
    std::atomic<bool> running_;
    std::thread logicThread_;
};

#endif // MAIN_LOGIC_H
