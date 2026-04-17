/**
 * 05_TemperatureComp — Middle
 * ===========================
 * Demonstrates pH temperature compensation using the Pasco 2001 formula.
 * Temperature affects the electrode response — compensation normalises
 * all readings to the 25°C standard so values are comparable regardless
 * of actual water temperature.
 *
 * Formula: pH_comp = ((pH_raw - 7) × (273.15 + T)) / (273.15 + 25) + 7
 *
 * What you will learn:
 *   - enableTemperatureCompensation() — enable/disable
 *   - setTemperature() — provide current water temperature
 *   - getCurrentTemperature() — read current setting
 *   - Effect of temperature on pH readings
 *   - TEMP_INVALID error — out-of-range temperature handling
 *
 * Notes:
 *   - Temperature compensation applies to pH only (not ORP)
 *   - Only active when sensor is calibrated
 *   - Valid range: 0–50°C
 *   - At 25°C: compensation = 0 (reference temperature)
 *   - Update temperature regularly for accurate results
 *
 * Hardware: PHX v2 board, pH probe, temperature sensor (any — read
 *           separately and pass value via setTemperature())
 */

#include "APAPHX2_ADS1115.h"

ADS1115_PHX_PH phSensor(0x49);

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    Serial.println(F("APAPHX2_ADS1115 — 05 Temperature Compensation"));
    Serial.println(F("==============================================="));

    phSensor.begin();

    if (!phSensor.isCalibrated()) {
        Serial.println(F("ERROR: not calibrated — run 04_Calibration first"));
        while (true) { ; }
    }

    // ── Demonstrate compensation ON vs OFF ─────────────────
    Serial.println(F("Comparing readings at different temperatures:"));
    Serial.println(F("  Temp(C)  Without comp  With comp  Difference"));
    Serial.println(F("  ───────  ────────────  ─────────  ──────────"));

    float testTemps[] = { 10.0f, 20.0f, 25.0f, 30.0f, 40.0f };
    uint8_t numTemps = sizeof(testTemps) / sizeof(testTemps[0]);

    for (uint8_t i = 0; i < numTemps; i++) {
        float temp = testTemps[i];

        // Reading WITHOUT compensation
        phSensor.enableTemperatureCompensation(false);
        float rawPH = runCycle();

        // Reading WITH compensation at this temperature
        phSensor.enableTemperatureCompensation(true);
        phSensor.setTemperature(temp);
        float compPH = runCycle();

        float diff = compPH - rawPH;

        Serial.print(F("  "));
        if (temp < 10.0f) Serial.print(F(" "));
        Serial.print(temp, 1);
        Serial.print(F("       "));
        Serial.print(rawPH, 4);
        Serial.print(F("      "));
        Serial.print(compPH, 4);
        Serial.print(F("     "));
        if (diff >= 0) Serial.print(F("+"));
        Serial.println(diff, 4);
    }

    Serial.println();

    // ── Temperature error handling ──────────────────────────
    Serial.println(F("Temperature validation:"));

    phSensor.setTemperature(25.0f);
    Serial.print(F("  setTemperature(25.0): error="));
    printError(phSensor.getLastError());

    phSensor.setTemperature(-5.0f);   // below 0°C — invalid
    Serial.print(F("  setTemperature(-5.0): error="));
    printError(phSensor.getLastError());

    phSensor.setTemperature(60.0f);   // above 50°C — invalid
    Serial.print(F("  setTemperature(60.0): error="));
    printError(phSensor.getLastError());

    phSensor.setTemperature(25.0f);   // restore valid temperature

    Serial.println();
    Serial.println(F("Continuous readings with compensation at current temp:"));
    Serial.println(F("(update WATER_TEMP_C to your actual water temperature)"));
}

const float WATER_TEMP_C = 25.0f;   // <── set your actual water temperature

void loop() {
    phSensor.enableTemperatureCompensation(true);
    phSensor.setTemperature(WATER_TEMP_C);

    float ph = runCycle();
    Serial.print(F("pH ("));
    Serial.print(WATER_TEMP_C, 1);
    Serial.print(F("°C): "));
    Serial.println(ph, 3);

    delay(2000);
}

// Run one 10-sample measurement cycle
float runCycle() {
    PHXConfig cfg;
    cfg.samples    = 10;
    cfg.delay_ms   = 5;
    cfg.avg_buffer = 1;
    phSensor.startReading(cfg);
    while (phSensor.getState() != PHXState::IDLE) {
        phSensor.updateReading();
    }
    return phSensor.getLastReading();
}

void printError(PHXError e) {
    switch (e) {
        case PHXError::NONE:         Serial.println(F("NONE")); break;
        case PHXError::TEMP_INVALID: Serial.println(F("TEMP_INVALID")); break;
        default:                     Serial.println(F("other")); break;
    }
}
