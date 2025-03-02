#include "MainLogic.h"
#include <iostream>

MainLogic::MainLogic(EventQueue<EventVariant> &eventQueue, const Config &config)
    : eventQueue_(eventQueue), config_(config), io_(&eventQueue_, config_), running_(true)
{
    if (!io_.initialize())
    {
        std::cerr << "Failed to initialize PCI7248IO." << std::endl;
        running_ = false;
    }
}

MainLogic::~MainLogic()
{
    running_ = false;
    if (logicThread_.joinable())
    {
        logicThread_.join();
    }
}

void MainLogic::run()
{
    logicThread_ = std::thread([this]()
                               {
        while (running_) {
            EventVariant event;
            eventQueue_.wait_and_pop(event);

            std::visit([this](auto&& e) { this->handleEvent(e); }, event);
        } });
}

void MainLogic::stop()
{
    running_ = false;
    if (logicThread_.joinable())
    {
        logicThread_.join();
    }
}

// **Event Handlers**
void MainLogic::handleEvent(const IOEvent &event)
{
    std::cout << "[IO Event] Processing input changes..." << std::endl;
    for (const auto &pair : event.channels)
    {
        const IOChannel &channel = pair.second;
        std::cout << "  " << channel.name << " -> " << channel.state
                  << " (" << (channel.eventType == IOEventType::Rising ? "Rising" : "Falling") << ")"
                  << std::endl;
    }
    if (event.channels.at("stopButton").eventType == IOEventType::Rising &&
        event.channels.at("startButton").state == 0)
    {

        std::cout << "Stop button pressed" << std::endl;
        // outputChannels["motor"].state = 0;
    }
}

void MainLogic::handleEvent(const CommEvent &event)
{
    std::cout << "[Comm Event] Received: " << event.message << std::endl;
}

void MainLogic::handleEvent(const GUIEvent &event)
{
    std::cout << "[GUI Event] UI Update: " << event.uiMessage << std::endl;
}

void MainLogic::handleEvent(const TimerEvent &event)
{
    std::cout << "[Timer Event] Timer ID: " << event.timerId << " triggered." << std::endl;
}
