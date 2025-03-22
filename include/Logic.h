#ifndef LOGIC_H
#define LOGIC_H

#include <thread>
#include <atomic>
#include "io/PCI7248IO.h"
#include "EventQueue.h"
#include "Event.h"
#include "Config.h"
#include <QObject>
#include "Timer.h"


class Logic : public QObject {
    Q_OBJECT
public:
    Logic(EventQueue<EventVariant> &eventQueue, const Config& config);
    ~Logic();

    void run();  // Main loop for event handling
    void stop() ;
    void emergencyShutdown();

signals:
    void updateGui(const QString &msg);
    
private:
    void handleEvent(const IOEvent& event);
    void handleEvent(const CommEvent& event);
    void handleEvent(const GuiEvent& event);
    void handleEvent(const TimerEvent& event);
    void handleEvent(const TerminationEvent& event);
    void blinkLED(std::string channelName);

    Config config_;
    EventQueue<EventVariant> &eventQueue_;
    PCI7248IO io_;
    std::atomic<bool> controllerRunning_;
    std::thread blinkThread_;
    std::unordered_map<std::string, IOChannel> outputChannels_;
    Timer t1_, t2_;


};

#endif // LOGIC_H
