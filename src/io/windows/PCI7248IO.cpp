#include "io/PCI7248IO.h"
#include "Logger.h"
#include "dask64.h"
#include <chrono>
#include <mutex>
#include <limits>
#include <stdexcept>

PCI7248IO::PCI7248IO(EventQueue<EventVariant>& eventQueue, const Config& config)
    : eventQueue_(eventQueue),
      config_(config),
      card_(-1),
      stopPolling_(false),
      timer_(nullptr),
      statsStartTime(std::chrono::steady_clock::now()),
      lastCallbackTime(),  // Initialize to epoch
      totalDuration(0),
      minDuration(std::numeric_limits<long long>::max()),
      maxDuration(0),
      iterationCount(0),
      delaysOver5ms(0)
{
    inputChannels_ = config_.getInputs();
    outputChannels_ = config_.getOutputs();
}

PCI7248IO::~PCI7248IO() {
    getLogger()->debug("Shutting down PCI7248IO...");
    stopPolling(); // Signal intent to stop (useful if external logic checks flag)

    // Signal threads to stop
    stopPolling_ = true;
    timerRunning_ = false;
    
    // Clean up timer
    if (timer_) {
        CancelWaitableTimer(timer_);
        CloseHandle(timer_);
        timer_ = nullptr;
    }
    
    // Timer resolution is managed in main.cpp

    // Ensure outputs are in a safe state before releasing the card
    resetConfiguredOutputPorts();

    // Release the hardware card
    if (card_ >= 0) {
        Release_Card(card_);
        getLogger()->debug("PCI-7248 Card ({}) released.", card_);
        card_ = -1;
    }

    // Ensure all logs are written
    getLogger()->flush();
}

