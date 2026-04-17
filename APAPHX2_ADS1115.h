/**
 * @file APAPHX2_ADS1115.h
 * @brief ADS1115 16-bit ADC Library for pH and RX/ORP Measurements
 * @version 1.0.0
 * @author APADevices [@kecup]
 *
 * Dedicated library for the ADS1115 ADC, optimized for APADevices PHX v2 board.
 * Based on APAPHX_ADS1015 but fully rewritten for 16-bit delta-sigma performance.
 *
 * ── HARDWARE OVERVIEW ───────────────────────────────────────────────────────
 *
 * APADevices PHX v2 board has TWO isolated power domains:
 *
 * Side 1 — MCU / Logic side (connector P1, IDC-2x5):
 *   Vcc-MC1 + GND-MC1 : MCU supply voltage — 3.3V or 5V (match your MCU)
 *   SCL-MC1, SDA-MC1  : I2C bus — must be same logic level as MCU
 *   PH-Alert           : ALERT/RDY from pH ADS1115 (optional)
 *   RX-Alert           : ALERT/RDY from RX ADS1115 (optional)
 *   H3 jumper          : enables onboard 4.7kΩ I2C pullups (R36, R37)
 *                        — enable if no external pullups on I2C bus
 *
 * Side 2 — Analog / Isolated side (connector CN2, 2-pin):
 *   +12V IN + GND12   : external 12V DC supply (required, always)
 *                        — feeds internal regulators behind ADUM1251 isolator
 *                        — protected by PolyPTC fuse (500mA), Schottky (reverse
 *                          polarity) and TVS (15V/24.4V transient)
 *
 * REQUIRED CONNECTIONS — both must always be present:
 *   1. External 12V DC supply → CN2 (+12V IN and GND12)
 *   2. MCU logic supply → P1 (Vcc-MC1 and GND-MC1, same voltage as MCU)
 *      + I2C bus → P1 (SCL-MC1 and SDA-MC1)
 *
 * The two grounds (GND-MC1 and GND12) are galvanically isolated from each
 * other by the ADUM1251 I2C isolator. Do NOT connect them together.
 *
 * ── ADS1115 DIFFERENTIAL INPUT ──────────────────────────────────────────────
 *
 * Both ADS1115 ICs use differential mode AIN0(+)/AIN1(−) — MUX 000 = 0x0000:
 *   AIN0 = 2.5V offset reference (probe analog-GND)
 *   AIN1 = LMP7721 OPAMP output (probe signal + 2.5V offset)
 *   Result = AIN0 − AIN1 = −probe_signal (sign inverted by hardware)
 *   Calibration two-point math compensates for sign — no user action needed.
 *
 * ── GAIN SELECTION ──────────────────────────────────────────────────────────
 *
 * pH channel  — Gain 2 (±2.048V, LSB = 62.5μV):
 *   Differential pH signal: ±~500mV max (14 pH × 59mV/pH from neutral)
 *   Fits well within ±2.048V range — Gain 2 gives better resolution.
 *
 * RX channel  — Gain 1 (±4.096V, LSB = 125μV):
 *   ORP probe output: ±2000mV full range
 *   Does not fit in ±2.048V — Gain 1 required.
 *
 * ── MEASUREMENT PERFORMANCE ─────────────────────────────────────────────────
 *
 * - pH:  0–14,     ±0.002 pH accuracy,  Gain 2 (±2.048V), ~947 counts/pH unit
 * - RX:  ±2000mV,  ±0.5mV  accuracy,   Gain 1 (±4.096V), ~8   counts/mV
 *
 * ── TYPICAL USAGE ───────────────────────────────────────────────────────────
 *
 * @code
 * #include "APAPHX2_ADS1115.h"
 *
 * ADS1115_PHX_PH phSensor(0x49);   // pH,  I2C 0x49, Gain 2 default
 * ADS1115_PHX_RX rxSensor(0x48);   // RX,  I2C 0x48, Gain 1 default
 *
 * void setup() {
 *     phSensor.begin();
 *     rxSensor.begin();
 *     if (!phSensor.isCalibrated()) {
 *         // run calibration procedure — see calibratePoint1/2()
 *     }
 * }
 * @endcode
 */

#ifndef APAPHX2_ADS1115_H
#define APAPHX2_ADS1115_H

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

