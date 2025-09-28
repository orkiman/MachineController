#include "machine/MachineCore.h"
#include <cctype>
#include <optional>
#include <unordered_set>

class DefaultMachineCore : public MachineCore {
  bool blinkLed0_ = false;
  bool lastLedState_ = false;
  // Per-port message storage owned by the machine core
  std::unordered_map<std::string, std::vector<std::string>> store_;
  // Fixed capacity for per-port vectors (configured by Config via Logic)
  std::size_t capacity_{0};

  // Sequence test config/state (from Tests tab)
  std::optional<int> lastSeqNumber_{};           // last extracted number
  int masterStartIndex_{0};                      // default aligns with GUI
  int masterLength_{1};                          // default aligns with GUI
  std::string sequenceDirection_{"Ascending"};  // "Ascending" or "Descending"
  bool masterSequenceEnabled_{false};            // off by default

  // Match test config/state (from Tests tab)
  bool matchTestEnabled_{false};
  int matchMasterStartIndex_{0};
  int matchReaderStartIndex_{0};
  int matchLength_{1};
  std::optional<int> lastMatchMaster_{};
  std::optional<int> lastMatchReader_{};

  // Master-in-File check (from Tests tab)
  bool masterInFileEnabled_{false};
  int masterInFileStartIndex_{0};
  int masterInFileLength_{1};
  std::unordered_set<std::string> masterFileSet_;

  // Ensure a given port's vector is sized to 'capacity_'
  void ensurePortCapacity(const std::string& port) {
    if (capacity_ == 0) return;
    auto& vec = store_[port];
    if (vec.size() != capacity_) vec.resize(capacity_);
  }

  // Extract a raw slice [startIndex, startIndex+length) from text. Returns false if invalid.
  bool extractSliceAt(const std::string& text, int startIndex, int length, std::string& out) const {
    if (startIndex < 0 || length <= 0) return false;
    if (static_cast<size_t>(startIndex) >= text.size()) return false;
    size_t len = static_cast<size_t>(length);
    size_t avail = text.size() - static_cast<size_t>(startIndex);
    if (len > avail) len = avail;
    out = text.substr(static_cast<size_t>(startIndex), len);
    return true;
  }

  // Extract a number from text slice [startIndex, startIndex+length)
  bool extractNumberAt(const std::string& text, int startIndex, int length, int& out) const {
    if (startIndex < 0 || length <= 0) return false;
    if (static_cast<size_t>(startIndex) >= text.size()) return false;
    size_t len = static_cast<size_t>(length);
    size_t avail = text.size() - static_cast<size_t>(startIndex);
    if (len > avail) len = avail;
    const std::string slice = text.substr(static_cast<size_t>(startIndex), len);

    std::string digits;
    digits.reserve(slice.size());
    for (unsigned char ch : slice) {
      if (std::isdigit(ch)) digits.push_back(static_cast<char>(ch));
    }
    if (digits.empty()) return false;
    try {
      out = std::stoi(digits);
      return true;
    } catch (...) {
      return false;
    }
  }

  // Check master sequence based on current configuration.
  // Returns true if the sequence condition passes. Always stores the latest number if extracted.
  bool checkMasterSequence(const std::string& text) {
    if (!masterSequenceEnabled_) return true; // disabled => pass
    int current{};
    if (!extractNumberAt(text, masterStartIndex_, masterLength_, current)) {
      return false; // could not extract a number
    }

    bool pass = true;
    if (!lastSeqNumber_.has_value()) {
      pass = true; // first number passes by definition
    } else {
      if (sequenceDirection_ == "Descending") {
        pass = (current == lastSeqNumber_.value() - 1);
      } else {
        // Default to ascending
        pass = (current == lastSeqNumber_.value() + 1);
      }
    }
    // Update last seen number regardless, so next comparison is relative to this one
    lastSeqNumber_ = current;
    return pass;
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

  // Configure Tests: master sequence options (can be wired from Logic/UI later)
  void setMasterSequenceEnabled(bool enabled) override { masterSequenceEnabled_ = enabled; }
  void setMasterSequenceConfig(int startIndex, int length, const std::string& direction) override {
    if (startIndex < 0) startIndex = 0;
    if (length < 1) length = 1;
    masterStartIndex_ = startIndex;
    masterLength_ = length;
    sequenceDirection_ = direction;
  }
  void resetMasterSequence() override { lastSeqNumber_.reset(); }

  // Public check function that accepts a string and applies the sequence test.
  // Uses current configuration (start, length, direction). Stores last number.
  bool testMasterSequence(const std::string& text) override { return checkMasterSequence(text); }

  // Configure Match test options
  void setMatchTestEnabled(bool enabled) override { matchTestEnabled_ = enabled; }
  void setMatchTestConfig(int masterStartIndex, int matchStartIndex, int length) override {
    if (masterStartIndex < 0) masterStartIndex = 0;
    if (matchStartIndex < 0) matchStartIndex = 0;
    if (length < 1) length = 1;
    matchMasterStartIndex_ = masterStartIndex;
    matchReaderStartIndex_ = matchStartIndex;
    matchLength_ = length;
  }
  void resetMatchTest() override {
    lastMatchMaster_.reset();
    lastMatchReader_.reset();
  }

  bool testMatchReaders(const std::string& masterText, const std::string& matchText) override {
    if (!matchTestEnabled_) return true; // disabled => pass

    int masterValue{};
    if (!extractNumberAt(masterText, matchMasterStartIndex_, matchLength_, masterValue)) {
      return false;
    }

    int matchValue{};
    if (!extractNumberAt(matchText, matchReaderStartIndex_, matchLength_, matchValue)) {
      return false;
    }

    lastMatchMaster_ = masterValue;
    lastMatchReader_ = matchValue;
    return masterValue == matchValue;
  }

  // Configure/get store capacity
  void setStoreCapacity(std::size_t cap) override {
    capacity_ = cap;
    // Normalize existing ports to new capacity
    for (auto& kv : store_) {
      kv.second.resize(capacity_);
    }
  }

  std::size_t getStoreCapacity() const override { return capacity_; }

  // Master-in-File overrides
  void setMasterInFileCheckEnabled(bool enabled) override { masterInFileEnabled_ = enabled; }
  void setMasterInFileExtraction(int startIndex, int length) override {
    if (startIndex < 0) startIndex = 0;
    if (length < 1) length = 1;
    masterInFileStartIndex_ = startIndex;
    masterInFileLength_ = length;
  }
  void setMasterFileReferenceSet(const std::unordered_set<std::string>& set) override {
    masterFileSet_ = set;
  }
  bool testMasterInFile(const std::string& text) override {
    if (!masterInFileEnabled_) return true;
    if (masterFileSet_.empty()) return false; // enabled but no reference
    std::string token;
    if (!extractSliceAt(text, masterInFileStartIndex_, masterInFileLength_, token)) return false;
    return masterFileSet_.find(token) != masterFileSet_.end();
  }

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
      lastLedState_ = !lastLedState_;
      fx.outputChanges.emplace_back("o0", lastLedState_ ? 1 : 0);
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
