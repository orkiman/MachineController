#include "Logic.h"
#include "Logger.h"
#include "communication/RS232Communication.h"
#include <iostream>

Logic::Logic(EventQueue<EventVariant> &eventQueue, const Config &config)
    : eventQueue_(eventQueue), config_(config), io_(eventQueue_, config) {
  if (!io_.initialize()) {
    std::cerr << "Failed to initialize PCI7248IO." << std::endl;
    getLogger()->error("[{}] Failed to initialize PCI7248IO.", __PRETTY_FUNCTION__);
  } else {
    // Get initial input states and emit signal to update the SettingsWindow
    // This ensures the IO tab shows the correct states when first opened
    auto initialInputs = io_.getInputChannelsSnapshot();
    emit inputStatesChanged(initialInputs);

    std::cout << "Initial input states sent to SettingsWindow." << std::endl;
  }
  getLogger()->debug("[{}] Logic initialized", __PRETTY_FUNCTION__);
  
  // Communication ports and timers will be initialized when the GUI is ready
}

Logic::~Logic() {
  // Signal termination to stop the event loop
  eventQueue_.push(TerminationEvent{});

  // Close all active communication ports
  for (auto &pair : activeCommPorts_) {
    pair.second.close();
  }
  activeCommPorts_.clear();
}

void Logic::initialize() {
  // Initialize timers if not already initialized
  if (!timersInitialized_) {
    if (!initTimers()) {
      getLogger()->error("[{}] Failed to initialize timers", __PRETTY_FUNCTION__);
    } else {
      getLogger()->info("[{}] Timers initialized successfully", __PRETTY_FUNCTION__);
      timersInitialized_ = true;
    }
  } else {
    getLogger()->debug("[{}] Timers already initialized, skipping", __PRETTY_FUNCTION__);
  }

  // Initialize communication ports if not already initialized
  if (!commsInitialized_) {
    if (!initializeCommunicationPorts()) {
      getLogger()->error("[{}] Failed to initialize communication ports", __PRETTY_FUNCTION__);
      emit guiMessage("Failed to initialize communication ports", "error");
    } else {
      getLogger()->info("[{}] Communication ports initialized successfully", __PRETTY_FUNCTION__);
      emit guiMessage("Communication ports initialized successfully", "info");
      commsInitialized_ = true;
    }
  } else {
    getLogger()->debug("[{}] Communication ports already initialized, skipping", __PRETTY_FUNCTION__);
  }
}

void Logic::run() {
  outputChannels_ = io_.getOutputChannels();

  // Run the event loop indefinitely until a TerminationEvent is received
  while (true) {
    EventVariant event;
    eventQueue_.wait_and_pop(event);

    // Check if this is a termination event
    if (std::holds_alternative<TerminationEvent>(event)) {
      getLogger()->info("[{}] Termination event received, exiting event loop", __PRETTY_FUNCTION__);
      break;
    }

    // Process the event
    std::visit([this](auto &&e) { this->handleEvent(e); }, event);
  }
}

void Logic::stop() {
  static std::once_flag stopFlag;
  std::call_once(stopFlag, [this]() {
    // Push termination event to stop the event loop
    eventQueue_.push(TerminationEvent{});
    // Stop IO polling
    io_.stopPolling();
  });
}

