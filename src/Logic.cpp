#include "Logic.h"
#include "Logger.h"
#include "communication/RS232Communication.h"
#include "utils/CompilerMacros.h" // Add cross-platform function name macro
#include "json.hpp"
#include <iostream>
#include <tuple>

Logic::Logic(EventQueue<EventVariant> &eventQueue, const Config &config)
    : eventQueue_(eventQueue), config_(config), io_(eventQueue_, config) {
  if (!io_.initialize()) {
    std::cerr << "Failed to initialize PCI7248IO." << std::endl;
    getLogger()->error("[{}] Failed to initialize PCI7248IO.", FUNCTION_NAME);
  } else {
    // Get initial input states and emit signal to update the SettingsWindow
    // This ensures the IO tab shows the correct states when first opened
    auto initialInputs = io_.getInputChannelsSnapshot();
    emit inputStatesChanged(initialInputs);

    std::cout << "Initial input states sent to SettingsWindow." << std::endl;
  }
  getLogger()->debug("[{}] Logic initialized", FUNCTION_NAME);
  
  // Communication ports and timers will be initialized when the GUI is ready

  // Create machine core implementation
  core_.reset(createDefaultMachineCore());
  // Configure barcode store capacity from config
  if (core_) {
    int cells = config_.getNumberOfMachineCells();
    if (cells < 0) cells = 0;
    core_->setStoreCapacity(static_cast<std::size_t>(cells));
  }
}

Logic::~Logic() {
    // The map's destructor will handle calling RS232Communication destructors.
    // RS232Communication destructor calls close(), which has checks for multiple calls.
     getLogger()->debug("Logic destructor finished."); // Add log to confirm destructor completes
}

void Logic::initialize() {
  // Initialize timers if not already initialized
  if (!timersInitialized_) {
    if (!initTimers()) {
      getLogger()->error("[{}] Failed to initialize timers", FUNCTION_NAME);
    } else {
      getLogger()->debug("[{}] Timers initialized successfully", FUNCTION_NAME);
      timersInitialized_ = true;
    }
  } else {
    getLogger()->debug("[{}] Timers already initialized, skipping", FUNCTION_NAME);
  }

  // Initialize communication ports if not already initialized
  if (!commsInitialized_) {
    if (!initializeCommunicationPorts()) {
      getLogger()->error("[{}] Failed to initialize communication ports", FUNCTION_NAME);
      emit guiMessage("Failed to initialize communication ports", "error");
    } else {
      getLogger()->debug("[{}] Communication ports initialized successfully", FUNCTION_NAME);
      emit guiMessage("Communication ports initialized successfully", "info");
      commsInitialized_ = true;
    }
  } else {
    getLogger()->debug("[{}] Communication ports already initialized, skipping", FUNCTION_NAME);
  }
}