// ============================================================
// Debug system — compile-time gate
// Uncomment the line below to enable debug output globally.
// Works in both Arduino IDE and PlatformIO without build flags.
// IMPORTANT: comment it out again before production use.
// ============================================================
// #define ADS1115_DEBUG   // <-- uncomment to enable debug output
#ifdef ADS1115_DEBUG
  #define ADS1115_DBG_PRINT(x)       if(_debugEnabled && _debugStream) _debugStream->print(x)
  #define ADS1115_DBG_PRINTLN(x)     if(_debugEnabled && _debugStream) _debugStream->println(x)
  #define ADS1115_DBG_PRINT2(x,f)    if(_debugEnabled && _debugStream) _debugStream->print(x,f)
  #define ADS1115_DBG_PRINTLN2(x,f)  if(_debugEnabled && _debugStream) _debugStream->println(x,f)
#else
  #define ADS1115_DBG_PRINT(x)
  #define ADS1115_DBG_PRINTLN(x)
  #define ADS1115_DBG_PRINT2(x,f)
  #define ADS1115_DBG_PRINTLN2(x,f)
#endif

// ============================================================
// EEPROM configuration
// ============================================================
#define ADS1115_EEPROM_VERSION  0xA2  ///< Increment if calibration data format changes
#define ADS1115_EEPROM_BASE     128   ///< Default base address — override per instance via constructor
                                      ///< ADS1115_PHX_PH default: 128 (occupies 128–160)
                                      ///< ADS1115_PHX_RX default: 161 (occupies 161–193)

// ============================================================
// Buffer sizes — fixed, AVR safe, no heap after begin()
// ============================================================
#define ADS1115_MAX_SAMPLES     25    ///< Hard cap on samples per reading cycle
                                      ///< Recommended default: 10 samples @ 128SPS = ~78ms/cycle
#define ADS1115_MAX_AVG         10    ///< Rolling average ring buffer max window size

// ============================================================
// Calibration constants
// ============================================================
#define ADS1115_CAL_SAMPLES          50       ///< Samples per calibration window (~890ms per window)
#define ADS1115_CAL_DELAY_MS         10       ///< Inter-sample delay during calibration (ms)
#define ADS1115_CAL_PAUSE_MS        500       ///< Pause between consecutive windows (ms)
#define ADS1115_CAL_SOAK_MS      200000UL    ///< Mandatory probe soak before stability loop (200s)
                                              ///< pH/ORP electrodes need 60-180s to equilibrate
                                              ///< in a new buffer solution before readings are valid.
#define ADS1115_CAL_TIMEOUT_MS   360000UL    ///< Max time for stability loop (360s = 6 minutes)
                                              ///< Replaces fixed retry count — time-based like v1.
                                              ///< On timeout: returns best result + CALIB_INVALID.
#define ADS1115_STABILITY_THRESHOLD  0.5f    ///< Per-window threshold (mV).
                                              ///< Three-window check: |A-B|, |B-C|, |A-C| < 0.5mV
                                              ///< = 4 LSB at Gain 1 (125μV/LSB). ~0.008 pH.

// ============================================================
// I2C addresses
// ============================================================
#define ADS1115_ADDR_48         0x48  ///< ADDR pin → GND  (RX/ORP board)
#define ADS1115_ADDR_49         0x49  ///< ADDR pin → VDD  (pH board)
#define ADS1115_ADDR_4A         0x4A  ///< ADDR pin → SDA
#define ADS1115_ADDR_4B         0x4B  ///< ADDR pin → SCL

// ============================================================
// ADS1115 register pointers
// ============================================================
#define ADS1115_REG_CONVERT     0x00  ///< Conversion result register
#define ADS1115_REG_CONFIG      0x01  ///< Configuration register

// ============================================================
// Config register — OS bit (single-shot trigger + ready flag)
// ============================================================
#define ADS1115_OS_START        0x8000  ///< Write: start single conversion
#define ADS1115_OS_READY        0x8000  ///< Read:  1 = conversion complete, 0 = busy

