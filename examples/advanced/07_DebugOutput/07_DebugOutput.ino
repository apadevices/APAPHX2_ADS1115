/**
 * 07_DebugOutput — Advanced
 * =========================
 * Shows how to use the library debug system during development.
 * Debug output gives full visibility into every library operation —
 * register writes, sample values, calibration windows, EEPROM and more.
 *
 * What you will learn:
 *   - How to enable ADS1115_DEBUG at compile time
 *   - enableDebug(true/false) — runtime toggle
 *   - setDebugStream() — redirect to any Stream (Serial1, SoftwareSerial...)
 *   - What debug output looks like for each operation
 *   - How to use debug to diagnose calibration or reading problems
 *
 * STEP 1 — Enable debug output:
 *   Open APAPHX2_ADS1115.h and uncomment this line near the top:
 *   // #define ADS1115_DEBUG   <-- remove the // to enable
 *
 * STEP 2 — Flash this sketch and watch Serial Monitor at 115200 baud.
 *
 * STEP 3 — When done developing, comment the define out again
 *   to remove all debug overhead from production firmware.
 *
 * Note: Debug output is Stream-based (Serial, UART).
 *       It is NOT suitable for LCD — use setMessageCallback() for LCD.
 *
 * Hardware: PHX v2 board, pH probe at 0x49
 */

// ADS1115_DEBUG must be defined in APAPHX2_ADS1115.h — see STEP 1 above
#include "APAPHX2_ADS1115.h"

ADS1115_PHX_PH phSensor(0x49);

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    Serial.println(F("APAPHX2_ADS1115 — 07 Debug Output"));
    Serial.println(F("==================================="));

    #ifndef ADS1115_DEBUG
    Serial.println(F("WARNING: ADS1115_DEBUG is not defined."));
    Serial.println(F("Open APAPHX2_ADS1115.h and uncomment:"));
    Serial.println(F("  #define ADS1115_DEBUG"));
    Serial.println(F("Then re-compile and flash."));
    Serial.println();
    #endif

    // Enable debug output to Serial (default stream)
    phSensor.enableDebug(true);
    phSensor.setDebugStream(Serial);   // explicit, but Serial is already default

    Serial.println(F("── begin() ──────────────────────────────────────"));
    phSensor.begin();   // debug shows: type, address, gain, dataRate, cal load

    Serial.println();
    Serial.println(F("── setGain() valid and invalid ──────────────────"));
    phSensor.setGain(ADS1115_GAIN_2);    // valid — debug shows new value
    phSensor.setGain(0xFFFF);            // invalid — debug shows "ignored"
    phSensor.setGain(ADS1115_GAIN_2);    // restore

    Serial.println();
    Serial.println(F("── readADC() — raw, mV, poll count ──────────────"));
    phSensor.readADC();   // debug shows: raw count, mV, OS-bit polls

    Serial.println();
    Serial.println(F("── startReading() + updateReading() ─────────────"));
    Serial.println(F("   (watch sample[0]..sample[4] and process output)"));
    PHXConfig cfg;
    cfg.samples    = 5;
    cfg.delay_ms   = 0;
    cfg.avg_buffer = 1;
    phSensor.startReading(cfg);
    while (phSensor.getState() != PHXState::IDLE) {
        phSensor.updateReading();
    }

    Serial.println();
    Serial.println(F("── cancelReading() ──────────────────────────────"));
    cfg.samples = 25; cfg.delay_ms = 50;
    phSensor.startReading(cfg);
    delay(80);
    phSensor.cancelReading();  // debug shows: cancelReading

    Serial.println();
    Serial.println(F("── Temperature compensation ──────────────────────"));
    phSensor.enableTemperatureCompensation(true);   // debug: tempComp: enabled
    phSensor.setTemperature(25.0f);                  // debug: setTemperature: 25.00
    phSensor.setTemperature(99.0f);                  // debug: setTemperature: INVALID
    phSensor.setTemperature(25.0f);
    cfg.samples = 3; cfg.delay_ms = 0;
    phSensor.startReading(cfg);
    while (phSensor.getState() != PHXState::IDLE) { phSensor.updateReading(); }
    phSensor.enableTemperatureCompensation(false);

    Serial.println();
    Serial.println(F("── Rolling average ring fill ─────────────────────"));
    phSensor.setRollingAverage(3);    // debug: setRollingAverage: window=3
    phSensor.clearRollingAverage();   // debug: clearRollingAverage
    cfg.avg_buffer = 3;
    for (uint8_t i = 0; i < 4; i++) {
        phSensor.startReading(cfg);
        while (phSensor.getState() != PHXState::IDLE) { phSensor.updateReading(); }
        // debug shows: avgRing: filled=1, 2, 3, 3 (capped at window)
    }

    Serial.println();
    Serial.println(F("── enableDebug(false) — silent mode ──────────────"));
    phSensor.enableDebug(false);
    phSensor.readADC();   // no debug output
    Serial.println(F("   (no [ADS1115] lines above = debug disabled correctly)"));

    phSensor.enableDebug(true);  // restore for any subsequent use
    Serial.println();
    Serial.println(F("Debug demo complete. Check output above."));
    Serial.println(F("Remember to comment out #define ADS1115_DEBUG before production."));
}

void loop() { }
