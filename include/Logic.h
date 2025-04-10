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
    
    // Initialize or reinitialize communication ports
    // Returns empty string on success, error message on failure
    QString initializeCommunicationPorts();
    
private:
    // Central logic cycle function - called after state changes from any event
    void oneLogicCycle();
    
    // Event handlers - update state and trigger oneLogicCycle
    void handleEvent(const IOEvent& event);
    void handleEvent(const CommEvent& event);
    void handleEvent(const GuiEvent& event);
    void handleEvent(const TimerEvent& event);
    void handleEvent(const TerminationEvent& event);
    
    // Helper functions
    void blinkLED(std::string channelName);
    void writeOutputs();
    void writeGUIOoutputs();
    

    const Config& config_;
    EventQueue<EventVariant> &eventQueue_;
    PCI7248IO io_;
    std::thread blinkThread_;
    
    // State tracking
    std::unordered_map<std::string, IOChannel> inputChannels_;  // Current input states
    std::unordered_map<std::string, IOChannel> outputChannels_; // Current output states
    std::unordered_map<std::string, std::string> commData_;     // Communication data by port
    
    // Flags for tracking which systems have updates
    bool inputsUpdated_{false};
    bool outputsUpdated_{false};
    bool commUpdated_{false};
    bool timerUpdated_{false};
    
    // Timers
    Timer t1_, t2_;
    
    // Communication ports
    RS232Communication communication1_;
    RS232Communication communication2_;
    
    // Control flags
    std::atomic<bool> controllerRunning_{true}; // Flag to control main loop and threads
    bool overrideOutputs_{false}; // Flag to control output overrides


};

#endif // LOGIC_H
