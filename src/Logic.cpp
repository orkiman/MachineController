#include "Logic.h"
#include <iostream>
#include "Logger.h"

Logic::Logic(EventQueue<EventVariant> &eventQueue, const Config &config)
    : eventQueue_(eventQueue), config_(config), io_(eventQueue_, config), controllerRunning_(true),
      communication1_(eventQueue, "communication1", config),
      communication2_(eventQueue, "communication2", config)
{
    if (!io_.initialize())
    {
        std::cerr << "Failed to initialize PCI7248IO." << std::endl;
        controllerRunning_ = false;
    }

    RS232Communication communication (eventQueue_, "communication1", config_);
    if (!communication.initialize())
    {
        std::cerr << "Failed to initialize RS232Communication." << std::endl;
        controllerRunning_ = false;
    }
    else
    {
        communication1_ = std::move(communication);
        std::cout << "RS232Communication initialized successfully." << std::endl;
    }
    
}

Logic::~Logic()
{
    controllerRunning_ = false;
    if (blinkThread_.joinable())
    {
        blinkThread_.join();
        getLogger()->info("Blink thread joined.");
    }
}

void Logic::run()
{
    if (controllerRunning_)
    {
        outputChannels_ = io_.getOutputChannels();
        blinkThread_ = std::thread([this]()
                                   { blinkLED("o0"); });
    }

    while (controllerRunning_)
    {
        EventVariant event;
        eventQueue_.wait_and_pop(event);

        std::visit([this](auto &&e)
                   { this->handleEvent(e); }, event);
    }
}

void Logic::stop()
{
    static std::once_flag stopFlag;
    std::call_once(stopFlag, [this]()
                   {
                       eventQueue_.push(TerminationEvent{});
                       io_.stopPolling(); });
}

// **Event Handlers**
void Logic::handleEvent(const IOEvent &event)
{
    std::cout << "[IO Event] Processing input changes..." << std::endl;
    for (const auto &pair : event.channels)
    {
        const IOChannel &channel = pair.second;
        std::cout << "  " << channel.name << " -> " << channel.state
                  << " channel.eventType = "
                  << (channel.eventType == IOEventType::Rising ? "Rising" : (channel.eventType == IOEventType::Falling ? "Falling" : (channel.eventType == IOEventType::None ? "None" : "Unknown")))
                  << std::endl;
    }
    const auto &in = event.channels;
    if (in.at("i8").eventType == IOEventType::Rising &&
        in.at("i9").state == 0)
    {
        std::cout << "start process started" << std::endl;
    }
    if (in.at("i11").eventType == IOEventType::Rising)
    {
        emit updateGui("i11 on, t1 is off");
        t1_.start(std::chrono::milliseconds(2000), [this]()
                  { emit updateGui("i11 on, t1 is on"); });
    }
    if (in.at("i11").eventType == IOEventType::Falling)
    {
        emit updateGui("i11 off t1 is off");
        t1_.cancel();
    }
}

void Logic::handleEvent(const CommEvent &event)
{
    std::cout << "[Comm Event] Received from port " 
              << event.port << ": " << event.message << std::endl;
}

void Logic::handleEvent(const GuiEvent &event)
{
    switch (event.type)
    {
    case GuiEventType::ButtonPress:
        std::cout << "[GUI Event] Button pressed: " << event.data << std::endl;
        // For example, if this button press should trigger an output change:
        // setOutput(...);
        break;
    case GuiEventType::SetOutput:
        std::cout << "[GUI Event] Set output " << event.identifier << " to "
                  << (event.intValue == 0 ? "OFF" : "ON") << std::endl;
        outputChannels_[event.identifier].state = event.intValue;
        io_.writeOutputs(outputChannels_);
        // Call a function to set the output accordingly, e.g.:
        // setOutput(event.outputId, event.boolValue);
        break;
    case GuiEventType::SetVariable:
        std::cout << "[GUI Event] Set variable: " << event.data
                  << " Value: " << event.intValue << std::endl;
        // setVariable(event.intValue);
        break;
    case GuiEventType::ParameterChange:
        std::cout << "[GUI Event] Parameter changed: " << event.data
                  << " New value: " << event.intValue << std::endl;
        // adjustParameter(event.intValue);
        break;
    case GuiEventType::StatusRequest:
        std::cout << "[GUI Event] Status request received." << std::endl;
        // trigger a status update
        break;
    case GuiEventType::StatusUpdate:
        std::cout << "[GUI Event] Status update: " << event.data << std::endl;
        break;
    case GuiEventType::ErrorMessage:
        std::cerr << "[GUI Event] Error: " << event.data << std::endl;
        break;
    case GuiEventType::GuiMessage:
        // Emit a signal that will be connected to MainWindow to display the message
        emit guiMessage(QString::fromStdString(event.data), QString::fromStdString(event.identifier));
        break;
    case GuiEventType::SendCommunicationMessage:
        if (event.identifier == "communication1")
        {
            if (!communication1_.send(event.data))
            {
                getLogger()->error("[GUI Event] SendMessage: Failed to send message to communication1");
            }else{
                getLogger()->info("[GUI Event] SendMessage: Message sent to communication1");
            }
        }
        else if (event.identifier == "communication2")
        {
            if (!communication2_.send(event.data))
            {
                getLogger()->error("[GUI Event] SendMessage: Failed to send message to communication2");
            }else{
                getLogger()->info("[GUI Event] SendMessage: Message sent to communication2");
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
}

void Logic::handleEvent(const TimerEvent &event)
{
    std::cout << "[Timer Event] Timer ID: " << event.timerId << " triggered." << std::endl;
}

void Logic::handleEvent(const TerminationEvent &event)
{
    controllerRunning_ = false;
    getLogger()->info("TerminationEvent received; shutting down logic thread.");
}

void Logic::blinkLED(std::string channelName)
{
    std::cout << "Blink thread started." << std::endl;
    while (controllerRunning_)
    {
        outputChannels_[channelName].state = !outputChannels_[channelName].state;
        io_.writeOutputs(outputChannels_);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

void Logic::emergencyShutdown()
{
    io_.resetConfiguredOutputPorts();
}

#include "moc_Logic.cpp"