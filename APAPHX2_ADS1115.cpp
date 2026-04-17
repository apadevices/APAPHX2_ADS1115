/**
 * @file APAPHX2_ADS1115.cpp
 * @brief ADS1115 16-bit ADC Library for pH and RX/ORP Measurements
 * @version 1.0.0
 * @author APADevices [@kecup]
 *
 * Implementation — Phase 1: ADC core layer
 *   - begin(), readADC(), setGain(), setDataRate()
 *   - Single-shot + OS-bit polling
 *   - Optional ALERT/RDY pin support
 *   - Differential MUX AIN1(+)/AIN0(−)
 *   - writeRegister(), readRegister(), waitForConversion()
 *   - getVoltageRange(), 16-bit math
 *   - Debug system hooks
 *
 * Phase 2 (state machine + memory) will add:
 *   startReading(), updateReading(), cancelReading()
 *
 * Phase 3 (calibration + chemistry) will add:
 *   calibratePHX(), calibratePHXReading(), saveCalibration(),
 *   loadCalibration(), applyTempCompensation(), validateRange()
 *
 * Phase 4 (rolling average) will add:
 *   setRollingAverage(), clearRollingAverage(), pushToAvgRing()
 *
 * Phase 5 (debug): complete — see enableDebug(), setDebugStream()
 */

#include "APAPHX2_ADS1115.h"



// ============================================================
// Constructor
// ============================================================

ADS1115_PHX::ADS1115_PHX(SensorType type,
                          uint8_t    i2cAddress,
                          int8_t     alertPin,
                          uint16_t   eepromBase)
    : _sensorType(type),
      _i2cAddress(i2cAddress),
      _alertPin(alertPin),
      _eepromBase(eepromBase),
      _gain(ADS1115_GAIN_1),          // subclass constructor overrides this
      _dataRate(ADS1115_DR_128),      // default 128 SPS ~7.8ms/conversion
      _state(PHXState::IDLE),
      _lastError(PHXError::NONE),
      _readingComplete(false),
      _lastReading(0.0f),
      _lastRawMV(0.0f),
      _sampleIndex(0),
      _lastSampleTime(0),
      _avgWindow(1),
      _avgIndex(0),
      _avgFilled(0),
      _avgReady(false),
      _waitingForDelay(false),
      _tempCompEnabled(false),
      _temperature(25.0f),
      _debugEnabled(false),
      _debugStream(nullptr),
      _messageCallback(nullptr)
{
    // Fixed buffers zeroed by C runtime on AVR (global objects).
    // begin() performs explicit clear — no redundant memset here.
}

// ============================================================
// begin()
// ============================================================

void ADS1115_PHX::begin(bool initWire) {
    if (initWire) Wire.begin();

    // Clear fixed buffers — ensures no garbage from uninitialised RAM
    memset(_samples, 0, sizeof(_samples));
    memset(_avgRing, 0, sizeof(_avgRing));
    _sampleIndex    = 0;
    _waitingForDelay = false;
    _avgIndex       = 0;
    _avgFilled      = 0;
    _avgReady       = false;
    _state          = PHXState::IDLE;
    _lastError      = PHXError::NONE;
    _readingComplete = false;

    // Configure ALERT/RDY pin if provided
    if (_alertPin != NO_ALERT) {
        pinMode(_alertPin, INPUT);

        // Configure ADS1115 ALERT/RDY pin to assert on conversion ready:
        // Set Lo_thresh MSB = 0, Hi_thresh MSB = 1
        // This puts ALERT/RDY in conversion-ready mode
        writeRegister(0x02, 0x0000); // Lo_thresh register → 0x0000
        writeRegister(0x03, 0x8000); // Hi_thresh register → 0x8000

        ADS1115_DBG_PRINTLN(F("[ADS1115] ALERT/RDY pin configured"));
    }

    // Debug stream defaults to Serial if not set by user
    #ifdef ADS1115_DEBUG
    if (_debugStream == nullptr) {
        _debugStream = &Serial;
    }
    #endif

    ADS1115_DBG_PRINT(F("[ADS1115] begin() — type: "));
    ADS1115_DBG_PRINTLN(_sensorType == SensorType::PH ? F("PH") : F("RX"));
    ADS1115_DBG_PRINT(F("[ADS1115] I2C address: 0x"));
    ADS1115_DBG_PRINT2(_i2cAddress, HEX);
    ADS1115_DBG_PRINT(F("  gain: 0x"));
    ADS1115_DBG_PRINT2(_gain, HEX);
    ADS1115_DBG_PRINT(F("  dataRate: 0x"));
    ADS1115_DBG_PRINTLN2(_dataRate, HEX);

    // Option A — auto-load calibration from EEPROM
    // Silent: sets CALIB_INVALID if no valid data, keeps factory defaults
    // User can check isCalibrated() or getLastError() after begin()
    loadCalibration();
}