// ============================================================
// Config register — MUX (input multiplexer)
// ADS1115 MUX truth table (bits [14:12]):
//   000 = 0x0000 → AIN0(+) / AIN1(−)  ← PHX v2 default
//   001 = 0x1000 → AIN0(+) / AIN3(−)
//   010 = 0x2000 → AIN1(+) / AIN3(−)
//   011 = 0x3000 → AIN2(+) / AIN3(−)  ← NOT AIN1/AIN0!
//   100 = 0x4000 → AIN0 single-ended
//   101 = 0x5000 → AIN1 single-ended
//   110 = 0x6000 → AIN2 single-ended
//   111 = 0x7000 → AIN3 single-ended
//
// PHX v2 hardware:
//   AIN0 = 2.5V offset reference (probe analog-GND)
//   AIN1 = LMP7721 OPAMP output (probe signal + 2.5V offset)
//   Result = AIN0 − AIN1 = −probe_signal (sign inverted)
//   Calibration math compensates for sign inversion.
//
// NOTE: AIN1(+)/AIN0(−) does NOT exist in ADS1115 MUX table.
//       The correct pair is AIN0(+)/AIN1(−) = MUX 000 = 0x0000.
// ============================================================
#define ADS1115_MUX_DIFF_0_1   0x0000  ///< Differential AIN0(+)/AIN1(−) — PHX v2 default
#define ADS1115_MUX_DIFF_0_3   0x1000  ///< Differential AIN0(+)/AIN3(−)
#define ADS1115_MUX_DIFF_1_3   0x2000  ///< Differential AIN1(+)/AIN3(−)
#define ADS1115_MUX_DIFF_2_3   0x3000  ///< Differential AIN2(+)/AIN3(−)
#define ADS1115_MUX_SINGLE_0   0x4000  ///< Single-ended AIN0
#define ADS1115_MUX_SINGLE_1   0x5000  ///< Single-ended AIN1
#define ADS1115_MUX_SINGLE_2   0x6000  ///< Single-ended AIN2
#define ADS1115_MUX_SINGLE_3   0x7000  ///< Single-ended AIN3

// ============================================================
// Config register — PGA (programmable gain amplifier)
// PHX v2 defaults:
//   pH channel → ADS1115_GAIN_2 (±2.048V, LSB = 62.5μV)
//               pH differential signal ±~500mV — fits Gain 2 range
//   RX channel → ADS1115_GAIN_1 (±4.096V, LSB = 125μV)
//               ORP probe ±2000mV full range — requires Gain 1
// ============================================================
#define ADS1115_GAIN_TWOTHIRDS  0x0000  ///< ±6.144V — LSB 187.5μV  bits[11:9]=000
#define ADS1115_GAIN_1          0x0200  ///< ±4.096V — LSB 125μV   ← RX default
#define ADS1115_GAIN_2          0x0400  ///< ±2.048V — LSB 62.5μV  ← pH default
#define ADS1115_GAIN_4          0x0600  ///< ±1.024V — LSB 31.25μV
#define ADS1115_GAIN_8          0x0800  ///< ±0.512V — LSB 15.625μV
#define ADS1115_GAIN_16         0x0A00  ///< ±0.256V — LSB 7.8125μV

// ============================================================
// Config register — MODE
// ============================================================
#define ADS1115_MODE_CONTINUOUS 0x0000  ///< Continuous conversion
#define ADS1115_MODE_SINGLESHOT 0x0100  ///< Single-shot, power-down after (default)

// ============================================================
// Config register — DR (data rate / samples per second)
// Default: ADS1115_DR_128 (~7.8ms per conversion)
// Lower rates = more internal averaging = better noise rejection
// ============================================================
#define ADS1115_DR_8            0x0000  ///< 8   SPS — 125ms/conversion  bits[7:5]=000
#define ADS1115_DR_16           0x0020  ///< 16  SPS — 62.5ms/conversion
#define ADS1115_DR_32           0x0040  ///< 32  SPS — 31.25ms/conversion
#define ADS1115_DR_64           0x0060  ///< 64  SPS — 15.6ms/conversion
#define ADS1115_DR_128          0x0080  ///< 128 SPS — 7.8ms/conversion  ← default
#define ADS1115_DR_250          0x00A0  ///< 250 SPS — 4ms/conversion
#define ADS1115_DR_475          0x00C0  ///< 475 SPS — 2.1ms/conversion
#define ADS1115_DR_860          0x00E0  ///< 860 SPS — 1.16ms/conversion

