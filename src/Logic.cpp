#include "Logic.h"
#include <iostream>
#include "Logger.h"

Logic::Logic(EventQueue<EventVariant> &eventQueue, const Config &config)
    : eventQueue_(eventQueue), config_(config), io_(eventQueue_, config_), running_(true)
{
    if (!io_.initialize())
    {
        std::cerr << "Failed to initialize PCI7248IO." << std::endl;
        running_ = false;
    }
}

Logic::~Logic()
{
    running_ = false;
    if (blinkThread_.joinable())
    {
        blinkThread_.join();
        getLogger()->info("Blink thread joined.");
    }
}

void Logic::run()
{
    if (running_)
    {
        outputChannels_ = io_.getOutputChannels();
        blinkThread_ = std::thread([this]()
                                   { blinkLED("o0"); });
    }

    while (running_)
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
    std::cout << "[Comm Event] Received: " << event.message << std::endl;
}

void Logic::handleEvent(const GuiEvent &event)
{
    switch (event.type)
    {
    case GuiEventType::ButtonPress:
        std::cout << "[GUI Event] Button pressed: " << event.uiMessage << std::endl;
        // For example, if this button press should trigger an output change:
        // setOutput(...);
        break;
    case GuiEventType::SetOutput:
        std::cout << "[GUI Event] Set output " << event.outputName << " to "
                  << (event.intValue == 0 ? "OFF" : "ON") << std::endl;
        outputChannels_[event.outputName].state = event.intValue;
        io_.writeOutputs(outputChannels_);
        // Call a function to set the output accordingly, e.g.:
        // setOutput(event.outputId, event.boolValue);
        break;
    case GuiEventType::SetVariable:
        std::cout << "[GUI Event] Set variable: " << event.uiMessage
                  << " Value: " << event.intValue << std::endl;
        // setVariable(event.intValue);
        break;
    case GuiEventType::ParameterChange:
        std::cout << "[GUI Event] Parameter changed: " << event.uiMessage
                  << " New value: " << event.intValue << std::endl;
        // adjustParameter(event.intValue);
        break;
    case GuiEventType::StatusRequest:
        std::cout << "[GUI Event] Status request received." << std::endl;
        // trigger a status update
        break;
    case GuiEventType::StatusUpdate:
        std::cout << "[GUI Event] Status update: " << event.uiMessage << std::endl;
        break;
    case GuiEventType::ErrorMessage:
        std::cerr << "[GUI Event] Error: " << event.uiMessage << std::endl;
        break;
    default:
        std::cout << "[GUI Event] Unknown event: " << event.uiMessage << std::endl;
        break;
    }
}

void Logic::handleEvent(const TimerEvent &event)
{
    std::cout << "[Timer Event] Timer ID: " << event.timerId << " triggered." << std::endl;
}

void Logic::handleEvent(const TerminationEvent &event)
{
    running_ = false;
    getLogger()->info("TerminationEvent received; shutting down logic thread.");
}

void Logic::blinkLED(std::string channelName)
{
    std::cout << "Blink thread started." << std::endl;
    while (running_)
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