// ============================================================
// ADC configuration
// ============================================================

void ADS1115_PHX::setGain(uint16_t gain) {
    // Validate against known gain values
    switch (gain) {
        case ADS1115_GAIN_TWOTHIRDS:
        case ADS1115_GAIN_1:
        case ADS1115_GAIN_2:
        case ADS1115_GAIN_4:
        case ADS1115_GAIN_8:
        case ADS1115_GAIN_16:
            _gain = gain;
            ADS1115_DBG_PRINT(F("[ADS1115] setGain: 0x"));
            ADS1115_DBG_PRINTLN2(_gain, HEX);
            break;
        default:
            // Invalid gain — silently keep current setting
            ADS1115_DBG_PRINTLN(F("[ADS1115] setGain: invalid value, ignored"));
            break;
    }
}

void ADS1115_PHX::setDataRate(uint16_t dataRate) {
    switch (dataRate) {
        case ADS1115_DR_8:   case ADS1115_DR_16:
        case ADS1115_DR_32:  case ADS1115_DR_64:
        case ADS1115_DR_128: case ADS1115_DR_250:
        case ADS1115_DR_475: case ADS1115_DR_860:
            _dataRate = dataRate;
            ADS1115_DBG_PRINT(F("[ADS1115] setDataRate: 0x"));
            ADS1115_DBG_PRINTLN2(_dataRate, HEX);
            return;
        default:
            ADS1115_DBG_PRINTLN(F("[ADS1115] setDataRate: invalid value, ignored"));
    }
}

// ============================================================
// Raw ADC read — single differential conversion
// AIN0(+) / AIN1(−): MUX = 000 = 0x0000
// AIN0 = 2.5V offset reference, AIN1 = probe signal + offset
// Result = AIN0 − AIN1 = −probe_signal (sign inverted)
// Calibration math compensates for sign inversion.
// Single-shot mode, dummy read settling + OS-bit polling or ALERT pin
// ============================================================

int16_t ADS1115_PHX::readADC() {
    // Public blocking convenience read.
    // Uses internal helpers — same path as state machine but synchronous.
    // Suitable for: direct user reads, calibration routines.
    // Not suitable for: use inside updateReading() state machine.
    _triggerConversion();

    if (!waitForConversion()) {
        ADS1115_DBG_PRINTLN(F("[ADS1115] readADC: conversion timeout!"));
        return 0;
    }

    int16_t raw = _readResult();

    ADS1115_DBG_PRINT(F("[ADS1115] readADC raw="));
    ADS1115_DBG_PRINT(raw);

    #ifdef ADS1115_DEBUG
    float mV = ((float)raw * getVoltageRange()) / 32768.0f * 1000.0f;
    ADS1115_DBG_PRINT(F("  mV="));
    ADS1115_DBG_PRINT2(mV, 3);
    ADS1115_DBG_PRINTLN(F(""));
    #endif

    return raw;
}

// ============================================================
// Conversion ready — OS-bit polling or ALERT pin
// ============================================================

bool ADS1115_PHX::waitForConversion(uint16_t timeoutMs) {
    unsigned long start = millis();

    if (_alertPin != NO_ALERT) {
        // ALERT/RDY pin mode — pin goes LOW when conversion ready
        // (configured in begin() via Lo/Hi threshold registers)
        while (digitalRead(_alertPin) == HIGH) {
            if (millis() - start >= timeoutMs) {
                return false; // timeout
            }
        }
        ADS1115_DBG_PRINTLN(F("[ADS1115] ready via ALERT pin"));
        return true;
    }

    // OS-bit polling fallback
    // OS bit = 1 in config register means conversion complete
    uint8_t pollCount = 0;
    while (!(readRegister(ADS1115_REG_CONFIG) & ADS1115_OS_READY)) {
        if (millis() - start >= timeoutMs) {
            return false; // timeout
        }
        pollCount++;
        // Small yield — important on ESP32/ESP8266 to feed watchdog
        // On AVR this compiles to nothing, no overhead
        #if defined(ESP8266) || defined(ESP32)
        yield();
        #endif
    }

    ADS1115_DBG_PRINT(F("[ADS1115] ready via OS-bit, polls: "));
    ADS1115_DBG_PRINTLN(pollCount);

    return true;
}