// ============================================================
// Config register — comparator (disabled, ALERT used for RDY only)
// ============================================================
#define ADS1115_COMP_QUE_DISABLE 0x0003 ///< Disable comparator, ALERT/RDY high-Z

// ============================================================
// Sensor types
// ============================================================
/**
 * @brief Sensor type — determines calibration, range validation,
 *        error codes and default gain applied at construction
 */
enum class SensorType {
    PH,   ///< pH measurement,  range 0–14
    RX    ///< RX/ORP measurement, range ±2000mV
};

// ============================================================
// State machine
// ============================================================
/**
 * @brief Measurement state machine states
 */
enum class PHXState {
    IDLE,       ///< Ready for new measurement
    COLLECTING, ///< Gathering ADC samples
    PROCESSING  ///< Computing final reading from samples
};

// ============================================================
// Error codes
// ============================================================
/**
 * @brief Error conditions
 */
enum class PHXError {
    NONE,           ///< No error
    PH_LOW,         ///< pH reading below 0
    PH_HIGH,        ///< pH reading above 14
    RX_LOW,         ///< RX/ORP reading below 0mV (typical pool range)
    RX_HIGH,        ///< RX/ORP reading above 2000mV
    TEMP_INVALID,   ///< Temperature outside valid range (0–50°C)
    CALIB_INVALID   ///< EEPROM version mismatch or no calibration data found
};

// ============================================================
// Calibration data structure
// ============================================================
/**
 * @brief Two-point calibration data
 * pH  defaults: ref1_value=4.0,   ref2_value=7.0
 * RX  defaults: ref1_value=475.0, ref2_value=650.0
 */
struct PHX_Calibration {
    float ref1_mV;      ///< First  calibration point — measured mV
    float ref2_mV;      ///< Second calibration point — measured mV
    float ref1_value;   ///< First  reference value (pH 4.0  or 475mV)
    float ref2_value;   ///< Second reference value (pH 7.0  or 650mV)
};

// ============================================================
// Reading configuration structure
// ============================================================
/**
 * @brief Reading configuration passed to startReading()
 * @note  samples clamped to ADS1115_MAX_SAMPLES (25) automatically
 * @note  avg_buffer clamped to ADS1115_MAX_AVG (10) automatically
 * @note  Sensor type is always taken from the instance (_sensorType),
 *        not from this struct — prevents type/instance mismatch.
 */
struct PHXConfig {
    uint8_t    samples;     ///< Samples per reading cycle (recommended: 10, max: 25)
    uint16_t   delay_ms;    ///< Inter-sample pause in ms, non-blocking via millis()
                            ///< 0 = trigger next conversion immediately after result ready
    uint8_t    avg_buffer;  ///< Rolling average window (1 = off, max: ADS1115_MAX_AVG)
};

// ============================================================
// Base class — ADS1115_PHX
// ============================================================
/**
 * @brief ADS1115 ADC controller for PHX v2 pH and RX/ORP measurements
 *
 * All library logic lives here. Users instantiate ADS1115_PHX_PH
 * or ADS1115_PHX_RX — thin subclasses that set correct defaults.
 */
class ADS1115_PHX {
public:

    /// Named constant for unused alert pin — use instead of -1
    static const int8_t NO_ALERT = -1;

    /**
     * @brief Construct a new ADS1115_PHX instance
     * @param type       Sensor type (SensorType::PH or SensorType::RX)
     * @param i2cAddress I2C address of this ADS1115 (0x48–0x4B)
     * @param alertPin   MCU pin connected to ADS1115 ALERT/RDY (or NO_ALERT)
     * @param eepromBase EEPROM base address for calibration storage
     */
    ADS1115_PHX(SensorType type,
                uint8_t    i2cAddress,
                int8_t     alertPin   = NO_ALERT,
                uint16_t   eepromBase = ADS1115_EEPROM_BASE);

    // ----------------------------------------------------------
    // Initialisation
    // ----------------------------------------------------------

    /**
     * @brief Initialise library, pre-allocate buffers, configure ALERT pin if used
     * @param initWire If true (default), calls Wire.begin() automatically.
     *                 Set false if Wire is already initialised elsewhere
     *                 (e.g. custom clock speed, ESP32 custom pins).
     * Must be called in setup() before any other method.
     */
    void begin(bool initWire = true);

    // ----------------------------------------------------------
    // ADC configuration
    // ----------------------------------------------------------

