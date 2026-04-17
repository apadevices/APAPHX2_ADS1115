/**
 * 06_RollingAverage — Middle
 * ==========================
 * Demonstrates the rolling average ring buffer filter.
 * Useful for smoothing noisy readings or suppressing occasional spikes
 * without introducing blocking delays.
 *
 * What you will learn:
 *   - setRollingAverage(n) — enable with window size n (2–10)
 *   - clearRollingAverage() — reset ring without losing window setting
 *   - isRollingAverageReady() — false during warm-up (first n readings)
 *   - avg_buffer in PHXConfig — set window per cycle (keep consistent!)
 *   - getLastRawMV() — raw mV before averaging (for comparison)
 *   - Effect of window size on noise reduction
 *
 * Key rule:
 *   Keep avg_buffer consistent between cycles. Changing avg_buffer
 *   between startReading() calls clears the ring. Use setRollingAverage()
 *   once in setup() and set avg_buffer to the same value in your config.
 *
 * Hardware: PHX v2 board, pH probe at 0x49
 */

#include "APAPHX2_ADS1115.h"

ADS1115_PHX_PH phSensor(0x49);

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    Serial.println(F("APAPHX2_ADS1115 — 06 Rolling Average"));
    Serial.println(F("======================================"));

    phSensor.begin();

    if (!phSensor.isCalibrated()) {
        Serial.println(F("WARNING: not calibrated — showing raw mV values"));
    }

    // Enable rolling average with window = 5
    phSensor.setRollingAverage(5);
    phSensor.clearRollingAverage();  // start fresh

    Serial.println(F("Window = 5. First 4 readings = warm-up (ring not full)."));
    Serial.println(F("  #     pH(avg5)   ready?"));
    Serial.println(F("  ───   ─────────  ──────"));
}

bool    cfgReady = false;
PHXConfig cfg;
uint16_t readCount = 0;

void loop() {
    if (!cfgReady) {
        cfg.samples    = 10;
        cfg.delay_ms   = 5;
        cfg.avg_buffer = 5;   // must match setRollingAverage() window
        cfgReady = true;
    }

    phSensor.startReading(cfg);
    while (phSensor.getState() != PHXState::IDLE) {
        phSensor.updateReading();
    }

    readCount++;

    Serial.print(F("  "));
    if (readCount < 10)  Serial.print(F(" "));
    if (readCount < 100) Serial.print(F(" "));
    Serial.print(readCount);
    Serial.print(F("    "));
    Serial.print(phSensor.getLastReading(), 3);
    Serial.print(F("      "));
    Serial.println(phSensor.isRollingAverageReady() ? F("YES") : F("warming"));

    // After 20 readings, demonstrate window change
    if (readCount == 20) {
        Serial.println();
        Serial.println(F("Switching to window = 3 (ring clears on change):"));
        phSensor.setRollingAverage(3);
        cfg.avg_buffer = 3;
        Serial.println(F("  #     pH(avg3)   ready?"));
        Serial.println(F("  ───   ─────────  ──────"));
    }

    delay(1000);
}
