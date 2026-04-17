/**
 * 09_PoolMonitor — Advanced
 * =========================
 * Complete pool water quality monitoring sketch.
 * Demonstrates how all library features work together in a realistic
 * application — the intended end use of the PHX v2 board.
 *
 * Features demonstrated:
 *   - Dual sensor non-blocking loop (pH + ORP)
 *   - Rolling average window = 5 for stable display values
 *   - Temperature compensation for pH
 *   - Message callback (Serial — swap for LCD in real product)
 *   - Alarm thresholds with hysteresis
 *   - Periodic Serial reporting (every 10s)
 *   - Error detection and reporting
 *   - Calibration check on startup
 *
 * Pool water chemistry reference ranges:
 *   pH:   7.2 – 7.6   (ideal 7.4)
 *   ORP:  650 – 750 mV (sanitised pool, chlorinated)
 *
 * Alarm thresholds (adjustable below):
 *   pH low alarm:   < 7.0
 *   pH high alarm:  > 7.8
 *   ORP low alarm:  < 600 mV  (insufficient sanitation)
 *   ORP high alarm: > 800 mV  (over-sanitised)
 *
 * Hardware: PHX v2 board
 *           pH probe at 0x49
 *           ORP probe at 0x48
 *           Temperature sensor (read externally — set via WATER_TEMP_C)
 *           Optional: alarm buzzer or LED on ALARM_PIN
 */

#include "APAPHX2_ADS1115.h"

// ── Hardware ─────────────────────────────────────────────────
ADS1115_PHX_PH phSensor(0x49);
ADS1115_PHX_RX rxSensor(0x48);

const int ALARM_PIN = LED_BUILTIN;   // change to buzzer pin if needed

// ── Water temperature ─────────────────────────────────────────
// Update this value from your temperature sensor regularly
// Or replace with a DS18B20 / NTC read in loop()
float WATER_TEMP_C = 25.0f;

// ── Alarm thresholds ──────────────────────────────────────────
const float PH_LOW_ALARM   = 7.0f;
const float PH_HIGH_ALARM  = 7.8f;
const float ORP_LOW_ALARM  = 600.0f;
const float ORP_HIGH_ALARM = 800.0f;

// ── Report interval ───────────────────────────────────────────
const unsigned long REPORT_INTERVAL_MS = 10000UL;   // 10 seconds

// ── Message callback ──────────────────────────────────────────
void onMessage(const __FlashStringHelper* m) {
    Serial.print(F("[PHX] ")); Serial.println(m);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }
    pinMode(ALARM_PIN, OUTPUT);
    digitalWrite(ALARM_PIN, LOW);

    Serial.println(F("============================================"));
    Serial.println(F("  PHX v2 Pool Monitor"));
    Serial.println(F("============================================"));

    phSensor.begin();
    rxSensor.begin();
    phSensor.setMessageCallback(onMessage);
    rxSensor.setMessageCallback(onMessage);

    // ── Calibration check ─────────────────────────────────
    bool phCal = phSensor.isCalibrated();
    bool rxCal = rxSensor.isCalibrated();

    Serial.print(F("pH  calibrated: ")); Serial.println(phCal ? F("YES") : F("NO — run 04_Calibration"));
    Serial.print(F("ORP calibrated: ")); Serial.println(rxCal ? F("YES") : F("NO — run 04_Calibration"));

    if (!phCal || !rxCal) {
        Serial.println(F("WARNING: uncalibrated sensor — readings unreliable."));
        Serial.println(F("Continuing in raw mV mode for diagnostics."));
    } else {
        // Print loaded calibration for verification
        PHX_Calibration phc = phSensor.getCalibration();
        Serial.print(F("pH  cal slope: "));
        float slope = phc.ref1_mV - phc.ref2_mV;
        if (slope < 0) slope = -slope;
        slope /= (phc.ref2_value - phc.ref1_value);
        Serial.print(slope, 2);
        Serial.println(F(" mV/pH"));
    }

    // ── Temperature compensation ──────────────────────────
    phSensor.enableTemperatureCompensation(true);
    phSensor.setTemperature(WATER_TEMP_C);
    Serial.print(F("Temp compensation: enabled at "));
    Serial.print(WATER_TEMP_C, 1); Serial.println(F("°C"));

    // ── Rolling average ───────────────────────────────────
    phSensor.setRollingAverage(5);
    rxSensor.setRollingAverage(5);
    Serial.println(F("Rolling average: window = 5"));

    Serial.println();
    Serial.println(F("Monitoring started. Report every 10 seconds."));
    Serial.println(F("  Time(s)  pH       ORP(mV)  Status"));
    Serial.println(F("  ───────  ───────  ───────  ──────────────"));
}