// ============================================================
// getVoltageRange() — maps gain constant to full-scale Volts
// ============================================================

float ADS1115_PHX::getVoltageRange() const {
    switch (_gain) {
        case ADS1115_GAIN_TWOTHIRDS: return 6.144f;
        case ADS1115_GAIN_1:         return 4.096f;
        case ADS1115_GAIN_2:         return 2.048f;
        case ADS1115_GAIN_4:         return 1.024f;
        case ADS1115_GAIN_8:         return 0.512f;
        case ADS1115_GAIN_16:        return 0.256f;
        default:                     return 4.096f; // safe fallback
    }
}

// ============================================================
// I2C register access
// ============================================================

void ADS1115_PHX::writeRegister(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(_i2cAddress);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8));    // high byte first
    Wire.write((uint8_t)(value & 0xFF));  // low byte
    Wire.endTransmission();
}

uint16_t ADS1115_PHX::readRegister(uint8_t reg) {
    Wire.beginTransmission(_i2cAddress);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(_i2cAddress, (uint8_t)2);
    uint16_t value = ((uint16_t)Wire.read() << 8) | (uint16_t)Wire.read();
    return value;
}

// ============================================================
// Debug system — runtime control
// ============================================================

void ADS1115_PHX::enableDebug(bool enabled) {
    _debugEnabled = enabled;
    #ifdef ADS1115_DEBUG
    if (_debugStream == nullptr) {
        _debugStream = &Serial;
    }
    #endif
}

void ADS1115_PHX::setDebugStream(Stream& stream) {
    _debugStream = &stream;
}

void ADS1115_PHX::setMessageCallback(MessageCallback callback) {
    _messageCallback = callback;
}

void ADS1115_PHX::_notify(const __FlashStringHelper* msg) {
    if (_messageCallback) _messageCallback(msg);
}

// ============================================================
// Phase 2 — State machine implementation
// Non-blocking: no delay() anywhere in this path.
// Inter-sample timing via millis() in updateReading().
// ============================================================

// ------------------------------------------------------------
// _triggerConversion() — start a single-shot conversion
// Non-blocking — returns immediately after I2C write.
// Dummy read gives chip time to clear OS bit before polling.
// ------------------------------------------------------------
void ADS1115_PHX::_triggerConversion() {
    uint16_t config = ADS1115_OS_START       |
                      ADS1115_MUX_DIFF_0_1   |
                      _gain                  |
                      ADS1115_MODE_SINGLESHOT |
                      _dataRate              |
                      ADS1115_COMP_QUE_DISABLE;
    writeRegister(ADS1115_REG_CONFIG, config);
    readRegister(ADS1115_REG_CONFIG); // dummy read — OS-bit settling
}

// ------------------------------------------------------------
// _isConversionReady() — single non-blocking OS-bit check
// Returns true if conversion complete, false if still busy.
// ------------------------------------------------------------
bool ADS1115_PHX::_isConversionReady() {
    if (_alertPin != NO_ALERT) {
        return (digitalRead(_alertPin) == LOW);
    }
    return (readRegister(ADS1115_REG_CONFIG) & ADS1115_OS_READY);
}

// ------------------------------------------------------------
// _readResult() — read raw conversion register
// Call only after _isConversionReady() returns true.
// ------------------------------------------------------------
int16_t ADS1115_PHX::_readResult() {
    return (int16_t)readRegister(ADS1115_REG_CONVERT);
}

