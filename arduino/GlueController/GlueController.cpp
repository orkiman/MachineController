// ==== R4 mode switch (set to 1 for fastest hardware timer ISR on Uno R4) ====
#define USE_R4_HWTIMER 1
// ===========================================================================

#include "GlueController.h"
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>

// ===== Globals =====
ControllerConfig config;

// Not hot-path: store rows dynamically here
struct GunConfigInternal {
  bool enabled = true;
  std::vector<GlueRow> rows;
};
static GunConfigInternal _guns_internal[4];
GunConfig guns[4]; // placeholder if other code references it

volatile uint32_t encoderCount = 0; // raw (wraps)
static uint32_t lastEncoderRaw = 0; // for delta
int64_t positionAccum = 0;          // monotonic
int64_t currentPosition64 = 0;      // snapshot used in loop

int pageLength = 0;
bool isCalibrating = false;

unsigned long lastHeartbeat = 0;
bool lastSensorState = HIGH;

String inputBuffer = "";

bool gunStates[4] = {false,false,false,false};
bool allFiringZonesInserted = false;
int64_t firingBasePosition = 0;

int  currentThreshold = 0; // 0..ADC_MAX

// Lines-mode: targets in ADC counts and per-gun phase state
static int lineStartADC = 0;
static int lineHoldADC  = 0;
static bool lineActive[4]   = {false,false,false,false};
static bool lineInHold[4]   = {false,false,false,false};
static unsigned long linePhaseMs[4] = {0,0,0,0};
static bool linePWMon[4]    = {false,false,false,false};

ZoneRing firingZones[4];

// ----- ADC backends -----
#if defined(__AVR__)
// ===== AVR (Uno R3) free-running ADC (10-bit) =====
volatile uint16_t gunCurrentADC10[4] = {512,512,512,512};
static volatile uint8_t _adcIndex = 0;

ISR(ADC_vect) {
  gunCurrentADC10[_adcIndex] = ADC;              // 10-bit result
  _adcIndex = (uint8_t)((_adcIndex + 1) & 0x03); // next channel
  uint8_t ch = analogPinToChannel(OUTPUT_CURRENT_PINS[_adcIndex]);
  ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
}

static void initFastADC_AVR() {
  ADMUX  = _BV(REFS0); // AVcc reference
  uint8_t ch = analogPinToChannel(OUTPUT_CURRENT_PINS[0]);
  ADMUX |= (ch & 0x0F);
  // Free-run, prescaler=32 (~500kHz ADC clk) → ~26us per sample
  ADCSRA = _BV(ADEN)|_BV(ADATE)|_BV(ADIE)|_BV(ADPS2)|_BV(ADPS0);
  ADCSRB = 0;
  ADCSRA |= _BV(ADSC);
}

static inline int getCurrentRaw(uint8_t gun) { return (int)gunCurrentADC10[gun]; } // 0..1023

#else
// ===== R4 (Renesas) timer-driven sampler (12-bit) =====
volatile uint16_t gunCurrentADC12[4] = {2048,2048,2048,2048};

  #if (USE_R4_HWTIMER==1) && (defined(ARDUINO_UNOR4_MINIMA) || defined(ARDUINO_UNOR4_WIFI))
    #include <HardwareTimer.h>
    static HardwareTimer *adcTimer = nullptr;
    static volatile uint8_t adcChanIdx = 0;
    static void adcTimerISR() {
      uint8_t ch = adcChanIdx;
      gunCurrentADC12[ch] = analogRead(OUTPUT_CURRENT_PINS[ch]); // ~10-15us typical
      adcChanIdx = (uint8_t)((adcChanIdx + 1) & 0x03);
    }
    void startADCSampler(uint32_t totalSampleRateHz) {
      analogReadResolution(12);
      #ifdef analogReadAveraging
        analogReadAveraging(1);
      #endif
      adcTimer = new HardwareTimer(0); // adjust index if needed by your core
      adcTimer->setOverflow(totalSampleRateHz, HERTZ); // one sample per tick (rotates channels)
      adcTimer->attachInterrupt(adcTimerISR);
      adcTimer->resume();
    }
  #else
    // Soft high-rate scheduler using micros() (non-blocking for updateGuns)
    static volatile uint8_t adcChanIdx = 0;
    static uint32_t adcNextTick = 0;
    static uint32_t adcTickPeriodUs = 40; // ~25 kS/s total by default

    static inline void adcSoftTick() {
      uint32_t now = micros();
      if ((int32_t)(now - adcNextTick) >= 0) {
        adcNextTick += adcTickPeriodUs;
        uint8_t ch = adcChanIdx;
        gunCurrentADC12[ch] = analogRead(OUTPUT_CURRENT_PINS[ch]);
        adcChanIdx = (uint8_t)((adcChanIdx + 1) & 0x03);
      }
    }
    void startADCSampler(uint32_t totalSampleRateHz) {
      analogReadResolution(12);
      #ifdef analogReadAveraging
        analogReadAveraging(1);
      #endif
      if (totalSampleRateHz < 4000) totalSampleRateHz = 4000;
      adcTickPeriodUs = (uint32_t)(1000000UL / totalSampleRateHz);
      adcNextTick = micros() + adcTickPeriodUs;
    }
  #endif

