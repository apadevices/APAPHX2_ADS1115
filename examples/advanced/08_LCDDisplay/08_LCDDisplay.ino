/**
 * 08_LCDDisplay — Advanced
 * ========================
 * Demonstrates setMessageCallback() to route library calibration messages
 * to an LCD 20×4 display instead of (or alongside) Serial.
 *
 * The library emits three short messages (≤20 chars, LCD row compatible):
 *   "Cal: wait 200s..."    — probe soak started
 *   "Calibration: stable!" — stable reading captured
 *   "Cal: timeout-check! " — stability timeout
 *
 * All messages fit one row of a 20×4 LCD without truncation.
 *
 * What you will learn:
 *   - setMessageCallback() — register output handler
 *   - Callback signature: void myFunc(const __FlashStringHelper*)
 *   - Routing messages to LCD (LiquidCrystal_I2C example)
 *   - Routing messages to Serial
 *   - Why user interaction prompts are sketch responsibility, not library
 *
 * Hardware: PHX v2 board + LCD 20×4 with I2C backpack
 *           Common LCD I2C address: 0x27 or 0x3F
 *           pH probe at 0x49
 *
 * Library required: LiquidCrystal_I2C (install via Library Manager)
 */

#include "APAPHX2_ADS1115.h"
#include <LiquidCrystal_I2C.h>

ADS1115_PHX_PH phSensor(0x49);

// LCD: address 0x27, 20 columns, 4 rows
// Change 0x27 to 0x3F if your LCD does not initialise
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ── Message callback — LCD version ──────────────────────────
// Library calls this with calibration progress messages.
// Message appears on row 3 (bottom row) of the LCD.
// All library messages are ≤20 chars — no truncation needed.
void onMessage(const __FlashStringHelper* m) {
    lcd.setCursor(0, 3);
    lcd.print(F("                    "));   // clear row
    lcd.setCursor(0, 3);
    lcd.print(m);                            // print message

    // Also echo to Serial for monitoring
    Serial.print(F("[lcd] ")); Serial.println(m);
}

// ── Helper: show instruction on LCD ──────────────────────────
void lcdInstruction(const char* line0, const char* line1 = "",
                    const char* line2 = "") {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(line0);
    lcd.setCursor(0, 1); lcd.print(line1);
    lcd.setCursor(0, 2); lcd.print(line2);
    // Row 3 reserved for library messages
}

// ── Helper: wait for button or Serial ENTER ───────────────────
// In a real product replace with your button/touchscreen logic
void waitForEnter() {
    Serial.println(F("  Press ENTER on Serial Monitor to continue..."));
    while (Serial.available()) Serial.read();
    while (!Serial.available()) { ; }
    while (Serial.available()) Serial.read();
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    // Initialise LCD
    lcd.init();
    lcd.backlight();
    lcd.clear();

    Serial.println(F("APAPHX2_ADS1115 — 08 LCD Display"));
    Serial.println(F("=================================="));

    phSensor.begin();

    // Register callback — library messages → LCD row 3 + Serial
    phSensor.setMessageCallback(onMessage);

    // ── Calibration with LCD prompts ───────────────────────
    lcdInstruction("pH Calibration", "Place in pH 4.0", "Press ENTER...");
    Serial.println(F("Place pH probe in pH 4.0 buffer."));
    waitForEnter();

    lcdInstruction("pH Calibration", "Capturing pH 4.0", "Please wait...");
    // Library messages appear on LCD row 3 automatically via callback
    float mV1 = phSensor.calibratePoint1(4.0f);

    // Show result on LCD
    lcd.setCursor(0, 2);
    lcd.print(F("Captured: "));
    lcd.print(mV1, 1);
    lcd.print(F(" mV    "));
    Serial.print(F("pH 4 captured: ")); Serial.print(mV1, 3); Serial.println(F(" mV"));
    delay(2000);

    lcdInstruction("pH Calibration", "Place in pH 7.0", "Press ENTER...");
    Serial.println(F("Rinse probe. Place in pH 7.0 buffer."));
    waitForEnter();

    lcdInstruction("pH Calibration", "Capturing pH 7.0", "Please wait...");
    bool ok = phSensor.calibratePoint2(7.0f);

    if (ok) {
        phSensor.saveCalibration();
        lcdInstruction("Calibration OK!", "Saved to EEPROM.", "Starting readings");
        Serial.println(F("pH calibration saved."));
    } else {
        lcdInstruction("Cal FAILED", "Check solutions.", "Rerun sketch.");
        Serial.println(F("Calibration failed."));
        while (true) { ; }
    }

    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(F("PHX v2 Monitor"));
    lcd.setCursor(0, 1); lcd.print(F("pH:"));
    lcd.setCursor(0, 2); lcd.print(F("ORP:"));
    Serial.println(F("Live readings starting..."));
}

bool    cfgReady = false;
PHXConfig cfg;

void loop() {
    if (!cfgReady) {
        cfg.samples    = 10;
        cfg.delay_ms   = 5;
        cfg.avg_buffer = 5;
        cfgReady = true;
        phSensor.setRollingAverage(5);
    }

    phSensor.startReading(cfg);
    while (phSensor.getState() != PHXState::IDLE) {
        phSensor.updateReading();
    }

    float ph = phSensor.getLastReading();

    // Update LCD
    lcd.setCursor(4, 1);
    lcd.print(ph, 3);
    lcd.print(F("     "));   // clear trailing chars

    // Update Serial
    Serial.print(F("pH: ")); Serial.println(ph, 3);

    delay(2000);
}