    /**
     * @brief Set ADC programmable gain
     * @param gain Use ADS1115_GAIN_x defines
     * PHX v2 defaults are set automatically by subclass constructor.
     */
    void setGain(uint16_t gain);

    /**
     * @brief Set ADC data rate (samples per second)
     * @param dataRate Use ADS1115_DR_x defines (8–860 SPS)
     * Default: ADS1115_DR_128 (~7.8ms per conversion)
     * Invalid values are silently ignored, current rate kept.
     */
    void setDataRate(uint16_t dataRate);

    /**
     * @brief Get current gain setting
     * @return Current gain register bits
     */
    uint16_t getGain() const { return _gain; }

    /**
     * @brief Get current data rate setting
     * @return Current data rate register bits
     */
    uint16_t getDataRate() const { return _dataRate; }

    // ----------------------------------------------------------
    // Raw ADC access
    // ----------------------------------------------------------

    /**
     * @brief Perform a single differential ADS1115 conversion (blocking)
     * Uses AIN1(+)/AIN0(−) differential input — PHX v2 standard.
     * Triggers single-shot conversion, polls OS bit until ready.
     * If alertPin was provided to constructor, uses pin state instead.
     * @return Signed 16-bit raw ADC result
     */
    int16_t readADC();

    // ----------------------------------------------------------
    // Non-blocking measurement state machine
    // ----------------------------------------------------------

    /**
     * @brief Start a new measurement sequence (non-blocking)
     * @param config Reading configuration
     * Returns immediately. Call updateReading() repeatedly in loop().
     * Ignored if state is not IDLE.
     */
    void startReading(const PHXConfig& config);

    /**
     * @brief Advance the measurement state machine (non-blocking)
     * Call this repeatedly in loop() after startReading().
     * Transitions: IDLE → COLLECTING → PROCESSING → IDLE
     */
    void updateReading();

    /**
     * @brief Cancel current measurement and reset to IDLE
     */
    void cancelReading();

    // ----------------------------------------------------------
    // Calibration
    // ----------------------------------------------------------

    /**
     * @brief Get a stable calibration reading (blocking)
     * Takes repeated readings until values stabilise within threshold.
     * Temperature compensation is disabled during calibration.
     * @note  Uses delay() internally — this is a privileged user process.
     *        MCU is dedicated to calibration during this call.
     * @return Stable voltage reading in mV
     */
    float calibratePHXReading();

    /**
     * @brief Store two-point calibration data in RAM
     * Validates that ref points are not identical (would cause div-by-zero).
     * @param cal Calibration struct with two reference points
     * @return true if valid and stored, false + CALIB_INVALID if points too close
     */
    bool calibratePHX(const PHX_Calibration& cal);

    /**
     * @brief Save current calibration to EEPROM
     * Writes version byte + both calibration structs.
     * @return true on success
     */
    bool saveCalibration();

    /**
     * @brief Load calibration from EEPROM
     * Checks version byte first. On mismatch sets CALIB_INVALID,
     * keeps factory defaults, returns false.
     * @return true if valid calibration loaded, false if invalid/missing
     */
    bool loadCalibration();

    // ----------------------------------------------------------
    // Guided calibration — easy two-step API
    // ----------------------------------------------------------

    /**
     * @brief Guided calibration — capture first reference point
     * Place probe in first buffer solution, call this function.
     * Blocking — takes ADS1115_CAL_SAMPLES readings until stable.
     * @param knownValue Known value of the buffer (pH unit or mV)
     *                   e.g. 4.0 for pH 4 buffer, 475.0 for 475mV ORP solution
     * @return Measured stable mV at this point (for display/logging)
     * @note Calibration is NOT complete until calibratePoint2() is called.
     */
    float calibratePoint1(float knownValue);

    /**
     * @brief Guided calibration — capture second reference point and finalise
     * Place probe in second buffer solution, call this function.
     * Blocking — takes ADS1115_CAL_SAMPLES readings until stable.
     * Automatically calls calibratePHX() to validate and store to RAM.
     * Does NOT auto-save to EEPROM — call saveCalibration() explicitly.
     * @param knownValue Known value of the buffer (pH unit or mV)
     *                   e.g. 7.0 for pH 7 buffer, 650.0 for 650mV ORP solution
     * @return true if calibration valid and stored in RAM, false if points too close
     */
    bool calibratePoint2(float knownValue);