// ------------------------------------------------------------
// _processReading() — average samples, calibrate, validate
// Called on COLLECTING → PROCESSING transition.
// ------------------------------------------------------------
void ADS1115_PHX::_processReading() {
    // Average all collected raw samples (in mV)
    float sum = 0.0f;
    for (uint8_t i = 0; i < _config.samples; i++) {
        sum += _samples[i];
    }
    float meanMV = sum / (float)_config.samples;
    _lastRawMV   = meanMV; // always store raw mV before calibration

    ADS1115_DBG_PRINT(F("[ADS1115] process: meanMV="));
    ADS1115_DBG_PRINT2(meanMV, 3);

    // Inline abs for float — avoids math.h dependency on AVR
    float calSpan = _cal.ref2_mV - _cal.ref1_mV;
    if (calSpan < 0.0f) calSpan = -calSpan;
    bool  calibrated = (calSpan > 0.001f);

    float value;

    // Apply two-point calibration if available
    // Calibration maps measured mV to known reference values.
    // Works regardless of sign direction — hardware characteristic.
    if (calibrated) {
        value = _cal.ref1_value +
                (_cal.ref2_value - _cal.ref1_value) *
                (meanMV - _cal.ref1_mV) /
                (_cal.ref2_mV - _cal.ref1_mV);

        ADS1115_DBG_PRINT(F("  calibrated="));
        ADS1115_DBG_PRINT2(value, 4);
    } else {
        // Not yet calibrated — return raw mV as-is, no range clamping
        value = meanMV;
        ADS1115_DBG_PRINT(F("  uncalibrated (raw mV)"));
    }

    // Temperature compensation — pH only, calibrated only, if enabled
    if (calibrated &&
        _sensorType == SensorType::PH &&
        _tempCompEnabled &&
        isValidTemperature(_temperature)) {
        value = applyTempCompensation(value, _temperature);
        ADS1115_DBG_PRINT(F("  tempComp="));
        ADS1115_DBG_PRINT2(value, 4);
    }

    ADS1115_DBG_PRINTLN(F(""));

    // Range validation — only meaningful on calibrated values
    if (calibrated) {
        validateRange(value);
    } else {
        _lastError = PHXError::NONE;
    }

    // Push to rolling average (Phase 4 implements ring, stub is passthrough)
    pushToAvgRing(value);

    _readingComplete = true;
}

// ------------------------------------------------------------
// startReading() — begin a new measurement cycle
// Non-blocking — triggers first conversion and returns.
// ------------------------------------------------------------
void ADS1115_PHX::startReading(const PHXConfig& config) {
    if (_state != PHXState::IDLE) return; // already running

    // Store and clamp config — use instance sensorType, not config.type
    _config         = config;
    _config.samples    = constrain(config.samples,    1, ADS1115_MAX_SAMPLES);
    _config.avg_buffer = constrain(config.avg_buffer, 1, ADS1115_MAX_AVG);

    // Reset cycle state
    _sampleIndex     = 0;
    _waitingForDelay = false;
    _readingComplete = false;
    _lastError       = PHXError::NONE;

    // Apply rolling average window if changed
    if (_config.avg_buffer != _avgWindow) {
        setRollingAverage(_config.avg_buffer);
    }

    ADS1115_DBG_PRINT(F("[ADS1115] startReading: samples="));
    ADS1115_DBG_PRINT(_config.samples);
    ADS1115_DBG_PRINT(F(" delay_ms="));
    ADS1115_DBG_PRINT(_config.delay_ms);
    ADS1115_DBG_PRINT(F(" avg="));
    ADS1115_DBG_PRINTLN(_config.avg_buffer);

    // Trigger first conversion immediately
    _triggerConversion();
    _lastSampleTime = millis();
    _state          = PHXState::COLLECTING;
}

// ------------------------------------------------------------
// updateReading() — advance state machine one step
// Call repeatedly from loop() — returns immediately each call.
// ------------------------------------------------------------
void ADS1115_PHX::updateReading() {
    switch (_state) {

        case PHXState::IDLE:
            return; // nothing to do

        case PHXState::COLLECTING: {
            // The COLLECTING state runs a two-phase loop per sample:
            //
            // Phase A — WAITING: conversion triggered, poll for ready
            //   _waitingForDelay = false
            //   Check OS-bit each call. When ready → read sample → Phase B.
            //
            // Phase B — DELAY: sample read, waiting inter-sample pause
            //   _waitingForDelay = true
            //   Check millis() each call. When delay elapsed → trigger → Phase A.
            //
            // _waitingForDelay starts false (first conversion triggered in startReading)

            if (!_waitingForDelay) {
                // Phase A: waiting for conversion result
                if (!_isConversionReady()) return; // not done yet

                // Conversion ready — read and store sample as mV
                int16_t raw = _readResult();
                float   mV  = ((float)raw * getVoltageRange()) / 32768.0f * 1000.0f;
                _samples[_sampleIndex] = mV;

                ADS1115_DBG_PRINT(F("[ADS1115] sample["));
                ADS1115_DBG_PRINT(_sampleIndex);
                ADS1115_DBG_PRINT(F("]="));
                ADS1115_DBG_PRINT2(mV, 3);
                ADS1115_DBG_PRINTLN(F(" mV"));

                _sampleIndex++;

                // All samples done → process
                if (_sampleIndex >= _config.samples) {
                    _state = PHXState::PROCESSING;
                    return;
                }

                // More samples needed
                if (_config.delay_ms == 0) {
                    // No inter-sample delay — trigger immediately
                    _triggerConversion();
                    // stay in Phase A
                } else {
                    // Start delay period — record timestamp, enter Phase B
                    _lastSampleTime  = millis();
                    _waitingForDelay = true;
                }

            } else {
                // Phase B: inter-sample delay period
                if ((millis() - _lastSampleTime) < _config.delay_ms) {
                    return; // delay not yet elapsed
                }
                // Delay elapsed — trigger next conversion, enter Phase A
                _triggerConversion();
                _waitingForDelay = false;
            }
            return;
        }

        case PHXState::PROCESSING:
            _processReading();
            _state = PHXState::IDLE;
            return;
    }
}

