/**
 * 04_Calibration — Middle
 * =======================
 * Complete guided calibration for both pH and ORP sensors.
 * Follow the Serial Monitor prompts step by step.
 *
 * What you will learn:
 *   - setMessageCallback() — receive library progress messages
 *   - calibratePoint1() / calibratePoint2() — guided two-step API
 *   - saveCalibration() — persist to EEPROM (you must call this!)
 *   - loadCalibration() — verify EEPROM round-trip
 *   - getCalibration() — inspect stored reference points
 *   - Electrode slope check — verify probe health
 *
 * What you need:
 *   pH calibration:  pH 4.0 liquid buffer + pH 7.0 liquid buffer
 *   ORP calibration: 475mV liquid reference + 650mV liquid reference
 *   Distilled water for rinsing between solutions
 *
 * IMPORTANT:
 *   Each calibration point takes ~200 seconds minimum.
 *   The probe must fully equilibrate in the buffer before the
 *   library accepts the reading. Do not rush this step.
 *
 * Total calibration time: ~15-20 minutes for both sensors.
 *
 * Hardware: PHX v2 board, both probes, buffer solutions
 */

#include "APAPHX2_ADS1115.h"

ADS1115_PHX_PH phSensor(0x49);
ADS1115_PHX_RX rxSensor(0x48);

// ── Message callback ────────────────────────────────────────
// Library calls this during calibration to report progress.
// You can replace Serial.println(m) with lcd.print(m) for LCD.
void onMessage(const __FlashStringHelper* m) {
    Serial.print(F("  [lib] ")); Serial.println(m);
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
    Serial.begin(115200);
    while (!Serial) { ; }

    Serial.println(F("APAPHX2_ADS1115 — 04 Calibration"));
    Serial.println(F("=================================="));
    Serial.println(F("Follow the prompts. Each point takes ~200 seconds."));
    Serial.println(F("Use LIQUID buffer solutions only — avoid powder types."));
    Serial.println();

    phSensor.begin();
    rxSensor.begin();
    phSensor.setMessageCallback(onMessage);
    rxSensor.setMessageCallback(onMessage);

    // Show current calibration state before we start
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

    // Point 1 — pH 4.0
    waitForEnter("  Rinse pH probe with distilled water.\n"
                 "  Place probe in pH 4.0 buffer solution.");
    Serial.println(F("  Capturing pH 4 reference point..."));

    float ph1mV = phSensor.calibratePoint1(4.0f);
    Serial.print(F("  Captured: ")); Serial.print(ph1mV, 3); Serial.println(F(" mV"));
    if (phSensor.getLastError() == PHXError::CALIB_INVALID) {
        Serial.println(F("  WARNING: probe unstable — result may be inaccurate"));
    }

    // Point 2 — pH 7.0
    waitForEnter("  Rinse pH probe with distilled water.\n"
                 "  Place probe in pH 7.0 buffer solution.");
    Serial.println(F("  Capturing pH 7 reference point..."));

    bool phOk = phSensor.calibratePoint2(7.0f);

    if (phOk) {
        PHX_Calibration cal = phSensor.getCalibration();
        float slope = cal.ref1_mV - cal.ref2_mV;
        if (slope < 0) slope = -slope;
        slope = slope / (7.0f - 4.0f);

        Serial.println(F("  pH calibration VALID:"));
        Serial.print(F("    ref1: ")); Serial.print(cal.ref1_mV, 3);
        Serial.print(F(" mV → pH ")); Serial.println(cal.ref1_value, 1);
        Serial.print(F("    ref2: ")); Serial.print(cal.ref2_mV, 3);
        Serial.print(F(" mV → pH ")); Serial.println(cal.ref2_value, 1);
        Serial.print(F("    Electrode slope: ")); Serial.print(slope, 2);
        Serial.print(F(" mV/pH  (ideal 59.16)  "));
        Serial.println((slope > 40.0f && slope < 70.0f) ? F("GOOD") : F("WARN — probe may need replacement"));

        phSensor.saveCalibration();
        Serial.println(F("  pH calibration saved to EEPROM."));
    } else {
        Serial.println(F("  pH calibration FAILED — buffer points too similar."));
        Serial.println(F("  Verify you used different buffer solutions."));
    }

    Serial.println();

    // ════════════════════════════════════════════════════════
    // ORP CALIBRATION
    // ════════════════════════════════════════════════════════
    printSep();
    Serial.println(F("ORP CALIBRATION — two reference solutions required"));
    printSep();

    // Point 1 — 475mV
    waitForEnter("  Place ORP probe in 475mV reference solution.");
    Serial.println(F("  Capturing 475mV reference point..."));

    float rx1mV = rxSensor.calibratePoint1(475.0f);
    Serial.print(F("  Captured: ")); Serial.print(rx1mV, 3); Serial.println(F(" mV"));

    // Point 2 — 650mV
    waitForEnter("  Place ORP probe in 650mV reference solution.");
    Serial.println(F("  Capturing 650mV reference point..."));

    bool rxOk = rxSensor.calibratePoint2(650.0f);

    if (rxOk) {
        PHX_Calibration cal = rxSensor.getCalibration();
        Serial.println(F("  ORP calibration VALID:"));
        Serial.print(F("    ref1: ")); Serial.print(cal.ref1_mV, 3);
        Serial.print(F(" mV → ")); Serial.print(cal.ref1_value, 0); Serial.println(F(" mV"));
        Serial.print(F("    ref2: ")); Serial.print(cal.ref2_mV, 3);
        Serial.print(F(" mV → ")); Serial.print(cal.ref2_value, 0); Serial.println(F(" mV"));

        rxSensor.saveCalibration();
        Serial.println(F("  ORP calibration saved to EEPROM."));
    } else {
        Serial.println(F("  ORP calibration FAILED — reference points too similar."));
    }

    // ════════════════════════════════════════════════════════
    // VERIFY — reload from EEPROM and confirm
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
    Serial.println(F("Calibration complete. Reset board to start measuring."));
    Serial.println(F("Calibration is auto-loaded by begin() on every boot."));
}

void loop() {
    // Nothing here — this sketch is setup-only
}
