#pragma once

#include <unordered_map>
#include <string>
#include <atomic>   // For std::atomic<bool>
#include <mutex>    // For std::mutex
#include <windows.h> // For PTP_TIMER, TIMECAPS (ensure minimal include if possible)
#include <threadpoolapiset.h> // More specific include for thread pool APIs

#include "Config.h"      // Provides full definition for Config.
#include "Event.h"       // Provides full definitions for EventVariant and (if defined there) IOEventType.
#include "IOChannel.h"   // Provides full definition for IOChannel and possibly IOEventType if not in Event.h.
#include "EventQueue.h"  // Provides full definition for EventQueue.

// Forward declaration for the event queue template (Good practice)
template <typename T>
class EventQueue;

// Consider forward declaring specific event types if needed and feasible
// struct IOEvent;
// class EventVariant;

// Define DASK types if not universally available or for clarity
// typedef short I16; // Example if I16 is used for card_

class PCI7248IO {
public:
    // --- Constructor & Destructor ---
    PCI7248IO(EventQueue<EventVariant>& eventQueue, const Config& config);
    ~PCI7248IO();

    // --- Public Interface ---

    // Initialize the card, configure ports, and start the polling timer.
    // Returns true on success, false otherwise.
    bool initialize();

    // Signal the polling mechanism to stop processing and prepare for shutdown.
    // Actual resource release happens in the destructor.
    void stopPolling();

    // Write the desired state to configured output channels.
    // The map should contain entries for channels intended to be ON.
    // Channels configured as output but not in the map (or with state 0) will be turned OFF.
    // Returns true on success, false if any hardware write fails.
    // This operation is thread-safe.
    bool writeOutputs(const std::unordered_map<std::string, IOChannel>& newOutputsState);

    // Reset all configured output ports to their OFF state.
    // Returns true on success, false otherwise.
    // This operation is thread-safe.
    bool resetConfiguredOutputPorts();

    // Get a thread-safe snapshot (copy) of the current input channel states.
    std::unordered_map<std::string, IOChannel> getInputChannelsSnapshot() const;

    // Get read-only access to the map defining the configured output channels.
    const std::unordered_map<std::string, IOChannel>& getOutputChannels() const;

    // --- Deleted Functions ---
    // Prevent copying and assignment as this class manages unique hardware resources
    // and background operations (timer).
    PCI7248IO(const PCI7248IO&) = delete;
    PCI7248IO& operator=(const PCI7248IO&) = delete;

private:
    // --- Internal Helper Functions ---
    void assignPortNames(std::unordered_map<std::string, IOChannel>& channels);
    void readInitialInputStates(const std::unordered_map<std::string, int>& portToChannel);
    void logConfiguredChannels();
    // Reads hardware inputs, updates internal state, returns true if any state changed.
    bool updateInputStates(); // Parameter removed - implementation detail
    // Pushes an IOEvent with the current input state snapshot to the queue.
    void pushStateEvent();
    // Gets the DASK library constant for a given port name ("A", "B", etc.).
    int getDaskChannel(const std::string& port) const;
    // Gets the base pin number offset for a given port name.
    int getPortBaseOffset(const std::string& port) const;

    // --- Timer Callback Worker ---
    // This function is executed periodically by the thread pool timer.
    void pollingIteration();

    // --- Member Variables ---

    // Dependencies & Configuration
    EventQueue<EventVariant>& eventQueue_; // Reference to the outgoing event queue
    const Config& config_;                // Reference to the system configuration

    // Hardware & State Representation
    int card_; // DASK card handle (consider using I16 if defined by dask64.h)
    std::unordered_map<std::string, IOChannel> inputChannels_;  // Current state of inputs
    std::unordered_map<std::string, IOChannel> outputChannels_; // Definition of outputs
    std::unordered_map<std::string, std::string> portsConfig_;  // Port name -> "input"/"output"

    // Polling Control & Timer Management
    std::atomic<bool> stopPolling_; // Flag signalling intent to stop polling loops/callbacks
    HANDLE timer_;  // Windows high-resolution timer handle
    bool timerRunning_;  // Flag to control timer thread to the thread pool timer object
    TIMECAPS tc_;                   // Stores timer capabilities for setting resolution

    // Synchronization Primitives
    // `mutable` allows locking in const methods like getInputChannelsSnapshot
    mutable std::mutex outputMutex_; // Protects access to output hardware (writeOutputs, reset)
    mutable std::mutex inputMutex_;  // Protects access to inputChannels_ map during updates/reads

    // Statistics tracking
    std::mutex statsMutex;
    std::chrono::steady_clock::time_point statsStartTime;
    std::chrono::steady_clock::time_point lastCallbackTime;
    long long totalDuration;
    long long minDuration;
    long long maxDuration;
    int iterationCount;
    int delaysOver5ms;
    const std::chrono::seconds statsInterval{10};
};