bool PCI7248IO::initialize() {
    if (!config_.isPci7248ConfigurationValid()) {
        getLogger()->error("Invalid PCI7248 configuration.");
        return false;
    }

    getLogger()->debug("Initializing PCI-7248...");
    card_ = Register_Card(PCI_7248, 0); // Use board number 0
    if (card_ < 0) {
        getLogger()->error("Failed to register PCI-7248! DASK Error Code: {}", card_);
        return false;
    }
    getLogger()->debug("PCI-7248 Card registered successfully (Card ID: {}).", card_);

    portsConfig_ = config_.getPci7248IoPortsConfiguration();

    // Define mapping from configuration string to DASK constant
    static const std::unordered_map<std::string, int> portToChannel = {
        {"A", Channel_P1A}, {"B", Channel_P1B}, {"CL", Channel_P1CL}, {"CH", Channel_P1CH}};

    // Configure each physical port as input or output based on config
    for (const auto& [portName, portTypeStr] : portsConfig_) {
        int daskPortConstant = -1;
        try {
            daskPortConstant = portToChannel.at(portName);
        } catch (const std::out_of_range& oor) {
             getLogger()->error("Invalid port name '{}' found in configuration.", portName);
             Release_Card(card_); card_ = -1;
             return false;
        }

        int portDirection = (portTypeStr == "output") ? OUTPUT_PORT : INPUT_PORT;
        I16 result = DIO_PortConfig(card_, daskPortConstant, portDirection);
        if (result != 0) {
            getLogger()->error("Failed to configure Port {} as {}. DASK Error Code: {}",
                             portName, portTypeStr, result);
            Release_Card(card_); card_ = -1;
            return false;
        }
         getLogger()->debug("Configured Port {} ({}) as {}", portName, daskPortConstant, portTypeStr);
    }

    assignPortNames(inputChannels_);
    assignPortNames(outputChannels_);

    readInitialInputStates(portToChannel);
    logConfiguredChannels();

    // Reset outputs to a known state (off) after configuration
    if (!resetConfiguredOutputPorts()) {
        getLogger()->error("Failed to perform initial reset of configured output ports.");
        Release_Card(card_); card_ = -1;
        return false;
    }

    // --- Start the polling timer ---

    // Set up high-resolution timer capabilities check
    if (timeGetDevCaps(&tc_, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
        getLogger()->error("Could not get timer capabilities.");
        Release_Card(card_); card_ = -1;
        return false;
    }

    getLogger()->debug("Timer capabilities - Min: {}ms, Max: {}ms", tc_.wPeriodMin, tc_.wPeriodMax);
    
    // Timer resolution (1ms) is already set in main.cpp

    // Create high-resolution timer
    timer_ = CreateWaitableTimerEx(
        nullptr,    // No name needed
        nullptr,    // Default security
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,  // Request high-resolution timer
        TIMER_ALL_ACCESS
    );
    
    if (timer_ == nullptr) {
        getLogger()->error("Failed to create high-resolution timer. Error: {}", GetLastError());
        Release_Card(card_); card_ = -1;
        return false;
    }

    // Start polling thread with real-time priority
    timerRunning_ = true;
    std::thread pollingThread([this]() {
        // Set thread to high priority
        HANDLE threadHandle = GetCurrentThread();
        if (!SetThreadPriority(threadHandle, THREAD_PRIORITY_TIME_CRITICAL)) {
            getLogger()->warn("Failed to set polling thread to high priority. Error: {}", GetLastError());
        } else {
            getLogger()->debug("Polling thread set to high priority.");
        }
        // // Set thread affinity to CPU 0
        // DWORD_PTR affinityMask = 1; // CPU 0
        // if (SetThreadAffinityMask(threadHandle, affinityMask) == 0) {
        //     getLogger()->warn("Failed to set timer thread affinity. Error: {}", GetLastError());
        // } else {
        //     getLogger()->debug("Polling Timer thread affinity set to CPU 0.");
        // }

        // --- Timer-based scheduling code (commented out for fast polling mode) ---    
        /*
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -20000LL; // 2ms in 100ns units (negative for relative time)

        if (!SetWaitableTimer(
            timer_,
            &dueTime,
            2,        // 2ms period
            nullptr,  // No completion routine
            nullptr,  // No argument
            FALSE     // No resume from power saving
        )) {
            getLogger()->error("Failed to set timer parameters. Error: {}", GetLastError());
            return;
        }

        while (timerRunning_) {
            if (WaitForSingleObject(timer_, INFINITE) == WAIT_OBJECT_0) {
                if (!stopPolling_) {
                    pollingIteration();
                }
            }
        }
        */

        // --- Fast polling mode with _mm_pause() ---
        #include <immintrin.h> // for _mm_pause
        while (timerRunning_) {
            if (!stopPolling_) {
                pollingIteration();
            }
            // Give CPU a minor relief in tight loop
            for (int i = 0; i < 100000; ++i) {
                _mm_pause();
            } 
        }


    });
    pollingThread.detach(); // Let the thread run independently

    getLogger()->debug("PCI7248IO initialized successfully. High-resolution timer started with 2ms interval.");
    return true;
}

// Assign the ioPort field based on pin number.
void PCI7248IO::assignPortNames(std::unordered_map<std::string, IOChannel>& channels) {
    for (auto& [name, channel] : channels) {
        if (channel.pin < 8) channel.ioPort = "A";
        else if (channel.pin < 16) channel.ioPort = "B";
        else if (channel.pin < 20) channel.ioPort = "CL";
        else if (channel.pin < 24) channel.ioPort = "CH"; // Pins 0-23 typically
        else {
            // Handle invalid pin number if necessary
             getLogger()->warn("Channel '{}' has an invalid pin number: {}", name, channel.pin);
             channel.ioPort = "Invalid"; // Mark as invalid
        }
    }
}

// Read initial input states for all ports configured as input.
void PCI7248IO::readInitialInputStates(const std::unordered_map<std::string, int>& portToChannel) {
    std::lock_guard<std::mutex> lock(inputMutex_); // Protect inputChannels_ during update

    for (const auto& [portName, portTypeStr] : portsConfig_) {
        if (portTypeStr != "input") continue;

        int daskPortConstant = portToChannel.at(portName); // Assumes valid name due to earlier check
        U32 portValue = 0;
        I16 result = DI_ReadPort(card_, daskPortConstant, &portValue);
        if (result != 0) {
            getLogger()->error("Failed to read initial state from Input Port {}. DASK Error Code: {}", portName, result);
            continue; // Skip this port but try others
        }

        // Apply active-low logic if necessary (Assume active-low for now)
        // TODO: Make active-low configurable per channel/port
        portValue = ~portValue;

        int baseOffset = getPortBaseOffset(portName);

        for (auto& [chanName, channel] : inputChannels_) {
            if (channel.ioPort == portName) {
                int pinWithinPort = channel.pin - baseOffset;
                if (pinWithinPort >= 0 && pinWithinPort < 8) { // Check valid bit range within port
                    channel.state = (portValue >> pinWithinPort) & 0x1;
                    channel.eventType = IOEventType::None; // Initial state has no edge
                     getLogger()->trace("Initial state for Input '{}' (Port {}, Pin {}): {}", chanName, portName, channel.pin, channel.state);
                } else {
                     getLogger()->warn("Input Channel '{}' pin {} calculation resulted in invalid bit position {} for port {}", chanName, channel.pin, pinWithinPort, portName);
                }
            }
        }
    }
}

// Log which channels are configured as input vs. output.
void PCI7248IO::logConfiguredChannels() {
    getLogger()->debug("--- Configured Input Channels ---");
    for (const auto& [_, ch] : inputChannels_) {
        getLogger()->debug("Input : {} (Port {}, Pin {})", ch.name, ch.ioPort, ch.pin);
    }
    getLogger()->debug("--- Configured Output Channels ---");
    for (const auto& [_, ch] : outputChannels_) {
        getLogger()->debug("Output: {} (Port {}, Pin {})", ch.name, ch.ioPort, ch.pin);
    }
     getLogger()->debug("---------------------------------");
}
void PCI7248IO::pollingIteration() {
    auto iterStart = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    
    // Read inputs and detect changes.
    bool anyChange = updateInputStates();
    if (anyChange) {
        pushStateEvent();
    }

    {
        std::lock_guard<std::mutex> lock(this->statsMutex);
        if (this->lastCallbackTime != std::chrono::steady_clock::time_point()) {
            const auto interval = std::chrono::duration_cast<std::chrono::microseconds>(now - this->lastCallbackTime).count();
            this->totalDuration += interval;
            this->minDuration = std::min(this->minDuration, interval);
            this->maxDuration = std::max(this->maxDuration, interval);
            
            if (interval > 5000) { // 5ms threshold
                this->delaysOver5ms++;
            }
        }
        this->lastCallbackTime = now;
        this->iterationCount++;

        // Check interval and log statistics
        const auto now = std::chrono::steady_clock::now();
        if (now - this->statsStartTime >= this->statsInterval) {
            if (this->iterationCount > 0) {
                const double avg = static_cast<double>(this->totalDuration) / this->iterationCount;
                double samplesPerSecond = this->iterationCount / 10.0; // 10-second window
                double actualInterval = 10000.0 / this->iterationCount; // actual ms between samples
                getLogger()->trace(
                    "[Poll Stats] Min: {:.3f}ms | Max: {:.3f}ms | Avg: {:.3f}ms | Samples: {} ({:.1f}/s, {:.3f}ms interval) | >5ms: {}",
                    this->minDuration / 1000.0,
                    this->maxDuration / 1000.0,
                    avg / 1000.0,
                    this->iterationCount,
                    samplesPerSecond,
                    actualInterval,
                    this->delaysOver5ms
                );
            }
            
            // Reset counters
            this->totalDuration = 0;
            this->minDuration = std::numeric_limits<long long>::max();
            this->maxDuration = 0;
            this->iterationCount = 0;
            this->delaysOver5ms = 0;
            this->statsStartTime = now;
        }
    }
}

// Reads input ports, checks for state changes, and updates `inputChannels_`.
bool PCI7248IO::updateInputStates() {
    bool anyChange = false;
    std::lock_guard<std::mutex> lock(inputMutex_); // Protect inputChannels_ during update

    for (const auto& [portName, portTypeStr] : portsConfig_) {
        if (portTypeStr != "input") continue;

        int daskPortConstant = getDaskChannel(portName);
        U32 portValue = 0;
        I16 result = DI_ReadPort(card_, daskPortConstant, &portValue);
        if (result != 0) {
            // Log error but continue trying other ports/channels
            getLogger()->error("Failed to read Input Port {} during polling. DASK Error Code: {}", portName, result);
            continue;
        }

        // Apply active-low logic (assumed)
        portValue = ~portValue;
        int baseOffset = getPortBaseOffset(portName);

        for (auto& [chanName, channel] : inputChannels_) {
            if (channel.ioPort != portName) continue;

            int pinWithinPort = channel.pin - baseOffset;
             if (pinWithinPort < 0 || pinWithinPort >= 8) continue; // Skip if pin mapping is wrong

            int newState = (portValue >> pinWithinPort) & 0x1;

            // Compare with previous state and update if changed
            if (channel.state != newState) {
                if (channel.state == 0 && newState == 1) {
                    channel.eventType = IOEventType::Rising;
                } else if (channel.state == 1 && newState == 0) {
                    channel.eventType = IOEventType::Falling;
                }
                 getLogger()->debug("Input state change: {} ({}) from {} to {}", channel.name, channel.eventType == IOEventType::Rising ? "Rising" : "Falling", channel.state, newState);
                channel.state = newState;
                anyChange = true;
            } else {
                // Reset event type if state hasn't changed since last event
                channel.eventType = IOEventType::None;
            }
        }
    }
    return anyChange;
}

// Push an IOEvent containing the updated inputChannels_.
void PCI7248IO::pushStateEvent() {
    IOEvent event;
    // Create a copy under lock to send to the queue
    {
       std::lock_guard<std::mutex> lock(inputMutex_);
       event.channels = inputChannels_; // Make a copy of the current state
    }
    eventQueue_.push(event); // Push the copy
}


// Resets all configured output ports to their off state.
bool PCI7248IO::resetConfiguredOutputPorts() {
    getLogger()->debug("Resetting configured output ports to OFF state.");
    // Passing an empty map to writeOutputs effectively turns all output pins off.
    // Lock is handled within writeOutputs.
    return writeOutputs({});
}

// Write outputs using active-low logic.
// Takes a map representing the desired *ON* state for output channels.
// Channels configured as output but *not* present in the map will be turned OFF.
bool PCI7248IO::writeOutputs(const std::unordered_map<std::string, IOChannel>& desiredOnOutputs) {
    std::lock_guard<std::mutex> lock(outputMutex_); // Ensure thread-safe access to hardware

    // Prepare an aggregate bitmask for each physical port configured as output.
    // Initialize all output port aggregates to 0 (all bits OFF initially).
    std::unordered_map<std::string, U32> portAggregates;
    for (const auto& [portName, portTypeStr] : portsConfig_) {
         if (portTypeStr == "output") {
              portAggregates[portName] = 0; // Initialize output ports only
         }
    }


    // Set bits corresponding to the channels that should be ON.
    for (const auto& [name, channelState] : desiredOnOutputs) {
        // Find the corresponding configured output channel definition
         auto it = outputChannels_.find(name);
         if (it == outputChannels_.end()) {
             getLogger()->warn("Attempted to write to non-configured or non-output channel: {}", name);
             continue; // Skip this entry
         }

         const IOChannel& configuredChannel = it->second;

        // Check if this channel belongs to a port configured as output
        if (portsConfig_.count(configuredChannel.ioPort) && portsConfig_.at(configuredChannel.ioPort) == "output") {
             int baseOffset = getPortBaseOffset(configuredChannel.ioPort);
             int pinWithinPort = configuredChannel.pin - baseOffset;

              if (pinWithinPort >= 0 && pinWithinPort < 8) {
                   // Only set the bit if the desired state is ON (non-zero)
                   if (channelState.state != 0) {
                         portAggregates[configuredChannel.ioPort] |= (1u << pinWithinPort);
                         getLogger()->trace("Setting bit {} for Port {} (Channel {})", pinWithinPort, configuredChannel.ioPort, name);
                   }
              } else {
                 getLogger()->warn("Output Channel '{}' pin {} calculation resulted in invalid bit position {} for port {}", configuredChannel.name, configuredChannel.pin, pinWithinPort, configuredChannel.ioPort);
              }
        } else {
             getLogger()->warn("Attempted to write to channel '{}' on Port {}, which is not configured as output.", name, configuredChannel.ioPort);
        }
    }

    // Write the final aggregated value to each physical output port.
    bool overallSuccess = true;
    for (const auto& [portName, aggregateValue] : portAggregates) {
        // We already know this port is configured as output from the initialization loop above.
        int daskPortConstant = getDaskChannel(portName);

        // Apply active-low logic: invert the bits for the physical write.
        // Mask to 8 bits just in case.
        U32 valueToWrite = (~aggregateValue) & 0xFF;

         getLogger()->trace("Writing value {:#04x} (raw aggregate {:#04x}) to Port {}", valueToWrite, aggregateValue, portName);
        I16 result = DO_WritePort(card_, daskPortConstant, valueToWrite);
        if (result != 0) {
            getLogger()->error("Failed to write to Output Port {}. DASK Error Code: {}", portName, result);
            overallSuccess = false; // Mark failure but continue trying other ports
        }
    }

    return overallSuccess;
}

// Signal intent to stop polling (actual stop happens in destructor).
void PCI7248IO::stopPolling() {
    this->stopPolling_ = true;
    
    // Log final statistics
    std::lock_guard<std::mutex> lock(this->statsMutex);
    if (this->iterationCount > 0) {
        const double avg = static_cast<double>(this->totalDuration) / this->iterationCount;
        getLogger()->trace(
            "[Final Poll Stats] Min: {:.3f}ms | Max: {:.3f}ms | Avg: {:.3f}ms | Samples: {} | >5ms: {}",
            this->minDuration / 1000.0,
            this->maxDuration / 1000.0,
            avg / 1000.0,
            this->iterationCount,
            this->delaysOver5ms
        );
    }
    
    getLogger()->debug("Stop polling signal set.");
    // Note: Timer callbacks might still execute briefly after this flag is set,
    // until the timer is fully closed in the destructor. The check within the
    // callback lambda prevents processing if the flag is set.
}

// Return a snapshot of the current input channels.
std::unordered_map<std::string, IOChannel> PCI7248IO::getInputChannelsSnapshot() const {
     std::lock_guard<std::mutex> lock(inputMutex_);
    // Return a copy to ensure thread safety and prevent modification of internal state.
    return inputChannels_;
}

// Return a reference to the output channels map.
const std::unordered_map<std::string, IOChannel>& PCI7248IO::getOutputChannels() const {
    // Generally safe to return const ref as internal structure doesn't change post-init.
    // The states within might be outdated if not updated via writeOutputs.
    return outputChannels_;
}

// Utility to map port name to the DASK channel constant.
int PCI7248IO::getDaskChannel(const std::string& port) const {
    // Use static map for efficiency
    static const std::unordered_map<std::string, int> channels = {
        {"A", Channel_P1A}, {"B", Channel_P1B}, {"CL", Channel_P1CL}, {"CH", Channel_P1CH}};
    try {
        return channels.at(port);
    } catch (const std::out_of_range& oor) {
        getLogger()->error("Invalid port name '{}' requested for DASK channel lookup.", port);
        // Consider returning a specific error code or throwing an exception
        return -1; // Or some other indicator of error
    }
}

// Return the base pin offset for a given port name.
int PCI7248IO::getPortBaseOffset(const std::string& port) const {
    if (port == "A") return 0;
    if (port == "B") return 8;
    if (port == "CL") return 16;
    if (port == "CH") return 20;

    getLogger()->warn("Requested base offset for unknown port name: {}", port);
    return 0; // Fallback, though this indicates a config/logic error upstream
}