    // ----------------------------------------------------------
    // Calibration status
    // ----------------------------------------------------------

    /**
     * @brief Check if valid calibration is loaded in RAM
     * @return true if calibration points are set and non-identical
     */
    bool isCalibrated() const;

    /**
     * @brief Get current calibration data (read-back)
     * Useful for display in setup menus or diagnostic logging.
     * @return Copy of current PHX_Calibration struct
     */
    PHX_Calibration getCalibration() const { return _cal; }

    // ----------------------------------------------------------
    // Temperature compensation
    // ----------------------------------------------------------

    /**
     * @brief Enable or disable temperature compensation (pH only)
     * Uses Pasco 2001 formula. Disabled by default.
     * Has no effect on RX measurements.
     */
    void enableTemperatureCompensation(bool enabled);

    /**
     * @brief Set current water temperature for compensation
     * @param tempC Temperature in Celsius (valid: 0–50°C)
     * Out-of-range → sets PHXError::TEMP_INVALID, temperature unchanged
     */
    void setTemperature(float tempC);

    /**
     * @brief Get current temperature setting
     */
    float getCurrentTemperature() const { return _temperature; }

    /**
     * @brief Check if temperature compensation is enabled
     */
    bool isTemperatureCompensationEnabled() const { return _tempCompEnabled; }

    // ----------------------------------------------------------
    // Rolling average
    // ----------------------------------------------------------

    /**
     * @brief Set rolling average window size
     * @param n Window size 1–ADS1115_MAX_AVG (1 = passthrough, off)
     * Off by default. Each completed reading is pushed into the ring.
     */
    void setRollingAverage(uint8_t n);

    /**
     * @brief Reset the rolling average ring buffer
     * Clears stored values but keeps the current window setting.
     * Call after calibration or probe swap to flush stale values.
     */
    void clearRollingAverage();

    /**
     * @brief Check if rolling average ring is full
     * @return true once the ring has been filled for the first time
     */
    bool isRollingAverageReady() const { return _avgReady; }

    // ----------------------------------------------------------
    // Status getters
    // ----------------------------------------------------------

    PHXState getState()           const { return _state; }
    bool     isReadingComplete()  const { return _readingComplete; }
    PHXError getLastError()       const { return _lastError; }

    /**
     * @brief Get last completed reading
     * Returns rolling average if enabled, raw reading otherwise.
     */
    float getLastReading() const { return _lastReading; }

    /**
     * @brief Get last raw averaged mV — before calibration and ring average
     * Useful for displaying single-reading vs rolling-average side by side
     * without disrupting the ring buffer by alternating avg_buffer values.
     */
    float getLastRawMV() const { return _lastRawMV; }

    /**
     * @brief Get sensor type for this instance
     */
    SensorType getSensorType() const { return _sensorType; }

    /**
     * @brief Get full-scale voltage range for current gain setting
     * Useful for manual mV conversion in test sketches:
     * mV = (raw * getVoltageRange() / 32768.0f) * 1000.0f
     * @return Voltage range in Volts (e.g. 2.048f for Gain 2, 4.096f for Gain 1)
     */
    float getVoltageRange() const;

    // ----------------------------------------------------------
    // Message callback — output device abstraction
    // ----------------------------------------------------------

    /// Callback type for user-facing library messages.
    /// Receives flash string pointer (F() macro) — max 20 characters.
    /// Compatible with Serial.println(msg), lcd.print(msg), etc.
    typedef void (*MessageCallback)(const __FlashStringHelper*);

    /**
     * @brief Register a callback for user-facing library messages
     * If not set (default), library is completely silent.
     * Called for calibration progress messages only — not debug output.
     * @param callback Function pointer: void myFunc(const __FlashStringHelper*)
     *
     * Serial example:
     *   void onMsg(const __FlashStringHelper* m) { Serial.println(m); }
     *   sensor.setMessageCallback(onMsg);
     *
     * LCD example:
     *   void onMsg(const __FlashStringHelper* m) { lcd.setCursor(0,3); lcd.print(m); }
     *   sensor.setMessageCallback(onMsg);
     */
    void setMessageCallback(MessageCallback callback);

    // ----------------------------------------------------------
    // Debug system — runtime control
    // ----------------------------------------------------------

