/**
 * 10_TickCallback — Advanced
 * ===========================
 * Same guided calibration as 04_Calibration, plus setTickCallback() wired
 * to a visible heartbeat (onboard LED + one dot per tick on Serial) —
 * demonstrates the tick hook added in 1.2.0 for feeding an external
 * watchdog (or any other periodic housekeeping) during calibration's
 * otherwise fully-silent, fully-blocking waits.
 *
 * What you will learn:
 *   - setTickCallback() — periodic heartbeat during the 200s soak and the
 *     stability-check loop, roughly once per second / once per round
 *   - Confirms the tick does not change calibration timing, samples, or
 *     the stability algorithm — same prompts, same ~200s wait, same result
 *   - A real integration would call a watchdog-feed function here instead
 *     of blinking an LED — see README.md "Tick callback" section
 *
 * What you need: same as 04_Calibration — PHX v2 board, both probes,
 * pH 4.0 / pH 7.0 buffers, 475mV / 650mV ORP references.
 *
 * What to watch for: the onboard LED should blink and a "." should print
 * roughly once per second during "Capturing ... reference point..." —
 * without this callback, that phase is completely silent for ~200+ seconds.
 *
 * Hardware: PHX v2 board, both probes, buffer solutions
 */

#include "APAPHX2_ADS1115.h"

ADS1115_PHX_PH phSensor(0x49);
ADS1115_PHX_RX rxSensor(0x48);

// ── Message callback ────────────────────────────────────────
// Library calls this during calibration to report progress.
void onMessage(const __FlashStringHelper* m) {
    Serial.print(F("  [lib] ")); Serial.println(m);
}

// ── Tick callback — NEW, this is what we're validating ──────
// Fires roughly once per second during the soak, once per round during
// the stability loop. Blink + dot are purely for visibility during the
// test — a real integration would call wdtUpdate() here instead.
void onTick() {
    static bool led = false;
    led = !led;
    digitalWrite(LED_BUILTIN, led);
    Serial.print(F("."));
}

// ── Utility ─────────────────────────────────────────────────
void waitForEnter(const char* prompt) {
    Serial.println(prompt);
    Serial.println(F("  Press ENTER when ready..."));
    while (Serial.available()) Serial.read();
    while (!Serial.available()) { ; }
    while (Serial.available()) Serial.read();
}

void printSep() { Serial.println(F("──────────────────────────────────────")); }

