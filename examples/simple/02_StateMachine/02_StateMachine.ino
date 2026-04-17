/**
 * 02_StateMachine — Simple
 * ========================
 * Demonstrates the non-blocking state machine — the correct way to use
 * this library in any real application. The MCU never blocks waiting for
 * conversions — it calls updateReading() in loop() and the library
 * advances one step each call.
 *
 * What you will learn:
 *   - PHXConfig — samples, delay_ms, avg_buffer
 *   - startReading() — begin a measurement cycle
 *   - updateReading() — advance the state machine (call in loop)
 *   - PHXState — IDLE, COLLECTING, PROCESSING
 *   - isReadingComplete() — poll for completion
 *   - getLastReading() — retrieve result
 *   - How other code can run concurrently during measurement
 *
 * Key concept:
 *   The library state machine runs alongside your other code.
 *   You do not wait for it — you check it each loop iteration.
 *
 * Hardware: PHX v2 board, pH probe at 0x49
 */

#include "APAPHX2_ADS1115.h"

ADS1115_PHX_PH phSensor(0x49);

// Measurement configuration
// 10 samples, 5ms between each → total ~130ms per cycle
PHXConfig cfg;
bool cfgReady = false;

// Timing
unsigned long lastResult = 0;
const unsigned long PRINT_INTERVAL = 2000;   // print every 2s

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    Serial.println(F("APAPHX2_ADS1115 — 02 State Machine"));
    Serial.println(F("===================================="));

    phSensor.begin();

    if (!phSensor.isCalibrated()) {
        Serial.println(F("WARNING: not calibrated — showing raw mV"));
    }

    Serial.println(F("State machine running — MCU free between updates"));
    Serial.println();
}

void loop() {
    // ── Initialise config once ─────────────────────────────
    if (!cfgReady) {
        cfg.samples    = 10;
        cfg.delay_ms   = 5;
        cfg.avg_buffer = 1;
        cfgReady = true;
    }

    // ── Start a new cycle when sensor is idle ──────────────
    if (phSensor.getState() == PHXState::IDLE) {
        phSensor.startReading(cfg);
    }

    // ── Advance state machine — one step per loop call ─────
    // This returns immediately — no blocking, no delay
    phSensor.updateReading();

    // ── Act on completed reading ───────────────────────────
    if (phSensor.isReadingComplete()) {
        // Reading complete — result available
        if (millis() - lastResult >= PRINT_INTERVAL) {
            lastResult = millis();

            Serial.print(F("pH: "));
            Serial.print(phSensor.getLastReading(), 3);
            Serial.print(F("  state: "));
            switch (phSensor.getState()) {
                case PHXState::IDLE:       Serial.print(F("IDLE"));       break;
                case PHXState::COLLECTING: Serial.print(F("COLLECTING")); break;
                case PHXState::PROCESSING: Serial.print(F("PROCESSING")); break;
            }
            Serial.print(F("  error: "));
            if (phSensor.getLastError() == PHXError::NONE) {
                Serial.println(F("none"));
            } else {
                Serial.println(F("!"));
            }
        }
    }

    // ── Other code runs here concurrently ──────────────────
    // Button reads, display updates, network, sensors, etc.
    // The state machine does not block any of this.
    doOtherWork();
}

// Simulated concurrent workload — replace with your actual code
void doOtherWork() {
    // Example: blink an LED, read a button, update a display...
    // This runs every loop iteration regardless of sensor state
}