// ------------------------------------------------------------
// cancelReading() — abort measurement, reset to IDLE
// ------------------------------------------------------------
void ADS1115_PHX::cancelReading() {
    _state           = PHXState::IDLE;
    _readingComplete = false;
    _sampleIndex     = 0;
    _waitingForDelay = false;
    _lastError       = PHXError::NONE;

    ADS1115_DBG_PRINTLN(F("[ADS1115] cancelReading"));
}

// ============================================================
// Phase 3 — Calibration + Chemistry
// ============================================================

// ------------------------------------------------------------
// calibratePHXReading() — blocking stable reading for calibration
//
// Process:
//   1. Print soak message — user already placed probe in buffer
//   2. Mandatory 200s soak — probe equilibrates in buffer solution
//      pH/ORP electrodes need 60-180s to reach stable potential
//   3. Three-window stability loop (A, B, C):
//      Each window = 50 samples × 10ms ≈ 890ms
//      Pause 500ms between windows
//      Check: |A-B|, |B-C|, |A-C| all < STABILITY_THRESHOLD (0.5mV)
//      Third condition catches slow monotonic drift A→B→C
//   4. Loop runs until stable or CAL_TIMEOUT_MS (360s) elapsed
//   5. On timeout: return best result + CALIB_INVALID
//
// Temperature compensation disabled for duration — raw mV only.
// delay() permitted — calibration is a privileged blocking process.
// ------------------------------------------------------------
float ADS1115_PHX::calibratePHXReading() {
    // Disable temp compensation — calibration captures raw probe mV
    bool tempWasEnabled = _tempCompEnabled;
    _tempCompEnabled    = false;

    // Config for calibration windows
    PHXConfig calCfg;
    calCfg.samples    = ADS1115_CAL_SAMPLES;
    calCfg.delay_ms   = ADS1115_CAL_DELAY_MS;
    calCfg.avg_buffer = 1;

    // Mandatory soak — probe must equilibrate in buffer solution.
    // One message, then silent wait. No variables, minimal SRAM use.
    _notify(F("Cal: wait 200s..."));

    for (uint32_t s = 0; s < (ADS1115_CAL_SOAK_MS / 1000UL); s++) {
        delay(1000);
        #if defined(ESP8266) || defined(ESP32)
        yield();
        #endif
    }

    ADS1115_DBG_PRINTLN(F("[ADS1115] calReading: soak complete, starting stability loop"));

    float winA   = 0.0f;
    float winB   = 0.0f;
    float winC   = 0.0f;
    float bestMV = 0.0f;
    uint32_t loopStart = millis();
    uint32_t attempt   = 0;

    // Capture initial window A before loop
    startReading(calCfg);
    while (getState() != PHXState::IDLE) { updateReading(); }
    winA   = _lastRawMV; // raw mV — not affected by existing calibration
    bestMV = winA;
    delay(ADS1115_CAL_PAUSE_MS);

    while ((millis() - loopStart) < ADS1115_CAL_TIMEOUT_MS) {

        // Window B
        startReading(calCfg);
        while (getState() != PHXState::IDLE) { updateReading(); }
        winB = _lastRawMV;
        delay(ADS1115_CAL_PAUSE_MS);

        // Window C
        startReading(calCfg);
        while (getState() != PHXState::IDLE) { updateReading(); }
        winC = _lastRawMV;

        attempt++;

        // Inline abs — no math.h
        float diffAB = winA - winB; if (diffAB < 0.0f) diffAB = -diffAB;
        float diffBC = winB - winC; if (diffBC < 0.0f) diffBC = -diffBC;
        float diffAC = winA - winC; if (diffAC < 0.0f) diffAC = -diffAC;

        bestMV = (winA + winB + winC) / 3.0f;

        ADS1115_DBG_PRINT(F("[ADS1115] calReading #"));
        ADS1115_DBG_PRINT((unsigned long)attempt);
        ADS1115_DBG_PRINT(F(" A="));   ADS1115_DBG_PRINT2(winA, 3);
        ADS1115_DBG_PRINT(F(" B="));   ADS1115_DBG_PRINT2(winB, 3);
        ADS1115_DBG_PRINT(F(" C="));   ADS1115_DBG_PRINT2(winC, 3);
        ADS1115_DBG_PRINT(F(" |AB|=")); ADS1115_DBG_PRINT2(diffAB, 3);
        ADS1115_DBG_PRINT(F(" |BC|=")); ADS1115_DBG_PRINT2(diffBC, 3);
        ADS1115_DBG_PRINT(F(" |AC|=")); ADS1115_DBG_PRINT2(diffAC, 3);

        if (diffAB < ADS1115_STABILITY_THRESHOLD &&
            diffBC < ADS1115_STABILITY_THRESHOLD &&
            diffAC < ADS1115_STABILITY_THRESHOLD) {
            // All three windows agree — probe is stable
            _notify(F("Calibration: stable!"));
            ADS1115_DBG_PRINTLN(F(" STABLE"));
            _lastError       = PHXError::NONE;
            _tempCompEnabled = tempWasEnabled;
            return bestMV;
        }

        ADS1115_DBG_PRINTLN(F(" drifting..."));

        // Slide window: C becomes next A
        winA = winC;
        delay(ADS1115_CAL_PAUSE_MS);
    }

    // Timeout — return best result with error flag
    _notify(F("Cal: timeout-check! "));
    ADS1115_DBG_PRINTLN(F("[ADS1115] calReading: TIMEOUT"));
    _lastError       = PHXError::CALIB_INVALID;
    _tempCompEnabled = tempWasEnabled;
    return bestMV;
}