void setup() {
    Serial.begin(9600);
    while (!Serial) { ; }

    pinMode(LED_BUILTIN, OUTPUT);

    Serial.println(F("APAPHX2_ADS1115 — 10 TickCallback"));
    Serial.println(F("=================================="));
    Serial.println(F("Follow the prompts. Each point takes ~200 seconds."));
    Serial.println(F("Watch for the onboard LED blinking + '.' printing during"));
    Serial.println(F("the wait — that phase used to be completely silent."));
    Serial.println();

    phSensor.begin();
    rxSensor.begin();
    phSensor.setMessageCallback(onMessage);
    rxSensor.setMessageCallback(onMessage);
    phSensor.setTickCallback(onTick);
    rxSensor.setTickCallback(onTick);

    Serial.print(F("pH  currently calibrated: "));
    Serial.println(phSensor.isCalibrated() ? F("YES (will be overwritten)") : F("NO"));
    Serial.print(F("ORP currently calibrated: "));
    Serial.println(rxSensor.isCalibrated() ? F("YES (will be overwritten)") : F("NO"));
    Serial.println();

    // ════════════════════════════════════════════════════════
    // pH CALIBRATION
    // ════════════════════════════════════════════════════════
    printSep();
    Serial.println(F("pH CALIBRATION — two buffer solutions required"));
    printSep();

    waitForEnter("  Rinse pH probe with distilled water.\n"
                 "  Place probe in pH 4.0 buffer solution.");
    Serial.println(F("  Capturing pH 4 reference point..."));

    float ph1mV = phSensor.calibratePoint1(4.0f);
    Serial.println();
    Serial.print(F("  Captured: ")); Serial.print(ph1mV, 3); Serial.println(F(" mV"));
    if (phSensor.getLastError() == PHXError::CALIB_INVALID) {
        Serial.println(F("  WARNING: probe unstable — result may be inaccurate"));
    }

    waitForEnter("  Rinse pH probe with distilled water.\n"
                 "  Place probe in pH 7.0 buffer solution.");
    Serial.println(F("  Capturing pH 7 reference point..."));

    bool phOk = phSensor.calibratePoint2(7.0f);
    Serial.println();

    if (phOk) {
        PHX_Calibration cal = phSensor.getCalibration();
        float slope = cal.ref1_mV - cal.ref2_mV;
        if (slope < 0) slope = -slope;
        slope = slope / (7.0f - 4.0f);

        Serial.println(F("  pH calibration VALID:"));
        Serial.print(F("    ref1: ")); Serial.print(cal.ref1_mV, 3);
        Serial.print(F(" mV -> pH ")); Serial.println(cal.ref1_value, 1);
        Serial.print(F("    ref2: ")); Serial.print(cal.ref2_mV, 3);
        Serial.print(F(" mV -> pH ")); Serial.println(cal.ref2_value, 1);
        Serial.print(F("    Electrode slope: ")); Serial.print(slope, 2);
        Serial.print(F(" mV/pH  (ideal 59.16)  "));
        Serial.println((slope > 40.0f && slope < 70.0f) ? F("GOOD") : F("WARN - probe may need replacement"));

        phSensor.saveCalibration();
        Serial.println(F("  pH calibration saved to EEPROM."));
    } else {
        Serial.println(F("  pH calibration FAILED - buffer points too similar."));
    }

    Serial.println();

    // ════════════════════════════════════════════════════════
    // ORP CALIBRATION
    // ════════════════════════════════════════════════════════
    printSep();
    Serial.println(F("ORP CALIBRATION — two reference solutions required"));
    printSep();

    waitForEnter("  Place ORP probe in 475mV reference solution.");
    Serial.println(F("  Capturing 475mV reference point..."));

    float rx1mV = rxSensor.calibratePoint1(475.0f);
    Serial.println();
    Serial.print(F("  Captured: ")); Serial.print(rx1mV, 3); Serial.println(F(" mV"));

    waitForEnter("  Place ORP probe in 650mV reference solution.");
    Serial.println(F("  Capturing 650mV reference point..."));

    bool rxOk = rxSensor.calibratePoint2(650.0f);
    Serial.println();

    if (rxOk) {
        PHX_Calibration cal = rxSensor.getCalibration();
        Serial.println(F("  ORP calibration VALID:"));
        Serial.print(F("    ref1: ")); Serial.print(cal.ref1_mV, 3);
        Serial.print(F(" mV -> ")); Serial.print(cal.ref1_value, 0); Serial.println(F(" mV"));
        Serial.print(F("    ref2: ")); Serial.print(cal.ref2_mV, 3);
        Serial.print(F(" mV -> ")); Serial.print(cal.ref2_value, 0); Serial.println(F(" mV"));

        rxSensor.saveCalibration();
        Serial.println(F("  ORP calibration saved to EEPROM."));
    } else {
        Serial.println(F("  ORP calibration FAILED - reference points too similar."));
    }

    // ════════════════════════════════════════════════════════
    // VERIFY
    // ════════════════════════════════════════════════════════
    Serial.println();
    printSep();
    Serial.println(F("VERIFICATION — reloading from EEPROM"));
    printSep();

    bool phLoaded = phSensor.loadCalibration();
    bool rxLoaded = rxSensor.loadCalibration();

    Serial.print(F("pH  EEPROM load: ")); Serial.println(phLoaded ? F("OK") : F("FAILED"));
    Serial.print(F("ORP EEPROM load: ")); Serial.println(rxLoaded ? F("OK") : F("FAILED"));

    Serial.println();
    Serial.println(F("Tick test complete. If you saw the LED blink and dots"));
    Serial.println(F("print during each 'Capturing...' wait, the tick hook works."));
}

void loop() {
    // Nothing here — this sketch is setup-only
}
