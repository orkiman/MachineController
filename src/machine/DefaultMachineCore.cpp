#include "machine/MachineCore.h"

class DefaultMachineCore : public MachineCore {
  bool blinkLed0_ = false;
public:
  void setBlinkLed(bool v) override { blinkLed0_ = v; }

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

    // Handle comm messages (JSON and raw)
    for (const auto& m : in.cellMsgs) {
      if (m.parsed && (*m.parsed).contains("type") && (*m.parsed)["type"] == "calibration_result") {
        if ((*m.parsed).contains("pulsesPerPage")) {
          fx.calibration = CalibrationResult{ (*m.parsed)["pulsesPerPage"].get<int>(), m.port };
          // keep scanning if multiple; or break if only one matters
        }
      } else {
        // raw small string example can be handled here based on m.port/m.offset/m.raw
      }
    }

    return fx;
  }
};

// Factory helper (optional)
MachineCore* createDefaultMachineCore() {
  return new DefaultMachineCore();
}