// ── Sensor state ──────────────────────────────────────────────
PHXConfig cfg;
bool cfgReady  = false;
bool phRunning = false;
bool rxRunning = false;
bool phDone    = false;
bool rxDone    = false;

// ── Reporting ─────────────────────────────────────────────────
unsigned long lastReport  = 0;
float         lastPH      = 0.0f;
float         lastORP     = 0.0f;
bool          alarmActive = false;

void loop() {
    // ── Initialise config once ──────────────────────────────
    if (!cfgReady) {
        cfg.samples    = 10;
        cfg.delay_ms   = 5;
        cfg.avg_buffer = 5;
        cfgReady = true;
    }

    // ── pH sensor cycle ────────────────────────────────────
    if (!phRunning) {
        phSensor.startReading(cfg);
        phRunning = true; phDone = false;
    }
    phSensor.updateReading();
    if (phRunning && phSensor.getState() == PHXState::IDLE && !phDone) {
        phRunning = false; phDone = true;
        lastPH = phSensor.getLastReading();
        // Update temperature compensation regularly
        phSensor.setTemperature(WATER_TEMP_C);
    }

    // ── ORP sensor cycle ───────────────────────────────────
    if (!rxRunning) {
        rxSensor.startReading(cfg);
        rxRunning = true; rxDone = false;
    }
    rxSensor.updateReading();
    if (rxRunning && rxSensor.getState() == PHXState::IDLE && !rxDone) {
        rxRunning = false; rxDone = true;
        lastORP = rxSensor.getLastReading();
    }

    // ── Alarm check ────────────────────────────────────────
    bool alarm = false;
    if (phSensor.isCalibrated()) {
        if (lastPH  < PH_LOW_ALARM  || lastPH  > PH_HIGH_ALARM)  alarm = true;
        if (lastORP < ORP_LOW_ALARM || lastORP > ORP_HIGH_ALARM)  alarm = true;
    }
    // Check library error flags
    if (phSensor.getLastError() != PHXError::NONE) alarm = true;
    if (rxSensor.getLastError() != PHXError::NONE) alarm = true;

    alarmActive = alarm;
    digitalWrite(ALARM_PIN, alarm ? HIGH : LOW);

    // ── Periodic report ────────────────────────────────────
    if (phDone && rxDone && (millis() - lastReport >= REPORT_INTERVAL_MS)) {
        lastReport = millis();

        // Time
        Serial.print(F("  "));
        unsigned long secs = millis() / 1000;
        if (secs < 100) Serial.print(F(" "));
        if (secs < 10)  Serial.print(F(" "));
        Serial.print(secs);
        Serial.print(F("      "));

        // pH
        Serial.print(lastPH, 3);
        Serial.print(F("    "));

        // ORP
        if (lastORP < 1000) Serial.print(F(" "));
        Serial.print(lastORP, 1);
        Serial.print(F("    "));

        // Status
        if (!phSensor.isCalibrated() || !rxSensor.isCalibrated()) {
            Serial.print(F("UNCALIBRATED"));
        } else if (alarm) {
            Serial.print(F("ALARM: "));
            if (lastPH  < PH_LOW_ALARM)   Serial.print(F("pH LOW "));
            if (lastPH  > PH_HIGH_ALARM)  Serial.print(F("pH HIGH "));
            if (lastORP < ORP_LOW_ALARM)  Serial.print(F("ORP LOW "));
            if (lastORP > ORP_HIGH_ALARM) Serial.print(F("ORP HIGH "));
        } else if (!phSensor.isRollingAverageReady() ||
                   !rxSensor.isRollingAverageReady()) {
            Serial.print(F("warming up..."));
        } else {
            Serial.print(F("OK"));
        }
        Serial.println();

        phDone = false;
        rxDone = false;
    }
}