void Logic::run() {
  // Initialize timers and communication ports within the Logic thread
  this->initialize();
  
  outputChannels_ = io_.getOutputChannels();

  // Run the event loop indefinitely until a TerminationEvent is received
  while (true) {
    EventVariant event;
    eventQueue_.wait_and_pop(event);

    // Check if this is a termination event
    if (std::holds_alternative<TerminationEvent>(event)) {
      getLogger()->debug("[{}] Termination event received", FUNCTION_NAME);
      closeAllPorts(); // Close ports BEFORE breaking
      getLogger()->debug("[{}] Exiting event loop", FUNCTION_NAME);
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
  getLogger()->debug("[{}] Received communication from {}: {}", FUNCTION_NAME, event.communicationName,
             event.message);
  std::cout << "[Communication] Received from " << event.communicationName << ": "
            << event.message << std::endl;

  // Compute offset for this port from configuration
  int offset = 0;
  auto commSettings = config_.getCommunicationSettings();
  if (commSettings.contains(event.communicationName) && commSettings[event.communicationName].contains("offset")) {
    offset = commSettings[event.communicationName]["offset"].get<int>();
  }

  // Prepare a single pending communication message for this cycle
  CommCellMessage cm;
  cm.port = event.communicationName;
  cm.offset = offset;
  cm.raw = event.message;
  try {
    cm.parsed = nlohmann::json::parse(event.message);
  } catch (...) {
    // ignore parse errors; keep raw only
  }
  pendingCommMsg_ = std::move(cm);

  // Run the central logic cycle
  oneLogicCycle();
}

void Logic::handleEvent(const GuiEvent &event) {
  // Log the event for debugging
  getLogger()->debug("[{}] GUI Event] Received: keyword='{}', data='{}', target='{}', intValue={}",
                    FUNCTION_NAME, event.keyword, event.data, event.target, event.intValue);

  bool runLogicCycle = false;

  // --- Handle events by keyword ---
  if (event.keyword == "SetOutput") {
    // Set an output channel state
    getLogger()->debug("[{}] Setting output {} to {}", FUNCTION_NAME, event.target, event.intValue);
    outputChannels_[event.target].state = event.intValue;
    outputsUpdated_ = true;
    runLogicCycle = true;
  }
  else if (event.keyword == "SetVariable") {
    // Handle variable setting
    getLogger()->debug("[{}] Setting variable {}", FUNCTION_NAME, event.target);
    
    if (event.target == "blinkLed0") {
      blinkLed0_ = !blinkLed0_;
      getLogger()->debug("[{}] LED blinking {}", FUNCTION_NAME, blinkLed0_ ? "enabled" : "disabled");
      if (core_) { core_->setBlinkLed(blinkLed0_); }
      
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
    getLogger()->debug("[{}] Parameters changed: {}", FUNCTION_NAME, event.data);
    
    // Check which parameters changed to avoid unnecessary reinitializations
    if (event.target.find("communication") != std::string::npos) {
      getLogger()->debug("[{}] Reinitializing communication ports due to parameter changes", FUNCTION_NAME);
      if (!initializeCommunicationPorts()) {
        getLogger()->error("[{}] Failed to reinitialize communication ports after parameter change", FUNCTION_NAME);
        emit guiMessage("Failed to reinitialize communication ports after parameter change", "error");
      }
    } else {
      getLogger()->debug("[{}] Skipping communication ports reinitialization as parameters don't affect them", FUNCTION_NAME);
    }
    
    // Only reinitialize timers if timer parameters changed
    if (event.target.find("timer") != std::string::npos) {
      getLogger()->debug("[{}] Reinitializing timers due to parameter changes", FUNCTION_NAME);
      // Mark as not initialized so it will be reinitialized
      timersInitialized_ = false;
      initTimers();
      timersInitialized_ = true; // Mark as initialized after reinitialization
    } else {
      getLogger()->debug("[{}] Skipping timers reinitialization as parameters don't affect them", FUNCTION_NAME);
    }
    
    if (event.target == "datafile") {
      // Reinitialize data file
      dataFile_.loadFromFile(event.data, config_);
      
      
    }
    
    runLogicCycle = true;
  } else if (event.keyword == "GuiMessage") {
    // Display a message in the GUI
    emit guiMessage(QString::fromStdString(event.data),
                    QString::fromStdString(event.target));
  } else if (event.keyword == "SendCommunicationMessage") {
    // Send a message to a communication port
    auto commPortIt = activeCommPorts_.find(event.target);
    if (commPortIt != activeCommPorts_.end()) {
      if (!commPortIt->second.send(event.data)) {
        getLogger()->error("[{}] Failed to send message to {}", FUNCTION_NAME, event.target);
      } else {
        getLogger()->debug("[{}] Message sent to {}: {}", FUNCTION_NAME, event.target, event.data);
        // Store the sent message in our communication data
        // communicationNewInputData_[event.target + "_sent"] = event.data;
        // commUpdated_ = true;
        // runLogicCycle = true;
      }
    } else {
      getLogger()->error("[{}] Communication port {} not found or not active", FUNCTION_NAME, event.target);
      emit guiMessage(QString("Communication port %1 not found or not active")
                          .arg(QString::fromStdString(event.target)),
                      "error");
    }
  } else {
    // Handle custom or machine-specific events
    getLogger()->debug("[{}] Custom event: keyword='{}', data='{}'", FUNCTION_NAME, event.keyword, event.data);
    
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
      getLogger()->debug("[{}] Output states updated successfully", FUNCTION_NAME);
    } else {
      getLogger()->error("[{}] Failed to update output states", FUNCTION_NAME);
      emit guiMessage("Failed to update output states", "error");
    }

    // Run the central logic cycle
    oneLogicCycle();
  } else {
    getLogger()->debug("[{}] Ignoring output state change request - override not enabled", FUNCTION_NAME);
  }
}

void Logic::handleEvent(const TerminationEvent &event) {
  getLogger()->debug("[{}] TerminationEvent received; shutting down logic thread.", FUNCTION_NAME);
  // No need to do anything here as the main loop will check for
  // TerminationEvent directly
}

void Logic::writeOutputs() {
  if (!io_.writeOutputs(outputChannels_)) {
    getLogger()->error("[{}] Failed to write output states", FUNCTION_NAME);
  }
}

void Logic::writeGUIOoutputs() {
  // Direct hardware access logic here - bypasses override check
  io_.writeOutputs(outputChannels_);
}


void Logic::emergencyShutdown() { io_.resetConfiguredOutputPorts(); }

bool Logic::initializeCommunicationPorts() {
  try {
    getLogger()->debug("[{}] Initializing communication ports...", FUNCTION_NAME);
    
    // Close existing communication ports if they're open
    for (auto &pair : activeCommPorts_) {
      pair.second.close();
    }
    activeCommPorts_.clear();

    // Get communication settings from config
    nlohmann::json commSettings = config_.getCommunicationSettings();
    if (commSettings.empty()) {
      const std::string errorMsg = "No communication settings found in configuration";
      getLogger()->error("[{}] {}", FUNCTION_NAME, errorMsg);
      emit guiMessage(QString::fromStdString(errorMsg), "error");
      return false;
    }

    int activePortsCount = 0;
    int successfullyInitialized = 0;
    
    // Iterate through all communication settings and initialize active ports
    for (auto it = commSettings.begin(); it != commSettings.end(); ++it) {
      const std::string &commName = it.key();
      const nlohmann::json &commConfig = it.value();

      // *** ADDED CHECK FOR EMPTY PORT NAME ***
      if (commName.empty()) {
        getLogger()->warn("[{}] Found communication setting with empty name, skipping.", FUNCTION_NAME);
        continue;
      }

      // Check if this port is marked as active in the configuration
      bool isActive = commConfig.value("active", false);
      if (!isActive) {
        getLogger()->debug("[{}] Communication port '{}' is not active in config, skipping", FUNCTION_NAME, commName);
        continue; // Skip inactive ports
      }
      
      activePortsCount++;

      // Try to emplace (construct in-place) the communication object
      auto emplaceResult = activeCommPorts_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(commName), // Key constructor arguments
          std::forward_as_tuple(eventQueue_, commName, config_) // Value (RS232Communication) constructor arguments
      );

      // Check if emplacement was successful (should always be true here since we clear the map)
      if (emplaceResult.second) {
          RS232Communication& newComm = emplaceResult.first->second; // Get reference to the emplaced object

          // Now initialize the object *after* it's securely in the map
          if (newComm.initialize()) {
              // Initialization successful
              successfullyInitialized++;
              getLogger()->debug("[{}] Communication port '{}' initialized successfully", FUNCTION_NAME, commName);
              emit guiMessage(QString("Communication port %1 initialized successfully")
                              .arg(QString::fromStdString(commName)),
                          "comm_success");
          } else {
              // Initialization failed for the emplaced object, remove it from the map
              const std::string errorMsg = "Communication port '" + commName + "' initialization failed";
              getLogger()->warn("[{}] {}", FUNCTION_NAME, errorMsg);
              emit guiMessage(QString::fromStdString(errorMsg), "warning");
              activeCommPorts_.erase(emplaceResult.first); // Erase the element that failed initialization
          }
      } else {
          // This case indicates the key already existed, which shouldn't happen after clearing the map.
           getLogger()->error("[{}] Failed to emplace communication port '{}' due to existing key (unexpected!)", FUNCTION_NAME, commName);
      }
    }

    // Check if any active ports were configured
    if (activePortsCount == 0) {
      const std::string errorMsg = "No active communication ports configured in settings";
      getLogger()->warn("[{}] {}", FUNCTION_NAME,   errorMsg);
      emit guiMessage(QString::fromStdString(errorMsg), "warning");
      // Return true if no active ports is not considered a failure
      return true; // Or false depending on desired behavior
    }
    
    // Check if any ports were successfully initialized
    if (activeCommPorts_.empty() && activePortsCount > 0) { // Added check for activePortsCount > 0
      const std::string errorMsg = "Failed to initialize any active communication ports. Check port settings and availability.";
      getLogger()->error("[{}] {}", FUNCTION_NAME, errorMsg);
      emit guiMessage(QString::fromStdString(errorMsg), "error");
      return false;
    }

    // Report partial success if some ports failed
    if (successfullyInitialized < activePortsCount) {
      const std::string warnMsg = std::to_string(successfullyInitialized) + " of " + 
                                 std::to_string(activePortsCount) + " active communication ports initialized";
      getLogger()->warn("[{}] {}", FUNCTION_NAME, warnMsg);
      emit guiMessage(QString::fromStdString(warnMsg), "warning");
    } else {
      const std::string successMsg = std::to_string(successfullyInitialized) + " communication port(s) initialized successfully";
      getLogger()->debug("[{}] {}", FUNCTION_NAME, successMsg);
      emit guiMessage(QString::fromStdString(successMsg), "info");
    }
    
    return !activeCommPorts_.empty(); // Return true if at least one port initialized successfully
  } catch (const std::exception &e) {
    const std::string errorMsg = "Error initializing communication ports: " + std::string(e.what());
    getLogger()->error("[{}] {}", FUNCTION_NAME, errorMsg);

    return false;
  }
}

void Logic::closeAllPorts() {
    getLogger()->debug("[{}] Closing all active communication ports...", FUNCTION_NAME);
    for (auto &pair : activeCommPorts_) {
        getLogger()->debug("Closing port '{}' from Logic::closeAllPorts", pair.first);
        getLogger()->flush(); // Explicitly flush logs before closing port
        pair.second.close();
    }
    getLogger()->debug("[{}] Finished closing communication ports.", FUNCTION_NAME);
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
    getLogger()->debug("[{}] Initializing timers...", FUNCTION_NAME);
    
    nlohmann::json timerSettings = config_.getTimerSettings();
    if (timerSettings.empty()) {
      const std::string errorMsg = "No timer settings found in configuration";
      getLogger()->error("[{}] {}", FUNCTION_NAME, errorMsg);
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
          getLogger()->warn("[{}] {}", FUNCTION_NAME, warnMsg);
          emit guiMessage(QString::fromStdString(warnMsg), "timer_warning");
          continue;
        }
        
        int duration = timerData["duration"].get<int>();
        if (duration <= 0) {
          const std::string warnMsg = "Timer '" + timerName + "' has invalid duration: " + std::to_string(duration) + "ms, skipping";
          getLogger()->warn("[{}] {}", FUNCTION_NAME, warnMsg);
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
        getLogger()->debug("[{}] Initialized timer: {} with duration: {}ms", FUNCTION_NAME, timerName, duration);
        emit guiMessage(QString("Timer %1 initialized with duration: %2ms")
                         .arg(QString::fromStdString(timerName))
                         .arg(duration),
                     "timer_success");
      } catch (const std::exception &e) {
        const std::string errorMsg = "Error initializing timer '" + timerName + "': " + e.what();
        getLogger()->error("[{}] {}", FUNCTION_NAME, errorMsg);
        emit guiMessage(QString::fromStdString(errorMsg), "timer_error");
      }
    }

    if (initializedCount == 0) {
      const std::string errorMsg = "Failed to initialize any timers";
      getLogger()->error("[{}] {}", FUNCTION_NAME, errorMsg);
      emit guiMessage(QString::fromStdString(errorMsg), "timer_error");
      return false;
    }

    const std::string successMsg = std::to_string(initializedCount) + " timer(s) initialized successfully";
    getLogger()->debug(  "[{}] {}", FUNCTION_NAME, successMsg);
    emit guiMessage(QString::fromStdString(successMsg), "info");
    return true;
  } catch (const std::exception &e) {
    const std::string errorMsg = "Error initializing timers: " + std::string(e.what());
    getLogger()->error("[{}] {}", FUNCTION_NAME, errorMsg);
    emit guiMessage(QString::fromStdString(errorMsg), "timer_error");
    return false;
  }
}

