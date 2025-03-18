#include "Logic.h"
#include <iostream>
#include "Logger.h"

Logic::Logic(EventQueue<EventVariant> &eventQueue, const Config &config)
    : eventQueue_(eventQueue), config_(config), io_(&eventQueue_, config_), running_(true)
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
    }
}

void Logic::run() {
    if(running_) {
        outputChannels_ = io_.getOutputChannels();
        blinkThread_ = std::thread([this]() {
            blinkLED("O0");
        });
    }

    while (running_) {
        EventVariant event;
        eventQueue_.wait_and_pop(event);

        std::visit([this](auto&& e) {
            this->handleEvent(e);
        }, event);
    }

    if (blinkThread_.joinable()) {
        blinkThread_.join();
        getLogger()->info("Blink thread joined.");
    }
}

void Logic::stop() {
    static std::once_flag stopFlag;
    std::call_once(stopFlag, [this]() {
        eventQueue_.push(TerminationEvent{});
        io_.stopPolling();

        if (blinkThread_.joinable()) {
            blinkThread_.join();
            getLogger()->info("Blink thread joined.");
        }
    });
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
          << (channel.eventType == IOEventType::Rising ? "Rising" : 
              (channel.eventType == IOEventType::Falling ? "Falling" : 
              (channel.eventType == IOEventType::None ? "None" : "Unknown")))
          << std::endl;
    }
    const auto &in = event.channels;
    if (in.at("i8").eventType == IOEventType::Rising &&
        in.at("i9").state == 0)
    {
        std::cout << "start process started" << std::endl;
    }
}

void Logic::handleEvent(const CommEvent &event)
{
    std::cout << "[Comm Event] Received: " << event.message << std::endl;
}

void Logic::handleEvent(const GUIEvent &event)
{
    std::cout << "[GUI Event] UI Update: " << event.uiMessage << std::endl;
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

void Logic::blinkLED(std::string channelName) {
    std::cout << "Blink thread started." << std::endl;
    while (running_) {
        outputChannels_[channelName].state = !outputChannels_[channelName].state;
        io_.writeOutputs(outputChannels_);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

void Logic::emergencyShutdown() {
    io_.resetConfiguredOutputPorts();
}