static inline int getCurrentRaw(uint8_t gun) { return (int)gunCurrentADC12[gun]; } // 0..4095
#endif
// ----- end ADC backends -----

// ===== Ring helpers =====
static inline bool ring_push(ZoneRing &r, const ActiveZone &z){
  if (r.count >= MAX_ZONES_PER_GUN) return false;
  r.buf[r.tail] = z;
  r.tail = (uint8_t)((r.tail + 1) % MAX_ZONES_PER_GUN);
  r.count++; return true;
}
static inline bool ring_pop(ZoneRing &r){
  if (!r.count) return false;
  r.head = (uint8_t)((r.head + 1) % MAX_ZONES_PER_GUN);
  r.count--; return true;
}
static inline ActiveZone& ring_peek(ZoneRing &r, uint8_t idxFromHead){
  uint8_t idx = (uint8_t)((r.head + idxFromHead) % MAX_ZONES_PER_GUN);
  return r.buf[idx];
}

// ===== ISR =====
void encoderISR(){ encoderCount++; }

// ===== Setup / Loop =====
void setup() {
  Serial.begin(115200);

  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(SENSOR_PIN,     INPUT_PULLUP);
  pinMode(STATUS_LED,     OUTPUT);
  for (int i=0;i<4;i++){ pinMode(GUN_PINS[i], OUTPUT); digitalWrite(GUN_PINS[i], LOW); }

  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderISR, RISING);

  for (int i=0;i<4;i++){ _guns_internal[i].enabled = true; _guns_internal[i].rows.clear(); }

#if defined(__AVR__)
  initFastADC_AVR();
#else
  // Start R4 sampler: e.g., 40 kS/s total (~10 kS/s per channel)
  startADCSampler(40000);
#endif

  digitalWrite(STATUS_LED, HIGH);
  lastSensorState = digitalRead(SENSOR_PIN);
}

void loop() {
#if !defined(__AVR__)
  // soft-timer path needs ticking; hwtimer path doesn't
  #if !((USE_R4_HWTIMER==1) && (defined(ARDUINO_UNOR4_MINIMA) || defined(ARDUINO_UNOR4_WIFI)))
    adcSoftTick();
  #endif
#endif

  // ===== Wrap-safe encoder to 64-bit monotonic =====
  noInterrupts(); uint32_t raw = encoderCount; interrupts();
  uint32_t delta = raw - lastEncoderRaw;   // unsigned handles wrap
  lastEncoderRaw = raw;
  positionAccum += (int64_t)delta;
  currentPosition64 = positionAccum;

  processSerial();
  checkSensor();

  if (config.enabled) updateGuns(); else shutdownAllGuns();

  if (!allFiringZonesInserted) {
    bool any=false; for(int i=0;i<4;i++){ if(gunStates[i]){ any=true; break; } }
    if (!any) calculateFiringZones();
  }
}

// ===== Serial / Protocol =====
void processSerial(){
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == STX) { inputBuffer = ""; }
    else if (c == ETX) {
      if (inputBuffer.length() > 0) {
        StaticJsonDocument<1024> doc;
        DeserializationError err = deserializeJson(doc, inputBuffer);
        if (!err) {
          String type = doc["type"] | "";
          if      (type == "controller_setup" || type == "config") {
            handleConfig(doc.as<JsonObject>());
          } else if (type == "plan") {
            handlePlan(doc.as<JsonObject>());
          } else if (type == "run") {
            config.enabled = true; digitalWrite(STATUS_LED, HIGH);
          } else if (type == "stop") {
            config.enabled = false; shutdownAllGuns(); digitalWrite(STATUS_LED, LOW);
          } else if (type == "calibrate") {
            initCalibration(doc.as<JsonObject>());
          } else if (type == "heartbeat") {
            handleHeartbeat(doc.as<JsonObject>()); // no-op
          }
        }
        inputBuffer = "";
      }
    } else { inputBuffer += c; }
  }
}