    /**
     * @brief Enable or disable debug output at runtime
     * Requires ADS1115_DEBUG to be defined at compile time.
     */
    void enableDebug(bool enabled);

    /**
     * @brief Redirect debug output to any Stream (default: Serial)
     * @param stream Reference to any Stream-derived object
     */
    void setDebugStream(Stream& stream);

protected:

    // Sensor identity
    SensorType  _sensorType;
    uint8_t     _i2cAddress;
    int8_t      _alertPin;
    uint16_t    _eepromBase;

    // ADC config
    uint16_t    _gain;
    uint16_t    _dataRate;

    // State machine
    PHXState    _state           = PHXState::IDLE;
    PHXError    _lastError       = PHXError::NONE;
    bool        _readingComplete  = false;
    float       _lastReading     = 0.0f;
    float       _lastRawMV       = 0.0f; ///< Raw averaged mV before calibration
                                          ///< Used by calibratePHXReading() to get
                                          ///< probe mV independently of cal state

    // Fixed sample buffer — pre-allocated in begin(), zero heap after
    float       _samples[ADS1115_MAX_SAMPLES];
    uint8_t     _sampleIndex     = 0;     ///< Samples collected so far in current cycle
    bool        _waitingForDelay = false;  ///< true = in inter-sample delay period
    unsigned long _lastSampleTime = 0;    ///< millis() timestamp of last sample read

    // Rolling average ring buffer — pre-allocated in begin()
    float       _avgRing[ADS1115_MAX_AVG];
    uint8_t     _avgWindow      = 1;      ///< 1 = off (passthrough)
    uint8_t     _avgIndex       = 0;
    uint8_t     _avgFilled      = 0;      ///< How many slots filled so far
    bool        _avgReady       = false;  ///< True once ring is full for first time

    // Calibration
    PHX_Calibration _cal;
    PHXConfig       _config;

    // Temperature compensation
    bool        _tempCompEnabled = false;
    float       _temperature     = 25.0f; ///< Default 25°C

    // Message callback — output device abstraction
    MessageCallback _messageCallback = nullptr;

    // Debug
    bool        _debugEnabled   = false;
    Stream*     _debugStream    = nullptr;

    // ----------------------------------------------------------
    // Internal helpers
    // ----------------------------------------------------------

    /**
     * @brief Send user-facing message via callback (if registered)
     * No-op if setMessageCallback() was not called.
     */
    void     _notify(const __FlashStringHelper* msg);

    /**
     * @brief Write 16-bit value to ADS1115 register
     */
    void     writeRegister(uint8_t reg, uint16_t value);

    /**
     * @brief Read 16-bit value from ADS1115 register
     */
    uint16_t readRegister(uint8_t reg);

    /**
     * @brief Poll OS bit until conversion ready or timeout (blocking loop)
     * Used by public readADC() only — state machine uses _isConversionReady()
     * @param timeoutMs Maximum wait time in milliseconds
     * @return true if ready, false if timed out
     */
    bool     waitForConversion(uint16_t timeoutMs = 500);

    /**
     * @brief Trigger a single-shot conversion — returns immediately (non-blocking)
     * Writes config register and performs one dummy read for OS-bit settling.
     * State machine calls this to start each sample.
     */
    void     _triggerConversion();

    /**
     * @brief Check if conversion is complete — single check, non-blocking
     * Reads config register once, checks OS bit.
     * @return true if conversion complete, false if still busy
     */
    bool     _isConversionReady();

    /**
     * @brief Read raw conversion result from ADC register
     * Call only after _isConversionReady() returns true.
     * @return Signed 16-bit raw ADC result
     */
    int16_t  _readResult();

    /**
     * @brief Process collected samples into final reading
     * Averages samples, applies calibration, temp compensation,
     * range validation and pushes to rolling average.
     * Called internally when COLLECTING → PROCESSING transition occurs.
     */
    void     _processReading();



    /**
     * @brief Apply Pasco 2001 temperature compensation formula
     * pH_comp = ((pH_raw − 7) × (273.15 + T)) / (273.15 + 25) + 7
     * @param pHRaw Raw pH before compensation
     * @param tempC Current temperature in Celsius
     * @return Temperature-compensated pH normalised to 25°C
     */
    float    applyTempCompensation(float pHRaw, float tempC) const;