// Central logic cycle function - called after state changes from any event
void Logic::oneLogicCycle() {
  // Handle LED blinking separately from timer state
  if (blinkLed0_) {
    // If timer1 triggered, toggle the LED
    if (timers_["timer1"].state_ == 1 &&
        timers_["timer1"].eventType_ == IOEventType::Rising) {
      outputChannels_["o0"].state = !outputChannels_["o0"].state;
      timers_["timer1"].state_ = 0;
      timers_["timer1"].eventType_ = IOEventType::None;
      timers_["timer1"].start(
          std::chrono::milliseconds(timers_["timer1"].getDuration()), [this]() {
            // Push the event to the queue
            eventQueue_.push(TimerEvent{"timer1"});
          });
    }
  } else {
    // If blinking is disabled, turn off the LED
    outputChannels_["o0"].state = 0;
  }

  if(!overrideOutputs_) {
    writeOutputs();
  }
  
  // Process communication data if available
  if (commUpdated_) {
    // First, populate communicationDataLists_ from communicationLatestInputData_
    for (const auto& [commName, message] : communicationNewInputData_) {
      // Only add messages for active communication ports
      if (activeCommPorts_.find(commName) != activeCommPorts_.end() && 
          !message.empty() && 
          commName.find("_sent") == std::string::npos) { // Skip sent messages
        
        // Add the message to this channel's data list
        communicationDataLists_[commName].push_back(message);
      }
    }
    
    // Now process data from all active communication ports
    for (auto& [portName, messageList] : communicationDataLists_) {
      if (!messageList.empty()) {
        getLogger()->debug("[{}] Processing {} messages from {}", __PRETTY_FUNCTION__, messageList.size(), portName);
        
        // Machine-specific logic for each port
        // This is where you would implement custom handling for different ports
        for (const auto& message : messageList) {
          // Example of port-specific processing:
          // if (portName == "COM1") {
          //   // Handle barcode data
          //   processBarcode(message);
          // } else if (portName == "COM2") {
          //   // Handle sensor data
          //   processSensorData(message);
          // } else {
          //   // Default handling for other ports
          //   processGenericMessage(portName, message);
          // }
          
          // For now, just log the message
          getLogger()->debug("[{}] Message from {}: {}", __PRETTY_FUNCTION__, portName, message);
        }
        
        // Clear the list after processing
        messageList.clear();
      }
    }
    
    // Reset the flag after processing all communication data
    commUpdated_ = false;
  }
  
  // reset timers edge
  for (auto& [name, timer] : timers_) {
    timer.setEventType(IOEventType::None);
  }
  
  return;




  // i didnt deal with this bottom yet
  // Log the start of a logic cycle
  getLogger()->debug("[{}] Starting logic cycle", __PRETTY_FUNCTION__);

  // Process input changes if inputs were updated
  if (inputsUpdated_) {
    getLogger()->debug("[{}] Processing input changes", __PRETTY_FUNCTION__);

    // Example of machine-specific logic based on input states
    // Check for specific input conditions
    if (inputChannels_.count("i8") > 0 && inputChannels_.count("i9") > 0) {
      if (inputChannels_["i8"].eventType == IOEventType::Rising &&
          inputChannels_["i9"].state == 0) {
        std::cout << "start process started" << std::endl;
        // Add machine-specific actions here
      }
    }

    inputsUpdated_ = false; // Reset the flag
  }

  // Process communication data if it was updated
  if (commUpdated_) {
    getLogger()->debug("[{}] Processing communication updates", __PRETTY_FUNCTION__);

    // Example: Process messages from different ports
    for (const auto &[port, messageList] : communicationDataLists_) {
      // Add machine-specific communication handling here
      // Example: Parse commands, update registers, etc.
      getLogger()->debug("[{}] Processing {} messages from {}", __PRETTY_FUNCTION__, messageList.size(), port);
      
      // Process each message in the list
      for (const auto& message : messageList) {
        getLogger()->debug("[{}] Message from {}: {}", __PRETTY_FUNCTION__, port, message);
        // Add your machine-specific message handling here
      }
    }

    commUpdated_ = false; // Reset the flag
  }

  // Process timer events if any timer was updated
  if (timerUpdated_) {
    getLogger()->debug("[{}] Processing timer updates", __PRETTY_FUNCTION__);

    // Add machine-specific timer handling here

    timerUpdated_ = false; // Reset the flag
  }

  // Apply output changes if needed
  if (outputsUpdated_) {
    getLogger()->debug("[{}] Applying output changes", __PRETTY_FUNCTION__);
    writeOutputs();
    outputsUpdated_ = false; // Reset the flag
  }

  // Log the end of a logic cycle
  getLogger()->debug("[{}] Logic cycle completed", __PRETTY_FUNCTION__);
}