void handleConfig(const JsonObject &json){
  config.type = json["controllerType"] | "dots";
  config.enabled = json["enabled"] | false;
  config.encoderPulsesPerMm = json["encoder"] | 1.0;
  config.sensorOffset = json["sensorOffset"] | 10;
  config.startCurrent = json["startCurrent"] | 1.0;
  // startDuration is specified in milliseconds
  config.startDuration = json["startDuration"] | 500;
  config.holdCurrent = json["holdCurrent"] | 0.5;
  config.minimumSpeed = json["minimumSpeed"] | 0.0; // mm/s (0 disables gating)
  config.dotSize = json["dotSize"] | "medium";

  config.sensorOffsetInPulses = (int)(config.sensorOffset * config.encoderPulsesPerMm);

  double thrA = 0.0;
  if (config.dotSize == "small")       thrA = SMALL_DOT_THRESHOLD;
  else if (config.dotSize == "medium") thrA = MEDIUM_DOT_THRESHOLD;
  else if (config.dotSize == "large")  thrA = LARGE_DOT_THRESHOLD;

  // Scale to ADC counts using ADC_MAX
  currentThreshold = (int)((thrA * 0.8 + 2.5) * (double)ADC_MAX / 5.0);

  // Precompute line-mode ADC targets from configured currents (same mapping as dots)
  lineStartADC = (int)((config.startCurrent * 0.8 + 2.5) * (double)ADC_MAX / 5.0);
  lineHoldADC  = (int)((config.holdCurrent  * 0.8 + 2.5) * (double)ADC_MAX / 5.0);

  digitalWrite(STATUS_LED, config.enabled ? HIGH : LOW);

  if (json.containsKey("guns")) {
    JsonArray gunsArray = json["guns"];
    for (JsonObject gunConfig : gunsArray) {
      int gunId = gunConfig["gunId"] | -1;
      if (gunId >= 0 && gunId < 4) {
        auto &g = _guns_internal[gunId];
        g.enabled = gunConfig["enabled"] | true;
        g.rows.clear();

        if (gunConfig.containsKey("rows")) {
          JsonArray rows = gunConfig["rows"];
          std::vector<GlueRow> tmp;
          for (JsonObject row : rows) {
            GlueRow r = {
              .from  = (int)(row["from"].as<float>()  * config.encoderPulsesPerMm) + config.sensorOffsetInPulses,
              .to    = (int)(row["to"].as<float>()    * config.encoderPulsesPerMm) + config.sensorOffsetInPulses,
              .space = (int)(row["space"].as<float>() * config.encoderPulsesPerMm)
            };
            tmp.push_back(r);
          }
          std::sort(tmp.begin(), tmp.end(), [](const GlueRow&a,const GlueRow&b){ return a.from < b.from; });
          for (const GlueRow &r : tmp) {
            if (!g.rows.empty() && g.rows.back().to >= r.from) {
              g.rows.back().to = std::max(g.rows.back().to, r.to);
            } else {
              g.rows.push_back(r);
            }
          }
        }
      }
    }
  }

  allFiringZonesInserted = false;
  for (int i=0;i<4;i++){ firingZones[i].head=firingZones[i].tail=firingZones[i].count=0; }
}

