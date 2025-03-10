#ifndef LOGIC_H
#define LOGIC_H

#include <thread>
#include <atomic>
#include "io/PCI7248IO.h"
#include "EventQueue.h"
#include "Event.h"
#include "Config.h"

class Logic {
public:
    Logic(EventQueue<EventVariant>& eventQueue, const Config& config);
    ~Logic();

    void run();  // Main loop for event handling
    void stop() ;
    void emergencyShutdown();

private:
    void handleEvent(const IOEvent& event);
    void handleEvent(const CommEvent& event);
    void handleEvent(const GUIEvent& event);
    void handleEvent(const TimerEvent& event);
    void handleEvent(const TerminationEvent& event);
    void blinkLED(std::string channelName);

    Config config_;
    EventQueue<EventVariant>& eventQueue_;
    PCI7248IO io_;
    std::atomic<bool> running_;
    std::thread logicThread_;
    std::thread blinkThread_;
    std::unordered_map<std::string, IOChannel> outputChannels_;


};

#endif // LOGIC_H