// **Event Handlers**
void Logic::handleEvent(const IOEvent &event) {
  std::cout << "[IO Event] Processing input changes..." << std::endl;

  // Log input changes
  for (const auto &pair : event.channels) {
    const IOChannel &channel = pair.second;
    std::cout << "  " << channel.name << " -> " << channel.state
              << " channel.eventType = "
              << (channel.eventType == IOEventType::Rising
                      ? "Rising"
                      : (channel.eventType == IOEventType::Falling
                             ? "Falling"
                             : (channel.eventType == IOEventType::None
                                    ? "None"
                                    : "Unknown")))
              << std::endl;
  }

  // Update our internal state with all input channels from the event
  inputChannels_ = event.channels;

  // Emit signal to update the SettingsWindow with current input states
  emit inputStatesChanged(inputChannels_);

  // Set flag to indicate inputs were updated
  inputsUpdated_ = true;

  // Run the central logic cycle
  oneLogicCycle();
}

void Logic::handleEvent(const CommEvent &event) {
  getLogger()->debug("[{}] Received communication from {}: {}", __PRETTY_FUNCTION__, event.communicationName,
             event.message);
  std::cout << "[Communication] Received from " << event.communicationName << ": "
            << event.message << std::endl;

  // Store the received message in our latest input data map
  communicationNewInputData_[event.communicationName] = event.message;
  
  // Set flag to indicate communication data was updated
  commUpdated_ = true;

  // Run the central logic cycle
  oneLogicCycle();
}


void Logic::handleEvent(const GuiEvent &event) {
  bool runLogicCycle = false; // Flag to determine if we should run the logic cycle

  // Log the event for debugging
  getLogger()->debug("[{}] GUI Event] Received: keyword='{}', data='{}', target='{}', intValue={}",
                    __PRETTY_FUNCTION__, event.keyword, event.data, event.target, event.intValue);

  // Handle events based on keyword
  if (event.keyword == "SetOutput") {
    // Set an output channel state
    getLogger()->info("[{}] Setting output {} to {}", __PRETTY_FUNCTION__, event.target, event.intValue);
    outputChannels_[event.target].state = event.intValue;
    outputsUpdated_ = true;
    runLogicCycle = true;
  }
  else if (event.keyword == "SetVariable") {
    // Handle variable setting
    getLogger()->info("[{}] Setting variable {}", __PRETTY_FUNCTION__, event.target);
    
    if (event.target == "blinkLed0") {
      blinkLed0_ = !blinkLed0_;
      getLogger()->info("[{}] LED blinking {}", __PRETTY_FUNCTION__, blinkLed0_ ? "enabled" : "disabled");
      
      if (blinkLed0_) {
        timers_["timer1"].start(
            std::chrono::milliseconds(timers_["timer1"].getDuration()), [this]() {
              // Push the event to the queue
              eventQueue_.push(TimerEvent{"timer1"});
            });
      } else {
        timers_["timer1"].cancel();
      }
    }
    runLogicCycle = true;
  }
  else if (event.keyword == "ParameterChange") {
    // Reinitialize components after parameter changes
    getLogger()->info("[{}] Parameters changed: {}", __PRETTY_FUNCTION__, event.data);
    
    // Check which parameters changed to avoid unnecessary reinitializations
    if (event.data.find("communication") != std::string::npos) {
      getLogger()->info("[{}] Reinitializing communication ports due to parameter changes", __PRETTY_FUNCTION__);
      // Mark as not initialized so it will be reinitialized
      commsInitialized_ = false;
      initializeCommunicationPorts();
      commsInitialized_ = true; // Mark as initialized after reinitialization
    } else {
      getLogger()->debug("[{}] Skipping communication ports reinitialization as parameters don't affect them", __PRETTY_FUNCTION__);
    }
    
    // Only reinitialize timers if timer parameters changed
    if (event.data.find("timer") != std::string::npos) {
      getLogger()->info("[{}] Reinitializing timers due to parameter changes", __PRETTY_FUNCTION__);
      // Mark as not initialized so it will be reinitialized
      timersInitialized_ = false;
      initTimers();
      timersInitialized_ = true; // Mark as initialized after reinitialization
    } else {
      getLogger()->debug("[{}] Skipping timers reinitialization as parameters don't affect them", __PRETTY_FUNCTION__);
    }
    
    runLogicCycle = true;
  }  
  else if (event.keyword == "GuiMessage") {
    // Display a message in the GUI
    emit guiMessage(QString::fromStdString(event.data),
                    QString::fromStdString(event.target));
  }
  else if (event.keyword == "SendCommunicationMessage") {
    // Send a message to a communication port
    auto commPortIt = activeCommPorts_.find(event.target);
    if (commPortIt != activeCommPorts_.end()) {
      if (!commPortIt->second.send(event.data)) {
        getLogger()->error("[{}] Failed to send message to {}", __PRETTY_FUNCTION__, event.target);
      } else {
        getLogger()->info("[{}] Message sent to {}: {}", __PRETTY_FUNCTION__, event.target, event.data);
        // Store the sent message in our communication data
        communicationNewInputData_[event.target + "_sent"] = event.data;
        commUpdated_ = true;
        runLogicCycle = true;
      }
    } else {
      getLogger()->error("[{}] Communication port {} not found or not active", __PRETTY_FUNCTION__, event.target);
      emit guiMessage(QString("Communication port %1 not found or not active")
                          .arg(QString::fromStdString(event.target)),
                      "error");
    }
  }
  else {
    // Handle custom or machine-specific events
    getLogger()->info("[{}] Custom event: keyword='{}', data='{}'", __PRETTY_FUNCTION__, event.keyword, event.data);
    
    // Add your machine-specific event handling here
    // Example:
    // if (event.keyword == "CustomMachineCommand") {
    //   handleCustomMachineCommand(event.data, event.target);
    // }
    
    runLogicCycle = true;
  }

  // Run the central logic cycle if needed
  if (runLogicCycle) {
    oneLogicCycle();
  }
}