// Plan updates: update guns/rows only
void handlePlan(const JsonObject &json){
  // Update guns from either a full guns[] array, or a single gun + rows payload
  bool updated = false;
  if (json.containsKey("guns")) {
    JsonArray gunsArray = json["guns"];
    for (JsonObject gunConfig : gunsArray) {
      int gunId = gunConfig["gunId"] | -1;
      if (gunId >= 0 && gunId < 4) {
        auto &g = _guns_internal[gunId];
        g.enabled = gunConfig["enabled"] | g.enabled;
        g.rows.clear();
        if (gunConfig.containsKey("rows")) {
          JsonArray rows = gunConfig["rows"];
          std::vector<GlueRow> tmp;
          for (JsonObject row : rows) {
            GlueRow r = {
              .from  = (int)(row["from"].as<float>()  * config.encoderPulsesPerMm) + config.sensorOffsetInPulses,
              .to    = (int)(row["to"].as<float>()    * config.encoderPulsesPerMm) + config.sensorOffsetInPulses,
              .space = (int)(row["space"].as<float>() * config.encoderPulsesPerMm)
            };
            tmp.push_back(r);
          }
          std::sort(tmp.begin(), tmp.end(), [](const GlueRow&a,const GlueRow&b){ return a.from < b.from; });
          for (const GlueRow &r : tmp) {
            if (!g.rows.empty() && g.rows.back().to >= r.from) g.rows.back().to = std::max(g.rows.back().to, r.to);
            else g.rows.push_back(r);
          }
        }
        updated = true;
      }
    }
  } else if (json.containsKey("rows")) {
    int gunId = json["gunId"] | 0;
    if (gunId >= 0 && gunId < 4) {
      auto &g = _guns_internal[gunId];
      if (json.containsKey("enabled")) g.enabled = json["enabled"].as<bool>();
      g.rows.clear();
      JsonArray rows = json["rows"];
      std::vector<GlueRow> tmp;
      for (JsonObject row : rows) {
        GlueRow r = {
          .from  = (int)(row["from"].as<float>()  * config.encoderPulsesPerMm) + config.sensorOffsetInPulses,
          .to    = (int)(row["to"].as<float>()    * config.encoderPulsesPerMm) + config.sensorOffsetInPulses,
          .space = (int)(row["space"].as<float>() * config.encoderPulsesPerMm)
        };
        tmp.push_back(r);
      }
      std::sort(tmp.begin(), tmp.end(), [](const GlueRow&a,const GlueRow&b){ return a.from < b.from; });
      for (const GlueRow &r : tmp) {
        if (!g.rows.empty() && g.rows.back().to >= r.from) g.rows.back().to = std::max(g.rows.back().to, r.to);
        else g.rows.push_back(r);
      }
      updated = true;
    }
  }

  if (updated) {
    // Clear pending zones so new plan takes effect on next sensor trigger
    allFiringZonesInserted = false;
    for (int i=0;i<4;i++){
      firingZones[i].head=firingZones[i].tail=firingZones[i].count=0;
      lineActive[i]=false; lineInHold[i]=false; linePWMon[i]=false;
    }
  }
}

void initCalibration(const JsonObject &json){
  pageLength = json["pageLength"] | 1000;
  isCalibrating = true;
  shutdownAllGuns();
}

void handleCalibrationSensorStateChange(bool sensorState){
  if (sensorState == LOW) { noInterrupts(); encoderCount = 0; interrupts(); lastEncoderRaw = 0; positionAccum = 0; }
  else {
    uint32_t raw; noInterrupts(); raw = encoderCount; interrupts();
    uint32_t delta = raw - lastEncoderRaw;
    lastEncoderRaw = raw;
    positionAccum += (int64_t)delta;
    sendCalibrationResult((int)positionAccum); // pulses measured over page
    config.encoderPulsesPerMm = (double)positionAccum / (pageLength * 10.0);
    isCalibrating = false;
  }
}

void handleHeartbeat(const JsonObject &){ /* no-op (status removed) */ }

// Small, infrequent
void sendCalibrationResult(int pulses){
  StaticJsonDocument<128> doc;
  doc["type"]="calibration_result"; doc["pulsesPerPage"]=pulses;
  String out; serializeJson(doc,out);
  Serial.print(STX); Serial.print(out); Serial.println(ETX);
}

// ===== IO =====
static inline void setGun(uint8_t idx, bool on){ digitalWrite(GUN_PINS[idx], on?HIGH:LOW); }
void shutdownAllGuns(){ for (int i=0;i<4;i++) setGun(i,false); }

// ===== Sensor / Zones =====
void checkSensor(){
  bool st = digitalRead(SENSOR_PIN);
  if (st != lastSensorState) {
    if (isCalibrating) {
      handleCalibrationSensorStateChange(st);
    } else if (config.enabled) {
      if (st == LOW) {
        firingBasePosition = currentPosition64;   // 64-bit base
        allFiringZonesInserted = false;           // append next idle
      }
    }
    lastSensorState = st;
  }
}

void calculateFiringZones(){
  // Append-only; supports overlaps when sensor retriggers quickly
  for (int i=0;i<4;i++){
    const auto &g = _guns_internal[i];
    if (!g.enabled) continue;
    for (const GlueRow &row : g.rows){
      ActiveZone z;
      z.from  = firingBasePosition + (int64_t)row.from;
      z.to    = firingBasePosition + (int64_t)row.to;
      z.space = row.space;
      z.next  = z.from;  // first drop at 'from'
      ring_push(firingZones[i], z); // drop silently if ring full
    }
  }
  allFiringZonesInserted = true;
}

