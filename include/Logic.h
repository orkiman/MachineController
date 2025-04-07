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
#include "communication/RS232Communication.h"


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
    void guiMessage(const QString &msg, const QString &identifier);
    void inputStatesChanged(const std::unordered_map<std::string, IOChannel>& inputs);
    
public slots:
    // Handle output override state changes from SettingsWindow
    void handleOutputOverrideStateChanged(bool enabled);
    
    // Handle output state changes from SettingsWindow
    void handleOutputStateChanged(const std::unordered_map<std::string, IOChannel>& outputs);
    
private:
    void handleEvent(const IOEvent& event);
    void handleEvent(const CommEvent& event);
    void handleEvent(const GuiEvent& event);
    void handleEvent(const TimerEvent& event);
    void handleEvent(const TerminationEvent& event);
    void blinkLED(std::string channelName);
    void writeOutputs();
    void writeGUIOoutputs();
    

    const Config& config_;
    EventQueue<EventVariant> &eventQueue_;
    PCI7248IO io_;
    std::thread blinkThread_;
    std::unordered_map<std::string, IOChannel> outputChannels_;
    Timer t1_, t2_;
    RS232Communication communication1_;
    RS232Communication communication2_;
    std::atomic<bool> controllerRunning_{true}; // Flag to control main loop and threads
    bool overrideOutputs_{false}; // Flag to control output overrides


};

#endif // LOGIC_H
