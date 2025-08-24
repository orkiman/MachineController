#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "io/IOChannel.h"
#include "json.hpp"

struct CommCellMessage {
  std::string port;
  int offset{0};
  std::string raw;
  std::optional<nlohmann::json> parsed; // present if JSON parsing succeeded
};

struct TimerEdge {
  bool rising{false};
  bool falling{false};
};

struct CycleInputs {
  const std::unordered_map<std::string, IOChannel>& inputs;
  std::unordered_map<std::string, TimerEdge> timerEdges; // timers that fired this cycle
  std::vector<CommCellMessage> cellMsgs;                 // messages received this cycle
  bool blinkLed0{false};                                  // example machine flag
};

struct TimerCmd {
  enum Type { Start, Stop } type{Start};
  std::string name;
  std::optional<int> durationMs; // if empty, reuse existing timer duration
};

struct CommSend {
  std::string port;
  std::string data;
};

struct CalibrationResult {
  int pulsesPerPage{0};
  std::string port;
};

struct CycleEffects {
  std::vector<std::pair<std::string,int>> outputChanges; // name -> state
  std::vector<TimerCmd> timerCmds;
  std::vector<CommSend> commSends;
  std::optional<CalibrationResult> calibration;
};

class MachineCore {
public:
  virtual ~MachineCore() = default;
  virtual CycleEffects step(const CycleInputs& in) = 0;
  // Optional knobs controlled by GUI
  virtual void setBlinkLed(bool) {}
};