// ------------------------------------------------------------
// calibratePHX() — validate and store cal struct to RAM
// ------------------------------------------------------------
bool ADS1115_PHX::calibratePHX(const PHX_Calibration& cal) {
    // Validate — points must not be identical (division by zero in cal math)
    float mvSpan  = cal.ref2_mV    - cal.ref1_mV;
    float valSpan = cal.ref2_value - cal.ref1_value;
    if (mvSpan    < 0.0f) mvSpan    = -mvSpan;
    if (valSpan   < 0.0f) valSpan   = -valSpan;

    if (mvSpan < 0.001f || valSpan < 0.001f) {
        _lastError = PHXError::CALIB_INVALID;
        ADS1115_DBG_PRINTLN(F("[ADS1115] calibratePHX: INVALID — points too close"));
        return false;
    }

    _cal       = cal;
    _lastError = PHXError::NONE;

    ADS1115_DBG_PRINT(F("[ADS1115] calibratePHX: ref1="));
    ADS1115_DBG_PRINT2(_cal.ref1_mV, 3);
    ADS1115_DBG_PRINT(F("mV→"));
    ADS1115_DBG_PRINT2(_cal.ref1_value, 3);
    ADS1115_DBG_PRINT(F("  ref2="));
    ADS1115_DBG_PRINT2(_cal.ref2_mV, 3);
    ADS1115_DBG_PRINT(F("mV→"));
    ADS1115_DBG_PRINTLN2(_cal.ref2_value, 3);

    return true;
}

// ------------------------------------------------------------
// saveCalibration() — persist RAM calibration to EEPROM
// Uses EEPROM.update() — only writes changed bytes (wear levelled)
// Layout: [0]=version byte, [1..16]=PHX_Calibration struct
// ------------------------------------------------------------
bool ADS1115_PHX::saveCalibration() {
    // Guard against EEPROM overflow on small devices
    #if defined(E2END)
    if ((_eepromBase + 16) > E2END) {
        ADS1115_DBG_PRINTLN(F("[ADS1115] saveCalibration: EEPROM too small!"));
        return false;
    }
    #endif

    // Write version byte
    EEPROM.update(_eepromBase, ADS1115_EEPROM_VERSION);

    // Write calibration struct byte by byte
    const uint8_t* p = (const uint8_t*)&_cal;
    for (uint8_t i = 0; i < sizeof(PHX_Calibration); i++) {
        EEPROM.update(_eepromBase + 1 + i, p[i]);
    }

    ADS1115_DBG_PRINT(F("[ADS1115] saveCalibration: written at addr="));
    ADS1115_DBG_PRINTLN(_eepromBase);

    return true;
}

