#include "MainLogic.h"
#include <iostream>
#include "Logger.h"

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

void MainLogic::run() {
    logicThread_ = std::thread([this]() {       
        while (running_) {
            EventVariant event;
            eventQueue_.wait_and_pop(event);

            std::visit([this](auto&& e) {
                this->handleEvent(e);
            }, event);
        }
    });
    
    if(running_) {
        outputChannels_ = io_.getOutputChannels();
        blinkThread_ = std::thread([this]() {
            // blinkLED("O0_motor");
            blinkLED("O21");
            
        });
    }
    
}


void MainLogic::stop() {
    // Push a termination event to unblock the waiting call.
    eventQueue_.push(TerminationEvent{});
    io_.stopPolling(); // Stop the IO polling thread if necessary.
    
    // Now join the logic thread from the main thread.
    if (logicThread_.joinable()) {
        logicThread_.join();
    }
    if (blinkThread_.joinable()) {
        blinkThread_.join();
        std::cout << "Blink thread joined." << std::endl;
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
          << " channel.eventType = " 
          << (channel.eventType == IOEventType::Rising ? "Rising" : 
              (channel.eventType == IOEventType::Falling ? "Falling" : 
              (channel.eventType == IOEventType::None ? "None" : "Unknown")))
          << std::endl;
    }
    const auto &in = event.channels;
    if (in.at("startButton").eventType == IOEventType::Rising &&
        in.at("stopButton").state == 0)
    {

        std::cout << "start process started" << std::endl;
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

void MainLogic::handleEvent(const TerminationEvent &event)
{
    // Just set running_ to false.
    running_ = false;
    // Optionally log the termination.
    getLogger()->info("TerminationEvent received; shutting down logic thread.");
}

void MainLogic::blinkLED(std::string channelName) {
    std::cout << "Blink thread started." << std::endl;
    while (running_) {
        // Toggle the LED state.
        outputChannels_[channelName].state = !outputChannels_[channelName].state;
        // std::cout << "Toggling " << channelName <<" state to " << outputChannels_[channelName].state << std::endl;
        io_.writeOutputs(outputChannels_);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}