/**
 * 03_DualSensor — Simple
 * ======================
 * Runs both pH and ORP sensors simultaneously using the non-blocking
 * state machine. Both sensors cycle independently — when one finishes
 * it restarts immediately without waiting for the other.
 *
 * What you will learn:
 *   - Running two sensors in parallel without blocking
 *   - The correct dual-sensor loop pattern
 *   - Using done flags to print results once per cycle
 *   - Reading both calibration states on startup
 *
 * Hardware: PHX v2 board, both probes connected
 *           pH at 0x49, ORP at 0x48
 */

#include "APAPHX2_ADS1115.h"

ADS1115_PHX_PH phSensor(0x49);
ADS1115_PHX_RX rxSensor(0x48);

// Message callback — shows library calibration messages on Serial
void onMessage(const __FlashStringHelper* m) { Serial.println(m); }

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    Serial.println(F("APAPHX2_ADS1115 — 03 Dual Sensor"));
    Serial.println(F("=================================="));

    phSensor.begin();
    rxSensor.begin();

    phSensor.setMessageCallback(onMessage);
    rxSensor.setMessageCallback(onMessage);

    Serial.print(F("pH  calibrated: ")); Serial.println(phSensor.isCalibrated() ? F("YES") : F("NO"));
    Serial.print(F("ORP calibrated: ")); Serial.println(rxSensor.isCalibrated() ? F("YES") : F("NO"));
    Serial.println();
    Serial.println(F("  pH      ORP(mV)"));
    Serial.println(F("  ──────  ────────"));
}

// Measurement config — shared between both sensors
bool      cfgReady  = false;
PHXConfig cfg;

// State tracking — each sensor independent
bool phRunning = false;
bool rxRunning = false;
bool phDone    = false;
bool rxDone    = false;

void loop() {
    // Initialise config on first call
    if (!cfgReady) {
        cfg.samples    = 10;
        cfg.delay_ms   = 5;
        cfg.avg_buffer = 1;
        cfgReady = true;
    }

    // ── pH sensor ──────────────────────────────────────────
    if (!phRunning) {
        phSensor.startReading(cfg);
        phRunning = true;
        phDone    = false;
    }
    phSensor.updateReading();
    if (phRunning && phSensor.getState() == PHXState::IDLE && !phDone) {
        phRunning = false;
        phDone    = true;
    }

    // ── ORP sensor ─────────────────────────────────────────
    if (!rxRunning) {
        rxSensor.startReading(cfg);
        rxRunning = true;
        rxDone    = false;
    }
    rxSensor.updateReading();
    if (rxRunning && rxSensor.getState() == PHXState::IDLE && !rxDone) {
        rxRunning = false;
        rxDone    = true;
    }

    // ── Print when both sensors completed this round ───────
    if (phDone && rxDone) {
        Serial.print(F("  "));
        Serial.print(phSensor.getLastReading(), 3);
        Serial.print(F("   "));
        Serial.println(rxSensor.getLastReading(), 1);
        phDone = false;
        rxDone = false;
    }
}
