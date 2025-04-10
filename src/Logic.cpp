#include "Logic.h"
#include <iostream>
#include "Logger.h"

Logic::Logic(EventQueue<EventVariant> &eventQueue, const Config &config)
    : eventQueue_(eventQueue), config_(config), io_(eventQueue_, config),
      communication1_(eventQueue, "communication1", config),
      communication2_(eventQueue, "communication2", config)
{
    if (!io_.initialize())
    {
        std::cerr << "Failed to initialize PCI7248IO." << std::endl;
        getLogger()->error("Failed to initialize PCI7248IO.");
    }
    else
    {
        // Get initial input states and emit signal to update the SettingsWindow
        // This ensures the IO tab shows the correct states when first opened
        auto initialInputs = io_.getInputChannelsSnapshot();
        emit inputStatesChanged(initialInputs);
        
        std::cout << "Initial input states sent to SettingsWindow." << std::endl;
    }

    // Initialize communication ports
    QString commError = initializeCommunicationPorts();
    if (!commError.isEmpty())
    {
        std::cerr << "Failed to initialize communication ports: " << commError.toStdString() << std::endl;
        getLogger()->error("Failed to initialize communication ports: {}", commError.toStdString());
    }
    else
    {
        std::cout << "Communication ports initialized successfully." << std::endl;
    }
    
    // Initialize timers
    initTimers();
    
}

Logic::~Logic()
{
    // Signal termination to stop the event loop
    eventQueue_.push(TerminationEvent{});
}

void Logic::run()
{
    outputChannels_ = io_.getOutputChannels();

    // Run the event loop indefinitely until a TerminationEvent is received
    while (true)
    {
        EventVariant event;
        eventQueue_.wait_and_pop(event);

        // Check if this is a termination event
        if (std::holds_alternative<TerminationEvent>(event))
        {
            getLogger()->info("Termination event received, exiting event loop");
            break;
        }

        // Process the event
        std::visit([this](auto &&e)
                   { this->handleEvent(e); }, event);
    }
}

void Logic::stop()
{
    static std::once_flag stopFlag;
    std::call_once(stopFlag, [this]()
                   {
                       // Push termination event to stop the event loop
                       eventQueue_.push(TerminationEvent{});
                       // Stop IO polling
                       io_.stopPolling(); 
                   });
}

// Central logic cycle function - called after state changes from any event
void Logic::oneLogicCycle()
{
    if (blinkLed0_ && timers_["timer1"].state_ == 1 && timers_["timer1"].eventType_ == IOEventType::Rising)
    {
        outputChannels_["o0"].state = !outputChannels_["o0"].state; 
        timers_["timer1"].state_ = 0;
        timers_["timer1"].eventType_ = IOEventType::None;
        timers_["timer1"].start(std::chrono::milliseconds(timers_["timer1"].getDuration()), [this](){
            // Push the event to the queue
            eventQueue_.push(TimerEvent{"timer1"});
        });
    }
    else
    {
        outputChannels_["o0"].state = 0;
        timers_["timer1"].state_ = 0;
        timers_["timer1"].eventType_ = IOEventType::None;
        timers_["timer1"].start(std::chrono::milliseconds(timers_["timer1"].getDuration()), [this](){
            // Push the event to the queue
            eventQueue_.push(TimerEvent{"timer1"});
        });
    }
    writeOutputs();
    return;
    // Log the start of a logic cycle
    getLogger()->debug("Starting logic cycle");
    
    // Process input changes if inputs were updated
    if (inputsUpdated_)
    {
        getLogger()->debug("Processing input changes");
        
        // Example of machine-specific logic based on input states
        // Check for specific input conditions
        if (inputChannels_.count("i8") > 0 && inputChannels_.count("i9") > 0)
        {
            if (inputChannels_["i8"].eventType == IOEventType::Rising &&
                inputChannels_["i9"].state == 0)
            {
                std::cout << "start process started" << std::endl;
                // Add machine-specific actions here
            }
        }
        
        
        
        inputsUpdated_ = false; // Reset the flag
    }
    
    // Process communication data if it was updated
    if (commUpdated_)
    {
        getLogger()->debug("Processing communication updates");
        
        // Example: Process messages from different ports
        for (const auto& [port, message] : commData_)
        {
            // Add machine-specific communication handling here
            // Example: Parse commands, update registers, etc.
            getLogger()->debug("Processing message from {}: {}", port, message);
        }
        
        commUpdated_ = false; // Reset the flag
    }
    
    // Process timer events if any timer was updated
    if (timerUpdated_)
    {
        getLogger()->debug("Processing timer updates");
        
        // Add machine-specific timer handling here
        
        timerUpdated_ = false; // Reset the flag
    }
    
    // Apply output changes if needed
    if (outputsUpdated_)
    {
        getLogger()->debug("Applying output changes");
        writeOutputs();
        outputsUpdated_ = false; // Reset the flag
    }
    
    // Log the end of a logic cycle
    getLogger()->debug("Logic cycle completed");
}

