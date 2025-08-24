#ifndef LOGIC_H
#define LOGIC_H

#include <thread>
#include <atomic>
#include <unordered_map>
#include <memory>
#include "io/PCI7248IO.h"
#include "EventQueue.h"
#include "Event.h"
#include "Config.h"
#include <QObject>
#include "Timer.h"
#include "communication/RS232Communication.h"
#include "dataFile/DataFile.h"
#include "machine/MachineCore.h"
#include "machine/DefaultMachineCoreFactory.h"

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
    void calibrationResponse(int pulsesPerPage, const std::string& controllerName);
    
public slots:
    // Initialize components that require GUI to be ready
    void initialize();
    
    // Handle output override state changes from SettingsWindow
    void handleOutputOverrideStateChanged(bool enabled);
    
    // Handle output state changes from SettingsWindow
    void handleOutputStateChanged(const std::unordered_map<std::string, IOChannel>& outputs);
    
    // Initialize or reinitialize communication ports
    // Returns true on success, false on failure
    bool initializeCommunicationPorts();
    

    
    // Check if a communication port is active
    bool isCommPortActive(const std::string& portName) const;
    
    // Get a reference to the active communication ports map
    const std::unordered_map<std::string, RS232Communication>& getActiveCommPorts() const;
    
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
    void writeOutputs();
    void writeGUIOoutputs();
    
    // Timer control functions
    void startTimer(const std::string& timerName);
    void stopTimer(const std::string& timerName);
    bool initTimers();
    

    const Config& config_;
    EventQueue<EventVariant> &eventQueue_;
    PCI7248IO io_;
    DataFile dataFile_;

    
    // State tracking
    std::unordered_map<std::string, IOChannel> inputChannels_;  // Current input states
    std::unordered_map<std::string, IOChannel> outputChannels_; // Current output states
    std::unordered_map<std::string, std::vector<std::string>> communicationDataLists_;  // Data lists for each port for processing in oneLogicCycle
    std::unordered_map<std::string, Timer> timers_; // Current timer states
    
    // Map of active communication ports (only includes initialized/active ports)
    std::unordered_map<std::string, RS232Communication> activeCommPorts_;
    
    // Flags for tracking which systems have updates
    bool inputsUpdated_{false};
    bool outputsUpdated_{false};
    bool commUpdated_{false};
    bool timerUpdated_{false};

    // Initialization flags
    bool commsInitialized_{false};
    bool timersInitialized_{false};

    bool blinkLed0_{false};
    
    // Control flags
    bool overrideOutputs_{false}; // Flag to control output overrides

    void closeAllPorts(); // Added declaration

    // Machine logic core (pluggable)
    std::unique_ptr<MachineCore> core_;
};

#endif // LOGIC_H