// ------------------------------------------------------------
// loadCalibration() — read calibration from EEPROM
// Version byte checked first — mismatch → CALIB_INVALID + defaults
// ------------------------------------------------------------
bool ADS1115_PHX::loadCalibration() {
    // Check version byte
    if (EEPROM.read(_eepromBase) != ADS1115_EEPROM_VERSION) {
        _lastError = PHXError::CALIB_INVALID;
        ADS1115_DBG_PRINTLN(F("[ADS1115] loadCalibration: version MISMATCH — using defaults"));
        return false;
    }

    // Read calibration struct
    PHX_Calibration loaded;
    uint8_t* p = (uint8_t*)&loaded;
    for (uint8_t i = 0; i < sizeof(PHX_Calibration); i++) {
        p[i] = EEPROM.read(_eepromBase + 1 + i);
    }

    // Validate loaded data
    float mvSpan  = loaded.ref2_mV    - loaded.ref1_mV;
    float valSpan = loaded.ref2_value - loaded.ref1_value;
    if (mvSpan  < 0.0f) mvSpan  = -mvSpan;
    if (valSpan < 0.0f) valSpan = -valSpan;

    if (mvSpan < 0.001f || valSpan < 0.001f) {
        _lastError = PHXError::CALIB_INVALID;
        ADS1115_DBG_PRINTLN(F("[ADS1115] loadCalibration: data INVALID — using defaults"));
        return false;
    }

    _cal       = loaded;
    _lastError = PHXError::NONE;

    ADS1115_DBG_PRINT(F("[ADS1115] loadCalibration: loaded ref1="));
    ADS1115_DBG_PRINT2(_cal.ref1_mV, 3);
    ADS1115_DBG_PRINT(F("mV ref2="));
    ADS1115_DBG_PRINTLN2(_cal.ref2_mV, 3);

    return true;
}

// ------------------------------------------------------------
// calibratePoint1() — guided cal: capture first reference point
// ------------------------------------------------------------
float ADS1115_PHX::calibratePoint1(float knownValue) {
    ADS1115_DBG_PRINT(F("[ADS1115] calibratePoint1: knownValue="));
    ADS1115_DBG_PRINTLN2(knownValue, 3);

    float mV         = calibratePHXReading();
    _cal.ref1_mV     = mV;
    _cal.ref1_value  = knownValue;

    ADS1115_DBG_PRINT(F("[ADS1115] calibratePoint1: captured mV="));
    ADS1115_DBG_PRINTLN2(mV, 3);

    return mV;
}

// ------------------------------------------------------------
// calibratePoint2() — guided cal: capture second point + finalise
// Calls calibratePHX() internally — validates + stores to RAM.
// Does NOT auto-save to EEPROM. Call saveCalibration() explicitly.
// ------------------------------------------------------------
bool ADS1115_PHX::calibratePoint2(float knownValue) {
    ADS1115_DBG_PRINT(F("[ADS1115] calibratePoint2: knownValue="));
    ADS1115_DBG_PRINTLN2(knownValue, 3);

    float mV        = calibratePHXReading();
    _cal.ref2_mV    = mV;
    _cal.ref2_value = knownValue;

    ADS1115_DBG_PRINT(F("[ADS1115] calibratePoint2: captured mV="));
    ADS1115_DBG_PRINTLN2(mV, 3);

    // Finalise — validate and store to RAM
    return calibratePHX(_cal);
}

// ------------------------------------------------------------
// isCalibrated() — check if valid calibration is in RAM
// ------------------------------------------------------------
bool ADS1115_PHX::isCalibrated() const {
    float span = _cal.ref2_mV - _cal.ref1_mV;
    if (span < 0.0f) span = -span;
    return (span > 0.001f);
}

void ADS1115_PHX::enableTemperatureCompensation(bool enabled) {
    _tempCompEnabled = enabled;
    ADS1115_DBG_PRINT(F("[ADS1115] tempComp: "));
    ADS1115_DBG_PRINTLN(_tempCompEnabled ? F("enabled") : F("disabled"));
}

