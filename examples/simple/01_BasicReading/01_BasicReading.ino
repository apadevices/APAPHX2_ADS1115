/**
 * 01_BasicReading — Simple
 * ========================
 * The simplest possible use of the library.
 * Demonstrates direct blocking ADC reads and single-cycle state machine.
 *
 * What you will learn:
 *   - How to create sensor instances
 *   - How begin() auto-loads calibration from EEPROM
 *   - How to check calibration status
 *   - readADC() — direct blocking single conversion
 *   - getLastReading() — returns calibrated pH or ORP mV
 *
 * Hardware: PHX v2 board connected to MCU via P1 connector
 *           pH probe at 0x49, ORP probe at 0x48
 *           Both probes can be in preserve liquid for this example
 *
 * Expected output (if calibrated):
 *   pH: 7.002   ORP: 524.6 mV
 *
 * Expected output (if not calibrated):
 *   NOT CALIBRATED — run example 04_Calibration first
 *   pH raw: -274.5 mV   ORP raw: -513.8 mV
 */

#include "APAPHX2_ADS1115.h"

// Sensor instances — address matches PHX v2 board
ADS1115_PHX_PH phSensor(0x49);   // pH  — I2C 0x49, Gain 2 (±2.048V)
ADS1115_PHX_RX rxSensor(0x48);   // ORP — I2C 0x48, Gain 1 (±4.096V)

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }   // wait for Serial (Leonardo / native USB boards)

    Serial.println(F("APAPHX2_ADS1115 — 01 Basic Reading"));
    Serial.println(F("==================================="));

    // begin() initialises I2C and auto-loads calibration from EEPROM
    phSensor.begin();
    rxSensor.begin();

    // Check calibration status
    Serial.print(F("pH  calibrated: "));
    Serial.println(phSensor.isCalibrated() ? F("YES") : F("NO — run 04_Calibration"));
    Serial.print(F("ORP calibrated: "));
    Serial.println(rxSensor.isCalibrated() ? F("YES") : F("NO — run 04_Calibration"));
    Serial.println();

    // Show calibration reference points if available
    if (phSensor.isCalibrated()) {
        PHX_Calibration cal = phSensor.getCalibration();
        Serial.print(F("pH  cal: ref1="));
        Serial.print(cal.ref1_mV, 2); Serial.print(F("mV→"));
        Serial.print(cal.ref1_value, 1); Serial.print(F("  ref2="));
        Serial.print(cal.ref2_mV, 2); Serial.print(F("mV→"));
        Serial.println(cal.ref2_value, 1);
    }
    if (rxSensor.isCalibrated()) {
        PHX_Calibration cal = rxSensor.getCalibration();
        Serial.print(F("ORP cal: ref1="));
        Serial.print(cal.ref1_mV, 2); Serial.print(F("mV→"));
        Serial.print(cal.ref1_value, 0); Serial.print(F("mV  ref2="));
        Serial.print(cal.ref2_mV, 2); Serial.print(F("mV→"));
        Serial.print(cal.ref2_value, 0); Serial.println(F("mV"));
    }
    Serial.println();

    // Direct blocking read — readADC() returns raw 16-bit signed count
    Serial.println(F("Direct ADC reads (raw counts):"));
    Serial.print(F("  pH  raw count: ")); Serial.println(phSensor.readADC());
    Serial.print(F("  ORP raw count: ")); Serial.println(rxSensor.readADC());
    Serial.println();

    Serial.println(F("Reading every 2 seconds..."));
    Serial.println(F("pH       ORP(mV)"));
}

void loop() {
    // Run a simple 10-sample blocking cycle on each sensor
    // This is the simplest form — suitable for slow polling applications
    PHXConfig cfg;
    cfg.samples    = 10;
    cfg.delay_ms   = 5;
    cfg.avg_buffer = 1;

    phSensor.startReading(cfg);
    while (phSensor.getState() != PHXState::IDLE) {
        phSensor.updateReading();
    }

    rxSensor.startReading(cfg);
    while (rxSensor.getState() != PHXState::IDLE) {
        rxSensor.updateReading();
    }

    // Print results
    Serial.print(phSensor.getLastReading(), 3);
    Serial.print(F("    "));
    Serial.println(rxSensor.getLastReading(), 1);

    delay(2000);
}