void Logic::handleEvent(const TimerEvent &event) {
  getLogger()->debug("[Timer Event] Timer: " + event.timerName + " triggered.");
  timers_[event.timerName].state_ = 1;
  timers_[event.timerName].eventType_ = IOEventType::Rising;
  // Run the central logic cycle
  oneLogicCycle();
}

void Logic::handleOutputOverrideStateChanged(bool enabled) {
  overrideOutputs_ = enabled;

  getLogger()->debug(std::string("Output override ") +
                     (enabled ? "enabled" : "disabled"));

  // Emit a message to the GUI
  emit guiMessage(
      QString("Output override %1").arg(enabled ? "enabled" : "disabled"),
      enabled ? "warning" : "info");

  // No need to run logic cycle here as this is just a control flag
}

void Logic::handleOutputStateChanged(
    const std::unordered_map<std::string, IOChannel> &outputs) {
  // Only process output changes if override is enabled
  if (overrideOutputs_) {
    // Update our output channels map
    for (const auto &[name, channel] : outputs) {
      outputChannels_[name] = channel;
    }

    // Set flag to indicate outputs were updated
    outputsUpdated_ = true;

    // Forward the output states to the IO module
    if (io_.writeOutputs(outputs)) {
      getLogger()->debug("[{}] Output states updated successfully", __PRETTY_FUNCTION__);
    } else {
      getLogger()->error("[{}] Failed to update output states", __PRETTY_FUNCTION__);
      emit guiMessage("Failed to update output states", "error");
    }

    // Run the central logic cycle
    oneLogicCycle();
  } else {
    getLogger()->debug("[{}] Ignoring output state change request - override not enabled", __PRETTY_FUNCTION__);
  }
}

void Logic::handleEvent(const TerminationEvent &event) {
  getLogger()->info("[{}] TerminationEvent received; shutting down logic thread.", __PRETTY_FUNCTION__);
  // No need to do anything here as the main loop will check for
  // TerminationEvent directly
}

