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
    

    // Controller state enum to replace the simple boolean flag
    enum class ControllerState {
        Running,      // Normal operation
        Stopped,      // Stopped due to error or user request
        OutputOverride // Outputs being controlled by SettingsWindow
    };
    
    const Config& config_;
    EventQueue<EventVariant> &eventQueue_;
    PCI7248IO io_;
    std::atomic<ControllerState> controllerState_; // Current controller state
    ControllerState previousState_; // Previous state before output override
    std::thread blinkThread_;
    std::unordered_map<std::string, IOChannel> outputChannels_;
    Timer t1_, t2_;
    RS232Communication communication1_;
    RS232Communication communication2_;


};

#endif // LOGIC_H