void ADS1115_PHX::setTemperature(float tempC) {
    if (isValidTemperature(tempC)) {
        _temperature = tempC;
        if (_lastError == PHXError::TEMP_INVALID) {
            _lastError = PHXError::NONE;
        }
        ADS1115_DBG_PRINT(F("[ADS1115] setTemperature: "));
        ADS1115_DBG_PRINTLN2(_temperature, 2);
    } else {
        _lastError = PHXError::TEMP_INVALID;
        ADS1115_DBG_PRINTLN(F("[ADS1115] setTemperature: INVALID"));
    }
}

bool ADS1115_PHX::isValidTemperature(float tempC) const {
    return (tempC >= 0.0f && tempC <= 50.0f);
}

float ADS1115_PHX::applyTempCompensation(float pHRaw, float tempC) const {
    // Pasco 2001 formula — normalises to 25°C
    return ((pHRaw - 7.0f) * (273.15f + tempC)) / (273.15f + 25.0f) + 7.0f;
}

void ADS1115_PHX::validateRange(float& value) {
    if (_sensorType == SensorType::PH) {
        if (value < 0.0f)  { value = 0.0f;  _lastError = PHXError::PH_LOW;  }
        else if (value > 14.0f) { value = 14.0f; _lastError = PHXError::PH_HIGH; }
        else { _lastError = PHXError::NONE; }
    } else {
        if (value < 0.0f)     { value = 0.0f;    _lastError = PHXError::RX_LOW;  }
        else if (value > 2000.0f) { value = 2000.0f; _lastError = PHXError::RX_HIGH; }
        else { _lastError = PHXError::NONE; }
    }
}

// ============================================================
// Phase 4 — Rolling Average
// Ring buffer, fixed size, pre-allocated in begin().
// Active only when _avgWindow >= 2.
// avg_buffer = 1 → passthrough, zero ring overhead.
// ============================================================

// ------------------------------------------------------------
// setRollingAverage() — enable and set window size
// Clears ring whenever window changes.
// ------------------------------------------------------------
void ADS1115_PHX::setRollingAverage(uint8_t n) {
    uint8_t newWindow = constrain(n, 1, ADS1115_MAX_AVG);
    if (newWindow != _avgWindow) {
        _avgWindow = newWindow;
        clearRollingAverage();
        ADS1115_DBG_PRINT(F("[ADS1115] setRollingAverage: window="));
        ADS1115_DBG_PRINTLN(_avgWindow);
    }
}

// ------------------------------------------------------------
// clearRollingAverage() — reset ring, keep window setting
// Call after calibration or probe swap to flush stale values.
// ------------------------------------------------------------
void ADS1115_PHX::clearRollingAverage() {
    memset(_avgRing, 0, sizeof(_avgRing));
    _avgIndex  = 0;
    _avgFilled = 0;
    _avgReady  = false;
    ADS1115_DBG_PRINTLN(F("[ADS1115] clearRollingAverage"));
}

// ------------------------------------------------------------
// pushToAvgRing() — store value, compute average, update _lastReading
//
// avg_buffer = 1: passthrough — _lastReading = value directly,
//                 ring not touched, zero overhead.
// avg_buffer >= 2: write into ring at _avgIndex, advance index
//                  (wraps via modulo), track fill count,
//                  set _avgReady when ring full first time,
//                  compute mean of filled slots only.
// ------------------------------------------------------------
void ADS1115_PHX::pushToAvgRing(float value) {
    if (_avgWindow < 2) {
        // Ring disabled — passthrough
        _lastReading = value;
        return;
    }

    // Write new value into ring
    _avgRing[_avgIndex] = value;
    _avgIndex = (_avgIndex + 1) % _avgWindow;

    // Track how many slots are filled (caps at _avgWindow)
    if (_avgFilled < _avgWindow) {
        _avgFilled++;
    }

    // Mark ready once ring is full for first time
    if (!_avgReady && _avgFilled >= _avgWindow) {
        _avgReady = true;
    }

    // Compute average over filled slots only
    // Avoids contamination from zeroed unfilled slots during warm-up
    float sum = 0.0f;
    for (uint8_t i = 0; i < _avgFilled; i++) {
        sum += _avgRing[i];
    }
    _lastReading = sum / (float)_avgFilled;

    ADS1115_DBG_PRINT(F("[ADS1115] avgRing: filled="));
    ADS1115_DBG_PRINT(_avgFilled);
    ADS1115_DBG_PRINT(F(" avg="));
    ADS1115_DBG_PRINTLN2(_lastReading, 3);
}