void Logic::writeOutputs() {
  if (!io_.writeOutputs(outputChannels_)) {
    getLogger()->error("[{}] Failed to write output states", __PRETTY_FUNCTION__);
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
//         outputChannels_[channelName].state =
//         !outputChannels_[channelName].state; writeOutputs();
//         std::this_thread::sleep_for(std::chrono::milliseconds(300));
//     }
// }

void Logic::emergencyShutdown() { io_.resetConfiguredOutputPorts(); }

bool Logic::initializeCommunicationPorts() {
  try {
    getLogger()->info("[{}] Initializing communication ports...", __PRETTY_FUNCTION__);
    
    // Close existing communication ports if they're open
    for (auto &pair : activeCommPorts_) {
      pair.second.close();
    }
    activeCommPorts_.clear();

    // Get communication settings from config
    nlohmann::json commSettings = config_.getCommunicationSettings();
    if (commSettings.empty()) {
      const std::string errorMsg = "No communication settings found in configuration";
      getLogger()->error("[{}] {}", __PRETTY_FUNCTION__, errorMsg);
      emit guiMessage(QString::fromStdString(errorMsg), "error");
      return false;
    }

    int activePortsCount = 0;
    int successfullyInitialized = 0;
    
    // Iterate through all communication settings and initialize active ports
    for (auto it = commSettings.begin(); it != commSettings.end(); ++it) {
      const std::string &commName = it.key();
      const nlohmann::json &commConfig = it.value();

      // Check if this port is marked as active in the configuration
      bool isActive = commConfig.value("active", false);
      if (!isActive) {
        getLogger()->debug("[{}] Communication port '{}' is not active in config, skipping", __PRETTY_FUNCTION__, commName);
        continue; // Skip inactive ports
      }
      
      activePortsCount++;

      // Create and initialize a new communication object
      RS232Communication comm(eventQueue_, commName, config_);
      if (comm.initialize()) {
        // Add to active communication ports map using move semantics
        activeCommPorts_.emplace(commName, std::move(comm));
        successfullyInitialized++;
        getLogger()->info("[{}] Communication port '{}' initialized successfully", __PRETTY_FUNCTION__, commName);
        emit guiMessage(QString("Communication port %1 initialized successfully")
                        .arg(QString::fromStdString(commName)),
                    "comm_success");
      } else {
        const std::string errorMsg = "Communication port '" + commName + "' initialization failed";
        getLogger()->warn("[{}] {}", __PRETTY_FUNCTION__, errorMsg);
        emit guiMessage(QString::fromStdString(errorMsg), "warning");
        // No need to clean up - object will be destroyed when it goes out of scope
      }
    }

    // Check if any active ports were configured
    if (activePortsCount == 0) {
      const std::string errorMsg = "No active communication ports configured in settings";
      getLogger()->warn("[{}] {}", __PRETTY_FUNCTION__,   errorMsg);
      emit guiMessage(QString::fromStdString(errorMsg), "warning");
      return false;
    }
    
    // Check if any ports were successfully initialized
    if (activeCommPorts_.empty()) {
      const std::string errorMsg = "Failed to initialize any communication ports. Check port settings and availability.";
      getLogger()->error("[{}] {}", __PRETTY_FUNCTION__, errorMsg);
      emit guiMessage(QString::fromStdString(errorMsg), "error");
      return false;
    }

    // Report partial success if some ports failed
    if (successfullyInitialized < activePortsCount) {
      const std::string warnMsg = std::to_string(successfullyInitialized) + " of " + 
                                 std::to_string(activePortsCount) + " communication ports initialized";
      getLogger()->warn("[{}] {}", __PRETTY_FUNCTION__, warnMsg);
      emit guiMessage(QString::fromStdString(warnMsg), "warning");
    } else {
      const std::string successMsg = std::to_string(successfullyInitialized) + " communication port(s) initialized successfully";
      getLogger()->info("[{}] {}", __PRETTY_FUNCTION__, successMsg);
      emit guiMessage(QString::fromStdString(successMsg), "info");
    }
    
    return true;
  } catch (const std::exception &e) {
    const std::string errorMsg = "Error initializing communication ports: " + std::string(e.what());
    getLogger()->error("[{}] {}", __PRETTY_FUNCTION__, errorMsg);
    emit guiMessage(QString::fromStdString(errorMsg), "error");
    return false;
  }
}

bool Logic::isCommPortActive(const std::string &portName) const {
  return activeCommPorts_.find(portName) != activeCommPorts_.end();
}

const std::unordered_map<std::string, RS232Communication> &
Logic::getActiveCommPorts() const {
  return activeCommPorts_;
}

bool Logic::initTimers() {
  try {
    getLogger()->info("[{}] Initializing timers...", __PRETTY_FUNCTION__);
    
    nlohmann::json timerSettings = config_.getTimerSettings();
    if (timerSettings.empty()) {
      const std::string errorMsg = "No timer settings found in configuration";
      getLogger()->error("[{}] {}", __PRETTY_FUNCTION__, errorMsg);
      emit guiMessage(QString::fromStdString(errorMsg), "timer_error");
      return false;
    }

    // Clear existing timers if any
    timers_.clear();
    int initializedCount = 0;

    // Iterate through all timer entries
    for (auto it = timerSettings.begin(); it != timerSettings.end(); ++it) {
      const std::string &timerName = it.key();
      const nlohmann::json &timerData = it.value();

      try {
        // Get the duration from the timer settings
        if (!timerData.contains("duration")) {
          const std::string warnMsg = "Timer '" + timerName + "' has no duration specified, skipping";
          getLogger()->warn("[{}] {}", __PRETTY_FUNCTION__, warnMsg);
          emit guiMessage(QString::fromStdString(warnMsg), "timer_warning");
          continue;
        }
        
        int duration = timerData["duration"].get<int>();
        if (duration <= 0) {
          const std::string warnMsg = "Timer '" + timerName + "' has invalid duration: " + std::to_string(duration) + "ms, skipping";
          getLogger()->warn("[{}] {}", __PRETTY_FUNCTION__, warnMsg);
          emit guiMessage(QString::fromStdString(warnMsg), "timer_warning");
          continue;
        }

        // Create a new Timer object directly in the map
        auto &timer = timers_[timerName]; // This creates a default-constructed Timer

        // Configure the timer
        timer.setName(timerName);
        timer.setDuration(duration);
        timer.setState(0); // Initialize as inactive
        timer.setEventType(IOEventType::None);

        initializedCount++;
        getLogger()->info("[{}] Initialized timer: {} with duration: {}ms", __PRETTY_FUNCTION__, timerName, duration);
        emit guiMessage(QString("Timer %1 initialized with duration: %2ms")
                         .arg(QString::fromStdString(timerName))
                         .arg(duration),
                     "timer_success");
      } catch (const std::exception &e) {
        const std::string errorMsg = "Error initializing timer '" + timerName + "': " + e.what();
        getLogger()->error("[{}] {}", __PRETTY_FUNCTION__, errorMsg);
        emit guiMessage(QString::fromStdString(errorMsg), "timer_error");
      }
    }

    if (initializedCount == 0) {
      const std::string errorMsg = "Failed to initialize any timers";
      getLogger()->error("[{}] {}", __PRETTY_FUNCTION__, errorMsg);
      emit guiMessage(QString::fromStdString(errorMsg), "timer_error");
      return false;
    }

    const std::string successMsg = std::to_string(initializedCount) + " timer(s) initialized successfully";
    getLogger()->info(  "[{}] {}", __PRETTY_FUNCTION__, successMsg);
    emit guiMessage(QString::fromStdString(successMsg), "info");
    return true;
  } catch (const std::exception &e) {
    const std::string errorMsg = "Error initializing timers: " + std::string(e.what());
    getLogger()->error("[{}] {}", __PRETTY_FUNCTION__, errorMsg);
    emit guiMessage(QString::fromStdString(errorMsg), "timer_error");
    return false;
  }
}

#include "moc_Logic.cpp"