    /**
     * @brief Validate temperature is within 0–50°C
     */
    bool     isValidTemperature(float tempC) const;

    /**
     * @brief Push a new value into the rolling average ring
     * Updates _lastReading with the new ring average.
     */
    void     pushToAvgRing(float value);

    /**
     * @brief Apply range validation and set error flags
     * Clamps reading to valid range, sets _lastError accordingly.
     */
    void     validateRange(float& value);
};

// ============================================================
// Subclass — ADS1115_PHX_PH
// pH sensor, I2C 0x49, Gain 1 (±4.096V), EEPROM base 128
// ============================================================
/**
 * @brief pH sensor subclass with PHX v2 optimised defaults
 *
 * Defaults applied automatically:
 * - Gain 2 (±2.048V) — 62.5μV LSB, ~947 counts/pH unit
 * - EEPROM base address 128
 * - SensorType::PH
 * Note: Gain 1 required — AIN0 at 2.5V offset exceeds Gain 2 absolute input range.
 *
 * @code
 * ADS1115_PHX_PH phSensor(0x49);                          // simplest
 * ADS1115_PHX_PH phSensor(0x49, alertPin);                // with alert
 * ADS1115_PHX_PH phSensor(0x49, ADS1115_PHX::NO_ALERT, 128); // full
 * @endcode
 */
class ADS1115_PHX_PH : public ADS1115_PHX {
public:
    /**
     * @param i2cAddress I2C address (default 0x49 on PHX v2 pH board)
     * @param alertPin   MCU pin for ALERT/RDY (or ADS1115_PHX::NO_ALERT)
     * @param eepromBase EEPROM base address for calibration (default 128)
     */
    ADS1115_PHX_PH(uint8_t  i2cAddress = ADS1115_ADDR_49,
                   int8_t   alertPin   = ADS1115_PHX::NO_ALERT,
                   uint16_t eepromBase = 128)
        : ADS1115_PHX(SensorType::PH, i2cAddress, alertPin, eepromBase)
    {
        // Gain 2 (±2.048V) — pH differential signal ±~500mV fits this range.
        // ADS1115 differential input measures AIN0−AIN1, not absolute voltage,
        // so the 2.5V common-mode offset on both inputs cancels out.
        // LSB = 62.5μV, ~947 counts/pH unit — target ±0.002pH met.
        _gain     = ADS1115_GAIN_2;    // ±2.048V — pH default
        _cal      = {0.0f, 0.0f, 4.0f, 7.0f}; // pH 4 + pH 7 factory defaults
    }
};

// ============================================================
// Subclass — ADS1115_PHX_RX
// RX/ORP sensor, I2C 0x48, Gain 1 (±4.096V), EEPROM base 161
// ============================================================
/**
 * @brief RX/ORP sensor subclass with PHX v2 optimised defaults
 *
 * Defaults applied automatically:
 * - Gain 1 (±4.096V) — 125μV LSB, ~16 counts/mV
 * - EEPROM base address 161
 * - SensorType::RX
 *
 * @code
 * ADS1115_PHX_RX rxSensor(0x48);                          // simplest
 * ADS1115_PHX_RX rxSensor(0x48, alertPin);                // with alert
 * ADS1115_PHX_RX rxSensor(0x48, ADS1115_PHX::NO_ALERT, 161); // full
 * @endcode
 */
class ADS1115_PHX_RX : public ADS1115_PHX {
public:
    /**
     * @param i2cAddress I2C address (default 0x48 on PHX v2 RX board)
     * @param alertPin   MCU pin for ALERT/RDY (or ADS1115_PHX::NO_ALERT)
     * @param eepromBase EEPROM base address for calibration (default 161)
     */
    ADS1115_PHX_RX(uint8_t  i2cAddress = ADS1115_ADDR_48,
                   int8_t   alertPin   = ADS1115_PHX::NO_ALERT,
                   uint16_t eepromBase = 161)
        : ADS1115_PHX(SensorType::RX, i2cAddress, alertPin, eepromBase)
    {
        _gain     = ADS1115_GAIN_1;    // ±4.096V — RX default
        _cal      = {0.0f, 0.0f, 475.0f, 650.0f}; // 475mV + 650mV factory defaults
    }
};

#endif // APAPHX2_ADS1115_H
