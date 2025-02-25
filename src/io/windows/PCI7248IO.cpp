#include "io/PCI7248IO.h"
#include <iostream>
#include <chrono>
#include <thread>

PCI7248IO::PCI7248IO(EventQueue<IOEvent>* eventQueue, const Config& config)
    : eventQueue_(eventQueue), config_(config), card_(-1)
{
    // Load IO device info (optional)
    std::string device = config_.getIODevice();
    std::cout << "Configuring IO for device: " << device << std::endl;

    // Populate outputs from configuration
    for (const auto& mapping : config_.getOutputs()) {
        ioPinMap_[mapping.name] = mapping.pin;
        ioStateMap_[mapping.name] = 0;
        std::cout << "Configured output: " << mapping.name << " on pin " << mapping.pin << std::endl;
    }

    // Populate inputs from configuration
    for (const auto& mapping : config_.getInputs()) {
        ioPinMap_[mapping.name] = mapping.pin;
        ioStateMap_[mapping.name] = 0;
        std::cout << "Configured input: " << mapping.name << " on pin " << mapping.pin << std::endl;
    }
}

PCI7248IO::~PCI7248IO() {
    if (card_ >= 0) {
        Release_Card(card_);
    }
}

bool PCI7248IO::initialize() {
    // Register the PCI-7248 card
    card_ = Register_Card(PCI_7248, 0);
    if (card_ < 0) {
        std::cerr << "Failed to register PCI-7248! Error Code: " << card_ << std::endl;
        return false;
    }
    // Configure ports:
    if (DIO_PortConfig(card_, Channel_P1A, OUTPUT_PORT) != 0) {
        std::cerr << "Failed to configure Port A." << std::endl;
        return false;
    }
    if (DIO_PortConfig(card_, Channel_P1B, INPUT_PORT) != 0) {
        std::cerr << "Failed to configure Port B." << std::endl;
        return false;
    }
    if (DIO_PortConfig(card_, Channel_P1CL, OUTPUT_PORT) != 0) {
        std::cerr << "Failed to configure Port CL." << std::endl;
        return false;
    }
    if (DIO_PortConfig(card_, Channel_P1CH, INPUT_PORT) != 0) {
        std::cerr << "Failed to configure Port CH." << std::endl;
        return false;
    }
    std::cout << "PCI7248IO initialized successfully." << std::endl;
    return true;
}

void PCI7248IO::updateInputs() {
    // Read inputs from Port B (pins 8-15)
    U32 portB = 0;
    if (DI_ReadPort(card_, Channel_P1B, &portB) != 0) {
        std::cerr << "Failed to read Port B." << std::endl;
        return;
    }
    for (int i = 0; i < 8; i++) {
        std::string ioName = "DI" + std::to_string(i);
        int pin = ioPinMap_[ioName];  // pin in [8,15]
        IOState newState = (portB >> (pin - 8)) & 0x1;
        if (newState != ioStateMap_[ioName]) {
            ioStateMap_[ioName] = newState;
            pushEvent(ioName, newState);
        }
    }
    // Read inputs from Port CH (pins 20-23)
    U32 portCH = 0;
    if (DI_ReadPort(card_, Channel_P1CH, &portCH) != 0) {
        std::cerr << "Failed to read Port CH." << std::endl;
        return;
    }
    for (int i = 8; i < 12; i++) {
        std::string ioName = "DI" + std::to_string(i);
        int pin = ioPinMap_[ioName];  // pin in [20,23]
        IOState newState = (portCH >> (pin - 20)) & 0x1;
        if (newState != ioStateMap_[ioName]) {
            ioStateMap_[ioName] = newState;
            pushEvent(ioName, newState);
        }
    }
}

bool PCI7248IO::writeOutput(const std::string& ioName, IOState state) {
    if (ioPinMap_.find(ioName) == ioPinMap_.end()) {
        std::cerr << "Output " << ioName << " not found." << std::endl;
        return false;
    }
    int pin = ioPinMap_[ioName];
    if (!writeHardware(pin, state)) {
        return false;
    }
    ioStateMap_[ioName] = state;
    return true;
}

bool PCI7248IO::writeOutputs(const std::unordered_map<std::string, IOState>& outputs) {
    bool success = true;
    for (const auto& out : outputs) {
        if (!writeOutput(out.first, out.second)) {
            success = false;
        }
    }
    return success;
}

const std::unordered_map<std::string, IOState>& PCI7248IO::getCurrentState() const {
    return ioStateMap_;
}

// Helper: Read an input value based on the IO name
IOState PCI7248IO::readInput(const std::string& ioName) {
    if (ioPinMap_.find(ioName) == ioPinMap_.end())
        return 0;
    int pin = ioPinMap_[ioName];
    if (pin >= 8 && pin <= 15) {  // Port B
        U32 portB = 0;
        if (DI_ReadPort(card_, Channel_P1B, &portB) == 0)
            return (portB >> (pin - 8)) & 0x1;
    } else if (pin >= 20 && pin <= 23) {  // Port CH
        U32 portCH = 0;
        if (DI_ReadPort(card_, Channel_P1CH, &portCH) == 0)
            return (portCH >> (pin - 20)) & 0x1;
    }
    return 0;
}

// Helper: Write an output value to hardware based on the pin number
bool PCI7248IO::writeHardware(int pinNumber, IOState state) {
    if (pinNumber >= 0 && pinNumber <= 7) {  // Port A (DO0-DO7)
        U32 currentPortA = 0;
        // Construct current port state from stored outputs
        for (int i = 0; i < 8; i++) {
            std::string ioName = "DO" + std::to_string(i);
            currentPortA |= (ioStateMap_[ioName] << i);
        }
        if (state == 0)
            currentPortA &= ~(1 << pinNumber);
        else
            currentPortA |= (1 << pinNumber);
        if (DO_WritePort(card_, Channel_P1A, currentPortA) != 0) {
            std::cerr << "Failed to write to Port A." << std::endl;
            return false;
        }
    } else if (pinNumber >= 16 && pinNumber <= 19) {  // Port CL (DO8-DO11)
        U32 currentPortCL = 0;
        for (int i = 8; i < 12; i++) {
            std::string ioName = "DO" + std::to_string(i);
            int mappedPin = ioPinMap_[ioName];  // Should be in [16,19]
            currentPortCL |= (ioStateMap_[ioName] << (mappedPin - 16));
        }
        if (state == 0)
            currentPortCL &= ~(1 << (pinNumber - 16));
        else
            currentPortCL |= (1 << (pinNumber - 16));
        if (DO_WritePort(card_, Channel_P1CL, currentPortCL) != 0) {
            std::cerr << "Failed to write to Port CL." << std::endl;
            return false;
        }
    } else {
        std::cerr << "Invalid pin number for output." << std::endl;
        return false;
    }
    return true;
}

void PCI7248IO::pushEvent(const std::string& ioName, IOState state) {
    if (eventQueue_) {
        IOEvent event { ioName, state };
        eventQueue_->push(event);
    }
}
