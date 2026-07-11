#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "safety_kernel.h"
#include "hmi_kernel.h"
#include "runtime_kernel.h"
#include "telemetry_kernel.h"
#include "dashboard_kernel.h"

const uint8_t  NUM_CELLS      = 4;
const uint8_t  CELL_PINS[NUM_CELLS] = {34, 35, 32, 33};

const float    CELL_MIN_V     = 3.00f;
const float    CELL_MAX_V     = 4.20f;
const int      ADC_MAX        = 4095;

const float    IMBALANCE_MINOR_PCT    = 2.0f;
const float    IMBALANCE_CRITICAL_PCT = 5.0f;
const float    CELL_FAILURE_V         = 3.10f;

const unsigned long SAMPLE_INTERVAL_MS  = 200;
const unsigned long HMI_TICK_MS         = 100;

const uint8_t LED_HEALTHY  = 25;
const uint8_t LED_WARNING  = 26;
const uint8_t LED_CRITICAL = 27;

const uint8_t RELAY_PIN  = 13;
const uint8_t BUZZER_PIN = 14;

LiquidCrystal_I2C lcd(0x27, 16, 2);

enum HealthState { HEALTHY, MINOR_IMBALANCE, CRITICAL_IMBALANCE, PACK_FAILURE };

struct PackData {
  float cellV[NUM_CELLS];
  float packSum;
  float packAvg;
  float imbalancePct;
  uint8_t weakestIdx;
  uint8_t strongestIdx;
  HealthState state;
};

PackData pack;

unsigned long lastSampleTime = 0;
unsigned long lastHmiTick    = 0;

void readCellVoltages(PackData &p) {
  for (uint8_t i = 0; i < NUM_CELLS; i++) {
    int raw = analogRead(CELL_PINS[i]);
    p.cellV[i] = CELL_MIN_V + ((float)raw / ADC_MAX) * (CELL_MAX_V - CELL_MIN_V);
  }
}

void computePackStats(PackData &p) {
  p.packSum = 0;
  p.weakestIdx = 0;
  p.strongestIdx = 0;

  for (uint8_t i = 0; i < NUM_CELLS; i++) {
    p.packSum += p.cellV[i];
    if (p.cellV[i] < p.cellV[p.weakestIdx])   p.weakestIdx = i;
    if (p.cellV[i] > p.cellV[p.strongestIdx]) p.strongestIdx = i;
  }
  p.packAvg = p.packSum / NUM_CELLS;

  float spread = p.cellV[p.strongestIdx] - p.cellV[p.weakestIdx];
  p.imbalancePct = (spread / p.packAvg) * 100.0f;
}

HealthState classifyHealth(const PackData &p) {
  for (uint8_t i = 0; i < NUM_CELLS; i++) {
    if (p.cellV[i] < CELL_FAILURE_V) return PACK_FAILURE;
  }
  if (p.imbalancePct >= IMBALANCE_CRITICAL_PCT) return CRITICAL_IMBALANCE;
  if (p.imbalancePct >= IMBALANCE_MINOR_PCT)    return MINOR_IMBALANCE;
  return HEALTHY;
}

void updateIndicators(HealthState state) {
  digitalWrite(LED_HEALTHY,  state == HEALTHY);
  digitalWrite(LED_WARNING,  state == MINOR_IMBALANCE);
  digitalWrite(LED_CRITICAL, (state == CRITICAL_IMBALANCE || state == PACK_FAILURE));
}

const char* stateLabel(HealthState s) {
  switch (s) {
    case HEALTHY:             return "Healthy";
    case MINOR_IMBALANCE:     return "Minor Imbalance";
    case CRITICAL_IMBALANCE:  return "Critical Imbal.";
    case PACK_FAILURE:        return "PACK FAILURE";
  }
  return "Unknown";
}

void logToSerial(const PackData &p) {
  Serial.print("C1:"); Serial.print(p.cellV[0], 3);
  Serial.print(" C2:"); Serial.print(p.cellV[1], 3);
  Serial.print(" C3:"); Serial.print(p.cellV[2], 3);
  Serial.print(" C4:"); Serial.print(p.cellV[3], 3);
  Serial.print(" | Avg:"); Serial.print(p.packAvg, 3);
  Serial.print(" | Imbalance:"); Serial.print(p.imbalancePct, 2); Serial.print("%");
  Serial.print(" | BattState: "); Serial.print(stateLabel(p.state));
  Serial.print(" | SafetyState: "); Serial.print(safety_getStateLabel());
  Serial.print(" | Reason: "); Serial.print(safety_getReasonLabel());
  Serial.print(" | Relay: "); Serial.print(safety_isRelayEngaged() ? "CLOSED" : "OPEN");
  Serial.print(" | RuntimeMode: "); Serial.println(runtime_getModeLabel());
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(LED_HEALTHY, OUTPUT);
  pinMode(LED_WARNING, OUTPUT);
  pinMode(LED_CRITICAL, OUTPUT);

  safety_init(RELAY_PIN, BUZZER_PIN, NUM_CELLS);
  runtime_init(NUM_CELLS);
  telemetry_init();
  dashboard_init();

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Battery+Safety+HMI");
  lcd.setCursor(0, 1);
  lcd.print("+Runtime Init...");
  delay(1500);
  lcd.clear();

  hmi_init(lcd);
}

void loop() {
  unsigned long now = millis();

  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;

    readCellVoltages(pack);
    computePackStats(pack);
    pack.state = classifyHealth(pack);
    updateIndicators(pack.state);

    safety_update(pack.cellV, now);

    bool safetyFaultActive = (safety_getState() == FAULT);
    runtime_update(now, pack.cellV, NUM_CELLS, safety_isRelayEngaged(), safetyFaultActive);

    if (runtime_forcesShutdown()) {
      safety_forceOpen();
    }

    logToSerial(pack);
    telemetry_update(now, pack.cellV, NUM_CELLS,
                 pack.packAvg, pack.imbalancePct,
                 stateLabel(pack.state), (uint8_t)pack.state,
                 safety_getStateLabel(), (uint8_t)safety_getState(),
                 runtime_getModeLabel(), (uint8_t)runtime_getMode(),
                 safety_isRelayEngaged(),
                 dashboard_getRiskScore(), 
                 dashboard_getRiskLevelLabel(),
                 dashboard_getRecommendation());

    dashboard_update(pack.imbalancePct,
                 (uint8_t)pack.state,
                 (uint8_t)safety_getState(),
                 (uint8_t)runtime_getMode(),
                 runtime_getFaultLogCount(),
                 safety_isRelayEngaged());             
  }

  if (now - lastHmiTick >= HMI_TICK_MS) {
    lastHmiTick = now;
    hmi_update(now,
               pack.packAvg, pack.imbalancePct,
               pack.cellV, NUM_CELLS,
               pack.weakestIdx, pack.strongestIdx,
               stateLabel(pack.state),
               safety_wantsDisplayOverride(),
               safety_getStateLabel(),
               safety_getReasonLabel(),
               safety_isRelayEngaged());
  }
}