// **Event Handlers**
void Logic::handleEvent(const IOEvent &event)
{
    std::cout << "[IO Event] Processing input changes..." << std::endl;
    
    // Log input changes
    for (const auto &pair : event.channels)
    {
        const IOChannel &channel = pair.second;
        std::cout << "  " << channel.name << " -> " << channel.state
                  << " channel.eventType = "
                  << (channel.eventType == IOEventType::Rising ? "Rising" : (channel.eventType == IOEventType::Falling ? "Falling" : (channel.eventType == IOEventType::None ? "None" : "Unknown")))
                  << std::endl;
    }
    
    // Get all current input states and update our internal state
    inputChannels_ = io_.getInputChannelsSnapshot();
    
    // Update event-specific input states (these have event types set)
    for (const auto& [name, channel] : event.channels)
    {
        inputChannels_[name] = channel;
    }
    
    // Emit signal to update the SettingsWindow with current input states
    emit inputStatesChanged(inputChannels_);
    
    // Set flag to indicate inputs were updated
    inputsUpdated_ = true;
    
    // Run the central logic cycle
    oneLogicCycle();
}

void Logic::handleEvent(const CommEvent &event)
{
    std::cout << "[Comm Event] Received from port " 
              << event.port << ": " << event.message << std::endl;
    
    // Store the received message in our communication data map
    commData_[event.port] = event.message;
    
    // Set flag to indicate communication data was updated
    commUpdated_ = true;
    
    // Run the central logic cycle
    oneLogicCycle();
}

void Logic::handleEvent(const GuiEvent &event)
{
    bool runLogicCycle = false; // Flag to determine if we should run the logic cycle
    
    switch (event.type)
    {
    case GuiEventType::ButtonPress:
        std::cout << "[GUI Event] Button pressed: " << event.data << std::endl;
        // For example, if this button press should trigger an output change:
        // setOutput(...);
        runLogicCycle = true; // Button press might affect machine state
        break;
        
    case GuiEventType::SetOutput:
        std::cout << "[GUI Event] Set output " << event.identifier << " to "
                  << (event.intValue == 0 ? "OFF" : "ON") << std::endl;
        outputChannels_[event.identifier].state = event.intValue;
        outputsUpdated_ = true;
        runLogicCycle = true;
        break;
        
    case GuiEventType::SetVariable:
        // std::cout << "[GUI Event] Set variable: " << event.data
        //           << " Value: " << event.intValue << std::endl;
        // setVariable(event.intValue);
        if (event.identifier == "blinkLed0")
        {
            blinkLed0_ = !blinkLed0_;
            if(blinkLed0_)
            {
                timers_["timer1"].start(std::chrono::milliseconds(timers_["timer1"].getDuration()), [this](){   
                    // Push the event to the queue
                    eventQueue_.push(TimerEvent{"timer1"});
                });
            }
            else
            {
                timers_["timer1"].cancel();
            }
        }
        runLogicCycle = true; // Variable change might affect machine state
        break;
        
    case GuiEventType::ParameterChange:
        std::cout << "[GUI Event] Parameter changed: " << event.data
                  << " New value: " << event.intValue << std::endl;
        initializeCommunicationPorts();
        initTimers();

        // adjustParameter(event.intValue);
        runLogicCycle = true; // Parameter change might affect machine state
        break;
        
    case GuiEventType::StatusRequest:
        std::cout << "[GUI Event] Status request received." << std::endl;
        // trigger a status update
        // No need to run logic cycle for status requests
        break;
        
    case GuiEventType::StatusUpdate:
        std::cout << "[GUI Event] Status update: " << event.data << std::endl;
        // No need to run logic cycle for status updates
        break;
        
    case GuiEventType::ErrorMessage:
        std::cerr << "[GUI Event] Error: " << event.data << std::endl;
        // No need to run logic cycle for error messages
        break;
        
    case GuiEventType::GuiMessage:
        // Emit a signal that will be connected to MainWindow to display the message
        emit guiMessage(QString::fromStdString(event.data), QString::fromStdString(event.identifier));
        // No need to run logic cycle for GUI messages
        break;
        
    case GuiEventType::SendCommunicationMessage:
        if (event.identifier == "communication1")
        {
            if (!communication1_.send(event.data))
            {
                getLogger()->error("[GUI Event] SendMessage: Failed to send message to communication1");
            } else {
                getLogger()->info("[GUI Event] SendMessage: Message sent to communication1");
                // Store the sent message in our communication data
                commData_["communication1_sent"] = event.data;
                commUpdated_ = true;
                runLogicCycle = true; // Sending a message might affect machine state
            }
        }
        else if (event.identifier == "communication2")
        {
            if (!communication2_.send(event.data))
            {
                getLogger()->error("[GUI Event] SendMessage: Failed to send message to communication2");
            } else {
                getLogger()->info("[GUI Event] SendMessage: Message sent to communication2");
                // Store the sent message in our communication data
                commData_["communication2_sent"] = event.data;
                commUpdated_ = true;
                runLogicCycle = true; // Sending a message might affect machine state
            }
        }
        else
        {
            getLogger()->error("[GUI Event] SendMessage: Unknown identifier: {}", event.identifier);
        }  
        break;  
        
    default:
        std::cout << "[GUI Event] Unknown event: " << event.data << std::endl;
        break;
    }
    
    // Run the central logic cycle if needed
    if (runLogicCycle)
    {
        oneLogicCycle();
    }
}