// ===== Hot path =====
void updateGuns(){
  if (isCalibrating) return;

  // --- Compute carriage speed (mm/s), update ~every 10ms ---
  static unsigned long spLastUs = 0;
  static int64_t       spLastPos = 0;
  static double        speedMmPerSec = 0.0;
  unsigned long nowUs = micros();
  if (spLastUs == 0) { spLastUs = nowUs; spLastPos = currentPosition64; }
  else {
    unsigned long du = nowUs - spLastUs;
    if (du >= 10000UL) {
      int64_t dp = currentPosition64 - spLastPos; // pulses
      double  sec = (double)du / 1000000.0;
      double  pps = dp / sec; // pulses per second
      speedMmPerSec = pps / (config.encoderPulsesPerMm > 0 ? config.encoderPulsesPerMm : 1.0);
      spLastUs = nowUs; spLastPos = currentPosition64;
    }
  }

  bool isLinesMode = (config.type == "lines");
  bool speedTooLow = isLinesMode && (config.minimumSpeed > 0.0) && (speedMmPerSec < config.minimumSpeed);

  if (isLinesMode) {
    // Lines: start at 'from', keep ON; after startDuration we're in hold (state only), turn OFF at 'to'.
    for (int i=0;i<4;i++){
      if (!_guns_internal[i].enabled){ setGun(i,false); lineActive[i]=false; lineInHold[i]=false; continue; }

      if (firingZones[i].count){
        ActiveZone &zone = ring_peek(firingZones[i], 0);

        if (currentPosition64 > zone.to) {
          ring_pop(firingZones[i]);
          setGun(i,false);
          lineActive[i] = false; lineInHold[i] = false; linePWMon[i] = false;
        } else if (currentPosition64 >= zone.from) {
          if (!lineActive[i]){
            lineActive[i] = true; lineInHold[i] = false; linePhaseMs[i] = millis();
          }

          // Phase transition
          if (!lineInHold[i]){
            unsigned long elapsed = millis() - linePhaseMs[i];
            if (elapsed >= (unsigned long)(config.startDuration)) {
              lineInHold[i] = true;
            }
          }

          // Bang-bang control around target current (start or hold)
          int adcNow = getCurrentRaw(i);
          const int hyst = (int)(ADC_MAX / 64); // ~1.6% FS
          int target = lineInHold[i] ? lineHoldADC : lineStartADC;
          if (adcNow > target + hyst)      linePWMon[i] = false;
          else if (adcNow < target - hyst) linePWMon[i] = true;

          // Apply speed gating
          if (speedTooLow) setGun(i,false);
          else             setGun(i,linePWMon[i]);
        } else {
          // haven't reached 'from': ensure OFF
          setGun(i,false); linePWMon[i] = false;
        }
      } else {
        setGun(i,false);
        lineActive[i] = false; lineInHold[i] = false; linePWMon[i] = false;
      }
    }
    return; // lines mode handled
  }

  // --- Dots mode (existing logic) ---
  const int adcMid = (ADC_MAX / 2);
  const int deadband = INITIATION_DEADBAND_COUNTS;

  for (int i=0;i<4;i++){
    if (!_guns_internal[i].enabled){
      gunStates[i] = false; setGun(i,false); continue;
    }

    // Process only head zone (bounded time)
    if (firingZones[i].count){
      ActiveZone &zone = ring_peek(firingZones[i], 0);

      if (currentPosition64 > zone.to) {
        ring_pop(firingZones[i]);
      } else if (currentPosition64 >= zone.from) {
        if (zone.space > 0) {
          if (currentPosition64 >= zone.next) {
            int adcNow = getCurrentRaw(i);                   // 0..ADC_MAX
            if (abs(adcNow - adcMid) < deadband) gunStates[i] = true;

            int64_t diff  = currentPosition64 - zone.next;
            int64_t steps = (diff >= 0) ? (diff / zone.space + 1) : 1;
            zone.next += steps * (int64_t)zone.space;
            if (zone.next > zone.to) zone.next = zone.to + 1;
          }
        } else {
          int adcNow = getCurrentRaw(i);
          if (abs(adcNow - adcMid) < deadband) gunStates[i] = true;
        }
      }
    }

    // Fire until threshold reached (exact cut)
    if (gunStates[i]) {
      int adcNow = getCurrentRaw(i);
      if (adcNow >= currentThreshold) { setGun(i,false); gunStates[i]=false; }
      else                              setGun(i,true);
    } else {
      setGun(i,false);
    }
  }
}
