#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstddef>
#include "io/IOChannel.h"
#include "json.hpp"

struct CommCellMessage {
  std::string commName;
  int offset{0};
  std::string raw;
  std::optional<nlohmann::json> parsed; // present if JSON parsing succeeded
};

struct TimerEdge {
  bool rising{false};
  bool falling{false};
};

struct TimerSnapshot {
  int durationMs{0};
  int state{0};                 // 0=inactive, 1=active
  IOEventType eventType{IOEventType::None};
};

struct CycleInputs {
  const std::unordered_map<std::string, IOChannel>& inputs;
  std::unordered_map<std::string, TimerEdge> timerEdges; // timers that fired this cycle
  std::unordered_map<std::string, IOChannel> outputsSnapshot; // snapshot of current outputs
  std::unordered_map<std::string, TimerSnapshot> timersSnapshot; // snapshot of timers
  std::optional<CommCellMessage> newCommMsg; // message received this cycle (if any)
  bool blinkLed0{false};                                  // example machine flag
};

struct TimerCmd {
  enum Type { Start, Stop } type{Start};
  std::string name;
  std::optional<int> durationMs; // if empty, reuse existing timer duration
};

struct CommSend {
  std::string commName;
  std::string data;
};

struct CalibrationResult {
  int pulsesPerPage{0};
  std::string commName;
};

struct CycleEffects {
  std::vector<std::pair<std::string,int>> outputChanges; // name -> state
  std::vector<TimerCmd> timerCmds;
  std::vector<CommSend> commSends;
  // Set to true if the machine core modified its barcode/message store in this cycle
  bool barcodeStoreChanged{false};
  std::optional<CalibrationResult> calibration;
};

class MachineCore {
public:
  virtual ~MachineCore() = default;
  virtual CycleEffects step(const CycleInputs& in) = 0;
  // Optional knobs controlled by GUI
  virtual void setBlinkLed(bool) {}

  // Tests (optional hooks; default no-ops)
  // Configure master sequence check options
  virtual void setMasterSequenceEnabled(bool) {}
  virtual void setMasterSequenceConfig(int /*startIndex*/, int /*length*/, const std::string& /*direction*/) {}
  virtual void resetMasterSequence() {}
  // Apply master sequence test to a text message. Returns true if it passes.
  // Default implementation: always pass.
  virtual bool testMasterSequence(const std::string&) { return true; }

  // Barcode grid support (optional; default no-ops)
  // Configure the maximum number of machine cells (rows) maintained per channel
  virtual void setStoreCapacity(std::size_t) {}
  // Retrieve the configured capacity (0 if unsupported)
  virtual std::size_t getStoreCapacity() const { return 0; }
  // Snapshot of per-port message storage; vectors should be sized to capacity
  virtual std::unordered_map<std::string, std::vector<std::string>> getBarcodeStoreSnapshot() const { return {}; }
};