void Logic::handleEvent(const TimerEvent &event)
{
    getLogger()->debug("[Timer Event] Timer: " + event.timerName + " triggered.");
    timers_[event.timerName].state_ = 1;
    timers_[event.timerName].eventType_ = IOEventType::Rising;
    // Run the central logic cycle
    oneLogicCycle();
}

void Logic::handleOutputOverrideStateChanged(bool enabled) {
    overrideOutputs_ = enabled;
    
    getLogger()->debug(std::string("Output override ") + (enabled ? "enabled" : "disabled"));
    
    // Emit a message to the GUI
    emit guiMessage(QString("Output override %1").arg(enabled ? "enabled" : "disabled"), 
                   enabled ? "warning" : "info");
    
    // No need to run logic cycle here as this is just a control flag
}

void Logic::handleOutputStateChanged(const std::unordered_map<std::string, IOChannel>& outputs) {
    // Only process output changes if override is enabled
    if (overrideOutputs_) {
        // Update our output channels map
        for (const auto& [name, channel] : outputs) {
            outputChannels_[name] = channel;
        }
        
        // Set flag to indicate outputs were updated
        outputsUpdated_ = true;
        
        // Forward the output states to the IO module
        if (io_.writeOutputs(outputs)) {
            getLogger()->debug("Output states updated successfully");
        } else {
            getLogger()->error("Failed to update output states");
            emit guiMessage("Failed to update output states", "error");
        }
        
        // Run the central logic cycle
        oneLogicCycle();
    } else {
        getLogger()->debug("Ignoring output state change request - override not enabled");
    }
}

void Logic::handleEvent(const TerminationEvent &event)
{
    getLogger()->info("TerminationEvent received; shutting down logic thread.");
    // No need to do anything here as the main loop will check for TerminationEvent directly
}


void Logic::writeOutputs() {
    if (!io_.writeOutputs(outputChannels_)) {
        getLogger()->error("Failed to write output states");
    }
}

void Logic::writeGUIOoutputs() {
    // Direct hardware access logic here - bypasses override check
    io_.writeOutputs(outputChannels_);
}

// void Logic::blinkLED(std::string channelName)
// {
//     std::cout << "Blink thread started." << std::endl;
//     while (controllerRunning_)
//     {
//         outputChannels_[channelName].state = !outputChannels_[channelName].state;
//         writeOutputs();
//         std::this_thread::sleep_for(std::chrono::milliseconds(300));
//     }
// }

void Logic::emergencyShutdown()
{
    io_.resetConfiguredOutputPorts();
}

QString Logic::initializeCommunicationPorts()
{
    try {
        // Close existing communication ports if they're open
        communication1_.close();
        communication2_.close();
        
        // Initialize communication1
        RS232Communication comm1(eventQueue_, "communication1", config_);
        if (!comm1.initialize()) {
            emit guiMessage("Failed to initialize communication port 1. Check port settings and availability.", "error");
            return QString("Failed to initialize communication port 1. Check port settings and availability.");          
        }
        communication1_ = std::move(comm1);
        
        // Initialize communication2
        RS232Communication comm2(eventQueue_, "communication2", config_);
        if (!comm2.initialize()) {
            emit guiMessage("Failed to initialize communication port 2. Check port settings and availability.", "error");
            return QString("Failed to initialize communication port 2. Check port settings and availability.");
        }
        communication2_ = std::move(comm2);
        
        emit guiMessage("Communication ports initialized successfully", "info");
        return QString(); // Empty string indicates success
    } catch (const std::exception& e) {
        return QString("Error initializing communication ports: %1").arg(e.what());
    }
}





void Logic::initTimers() {
    nlohmann::json timerSettings = config_.getTimerSettings();
    
    // Clear existing timers if any
    timers_.clear();
        
    // Iterate through all timer entries
    for (auto it = timerSettings.begin(); it != timerSettings.end(); ++it) {
        const std::string& timerName = it.key();
        const nlohmann::json& timerData = it.value();
        
        // Get the duration from the timer settings
        int duration = timerData["duration"].get<int>();
        
        // Create a new Timer object directly in the map
        auto& timer = timers_[timerName]; // This creates a default-constructed Timer
        
        // Configure the timer
        timer.setName(timerName);
        timer.setDuration(duration);
        timer.setState(0); // Initialize as inactive
        timer.setEventType(IOEventType::None);
        
        std::cout << "Initialized timer: " << timerName << " with duration: " << duration << "ms" << std::endl;
    }
}

#include "moc_Logic.cpp"