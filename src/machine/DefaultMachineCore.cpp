#include "machine/MachineCore.h"

class DefaultMachineCore : public MachineCore {
  bool blinkLed0_ = false;
  // Per-port message storage owned by the machine core
  std::unordered_map<std::string, std::vector<std::string>> store_;
  // Fixed capacity for per-port vectors (configured by Config via Logic)
  std::size_t capacity_{0};

  // Ensure a given port's vector is sized to 'capacity_'
  void ensurePortCapacity(const std::string& port) {
    if (capacity_ == 0) return;
    auto& vec = store_[port];
    if (vec.size() != capacity_) vec.resize(capacity_);
  }

  // Helper: shift all messages in a given port's vector to the right by 'by'.
  // NOTE: No max-size policy here; the vector grows. Add your cap logic later.
  void shiftRightPort(const std::string& port, size_t by = 1) {
    if (by == 0 || capacity_ == 0) return;
    auto it = store_.find(port);
    if (it == store_.end()) return;
    auto& vec = it->second;
    if (vec.size() != capacity_) vec.resize(capacity_);
    if (capacity_ == 0) return;
    if (by > capacity_) by = capacity_;

    // Move from back to front within fixed capacity, dropping overflow
    for (size_t i = capacity_; i-- > 0; ) {
      size_t dest = i + by;
      if (dest < capacity_) {
        vec[dest] = std::move(vec[i]);
      }
      // else: drop overflow beyond capacity
    }
    // Clear the first 'by' positions
    for (size_t i = 0; i < by; ++i) vec[i].clear();
    //
  }

public:
  void setBlinkLed(bool v) override { blinkLed0_ = v; }

  // Configure/get store capacity
  void setStoreCapacity(std::size_t cap) override {
    capacity_ = cap;
    // Normalize existing ports to new capacity
    for (auto& kv : store_) {
      kv.second.resize(capacity_);
    }
  }

  std::size_t getStoreCapacity() const override { return capacity_; }

  std::unordered_map<std::string, std::vector<std::string>> getBarcodeStoreSnapshot() const override {
    // Ensure vectors are exactly capacity_ in the snapshot
    auto copy = store_;
    if (capacity_ > 0) {
      for (auto& kv : copy) kv.second.resize(capacity_);
    }
    return copy;
  }

  CycleEffects step(const CycleInputs& in) override {
    CycleEffects fx;

    // LED blink demo: toggle o0 on timer1 rising edge if enabled
    auto t = in.timerEdges.find("timer1");
    if (blinkLed0_ && t != in.timerEdges.end() && t->second.rising) {
      // For demo we set to 1; Logic can compute toggle if needed with current state
      fx.outputChanges.emplace_back("o0", 1);
      fx.timerCmds.push_back({TimerCmd::Start, "timer1", std::nullopt});
    } else if (!blinkLed0_) {
      fx.outputChanges.emplace_back("o0", 0);
    }

    // Example: start condition using inputs i8/i9
    auto i8 = in.inputs.find("i8");
    auto i9 = in.inputs.find("i9");
    if (i8 != in.inputs.end() && i9 != in.inputs.end()) {
      if (i8->second.eventType == IOEventType::Rising && i9->second.state == 0) {
        fx.outputChanges.emplace_back("startRelay", 1);
      }
    }

    // Handle at most one new communication message this cycle
    if (in.newCommMsg) {
      const auto& m = *in.newCommMsg;
      // Example: handle calibration_result immediately
      if (m.parsed && (*m.parsed).contains("type") && (*m.parsed)["type"] == "calibration_result") {
        if ((*m.parsed).contains("pulsesPerPage")) {
          fx.calibration = CalibrationResult{ (*m.parsed)["pulsesPerPage"].get<int>(), m.commName };
        }
      } else {
        // Default behavior: store by offset within fixed capacity (deferred handling)
        ensurePortCapacity(m.commName);
        if (capacity_ == 0) {
          // No capacity configured yet; ignore storing
        } else {
          std::size_t idx = static_cast<std::size_t>(m.offset);
          if (idx < capacity_) {
            store_[m.commName][idx] = m.raw;
            // Mark that barcode/message store changed this cycle
            fx.barcodeStoreChanged = true;
          }
          // else: offset beyond capacity; ignore or clamp (choosing ignore)
        }
      }
    }

    // Demo: when input i8 has a rising edge, shift the latest message port to the right by 1
    // Adjust the input name and port selection to your real machine logic.
    if (i8 != in.inputs.end() && i8->second.eventType == IOEventType::Rising) {
      shiftRightPort("communication1", 1);
      // Mark that barcode/message store changed due to shift
      fx.barcodeStoreChanged = true;
    }

    return fx;
  }
};

// Factory helper (optional)
MachineCore* createDefaultMachineCore() {
  return new DefaultMachineCore();
}