void Logic::oneLogicCycle() {
  // Build CycleInputs for the core
  CycleInputs in{inputChannels_};
  in.blinkLed0 = blinkLed0_;

  // Collect timer edges for this cycle (Option A)
  for (auto& [name, t] : timers_) {
    TimerEdge edge{};
    if (t.eventType_ == IOEventType::Rising) edge.rising = true;
    if (t.eventType_ == IOEventType::Falling) edge.falling = true;
    if (edge.rising || edge.falling) {
      in.timerEdges[name] = edge;
    }
  }

  // Provide snapshots needed by the core
  in.outputsSnapshot = outputChannels_;
  in.timersSnapshot.clear();
  for (auto& [tname, t] : timers_) {
    TimerSnapshot ts;
    ts.durationMs = t.getDuration();
    ts.state = t.getState();
    ts.eventType = t.getEventType();
    in.timersSnapshot[tname] = ts;
  }

  // Provide at most one new communication message for this cycle
  in.newCommMsg = pendingCommMsg_;

  // Let core compute effects
  CycleEffects fx;
  if (core_) {
    fx = core_->step(in);
  }

  // This cycle has consumed the pending comm message (if any)
  pendingCommMsg_ = std::nullopt;

  // Apply output changes (deferred single write)
  if (!fx.outputChanges.empty()) {
    for (const auto& [name, state] : fx.outputChanges) {
      outputChannels_[name].state = state;
    }
    outputsUpdated_ = true;
  }

  // Apply timer commands
  for (const auto& cmd : fx.timerCmds) {
    if (cmd.type == TimerCmd::Start) {
      // update duration if provided
      if (cmd.durationMs) {
        timers_[cmd.name].setDuration(*cmd.durationMs);
      }
      startTimer(cmd.name);
    } else {
      stopTimer(cmd.name);
    }
  }

  // Send comm messages
  for (const auto& s : fx.commSends) {
    auto it = activeCommPorts_.find(s.port);
    if (it != activeCommPorts_.end()) {
      it->second.send(s.data);
    } else {
      getLogger()->warn("[{}] comm send skipped; port '{}' not active", FUNCTION_NAME, s.port);
    }
  }

  // Handle calibration results
  if (fx.calibration) {
    emit calibrationResponse(fx.calibration->pulsesPerPage, fx.calibration->port);
  }

  // Single hardware write for this cycle unless GUI override enabled
  if (!overrideOutputs_ && outputsUpdated_) {
    getLogger()->debug("[{}] Applying output changes", FUNCTION_NAME);
    writeOutputs();
    outputsUpdated_ = false;
  }

  // Reset timer edge flags after the cycle
  for (auto& [name, t] : timers_) {
    t.setEventType(IOEventType::None);
    t.state_ = 0;
  }

  // Inputs flag is informational only now
  inputsUpdated_ = false;

  // Log the end of a logic cycle
  getLogger()->debug("[{}] Logic cycle completed", FUNCTION_NAME);

  // Publish barcode store snapshot for GUI
  if (core_) {
    auto snap = core_->getBarcodeStoreSnapshot();
    QMap<QString, QStringList> out;
    for (const auto& kv : snap) {
      const std::string& port = kv.first;
      const auto& vec = kv.second;
      QStringList list;
      list.reserve(static_cast<int>(vec.size()));
      for (const auto& s : vec) list.push_back(QString::fromStdString(s));
      out.insert(QString::fromStdString(port), list);
    }
    emit barcodeStoreUpdated(out);
  }
}

void Logic::startTimer(const std::string& timerName) {
  auto it = timers_.find(timerName);
  if (it == timers_.end()) {
    getLogger()->warn("[{}] startTimer: timer '{}' not found", FUNCTION_NAME, timerName);
    return;
  }
  int dur = it->second.getDuration();
  if (dur <= 0) return;
  it->second.start(std::chrono::milliseconds(dur), [this, timerName]() {
    eventQueue_.push(TimerEvent{timerName});
  });
}

void Logic::stopTimer(const std::string& timerName) {
  auto it = timers_.find(timerName);
  if (it == timers_.end()) return;
  it->second.cancel();
}

#include "moc_Logic.cpp"