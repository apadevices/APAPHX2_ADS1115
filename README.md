# APAPHX2_ADS1115

**Arduino library for APADevices PHX v2 pool monitoring board**  
pH and ORP/RX measurement using the ADS1115 16-bit ADC with full isolation, two-point calibration, temperature compensation and non-blocking state machine design.

---

## Contents

- [What this library does](#what-this-library-does)
- [Hardware overview](#hardware-overview)
- [Wiring](#wiring)
- [Installation](#installation)
- [Quick start](#quick-start)
- [Calibration](#calibration)
- [API reference](#api-reference)
- [Debug system](#debug-system)
- [Message callback](#message-callback)
- [Platform notes](#platform-notes)
- [Examples](#examples)
- [Board quick spec](#board-quick-spec)
- [License](#license)

---

## What this library does

The APADevices PHX v2 board measures two water quality parameters simultaneously:

| Channel | Measurement | Range | Accuracy | ADC gain |
|---------|-------------|-------|----------|----------|
| pH | Acid/base balance | 0 – 14 pH | ±0.002 pH | Gain 2 (±2.048V, 62.5μV LSB) |
| ORP/RX | Oxidation-reduction potential | ±2000 mV | ±0.5 mV | Gain 1 (±4.096V, 125μV LSB) |

Both probes connect to isolated, precision analog frontends on the PHX v2 board. The ADS1115 16-bit delta-sigma ADC reads each channel differentially — the library handles all register configuration, timing, calibration and unit conversion.

**Key features:**
- Non-blocking state machine — measurement never blocks `loop()`
- Two-point calibration with EEPROM persistence — survives power cycles
- Temperature compensation for pH (Pasco 2001 formula, normalised to 25°C)
- Rolling average filter — configurable window 1–10 readings
- Message callback — route library messages to Serial, LCD or any display
- Compile-time + runtime debug system
- Zero heap allocation after `begin()` — safe on AVR

---

## Hardware overview

The PHX v2 board contains **two fully isolated measurement modules** on a single PCB:

- **pH module** — ADS1115 at I2C address `0x49`, Gain 2
- **ORP/RX module** — ADS1115 at I2C address `0x48`, Gain 1

Each module has its own ADUM1251 I2C isolator and LMP7721 precision op-amp frontend. Both share the same I2C bus on the MCU side but are electrically isolated from each other and from the MCU on the analog side.

### Power domains

The board has **two separate power domains** that must both be connected:

```
┌─────────────────────────────────────────────────────────────┐
│  MCU side (logic)          │  Analog side (isolated)        │
│  Connector P1              │  Connector CN2                 │
│                            │                                │
│  Vcc-MC1: 3.3V or 5V      │  +12V IN: external 12V DC     │
│  GND-MC1: MCU ground       │  12V/GND: 12V ground          │
│  SCL, SDA: I2C bus         │                                │
│  (same voltage as MCU)     │  Powers all analog circuits    │
│                            │  behind ADUM1251 isolators     │
└─────────────────────────────────────────────────────────────┘
```

> ⚠️ **Both power connections are required.** The board will not function with only one supply connected.

> ⚠️ **The two grounds (GND-MC1 and 12V/GND) are galvanically isolated.** Do not connect them together.

---

## Wiring

### Connector P1 — IDC 2×5 pin header (2.54mm pitch)

```
P1 Pin  │ Signal      │ Connect to
────────┼─────────────┼──────────────────────────────────────
  10    │ GND-MC1     │ MCU GND
   9    │ Vcc-MC1     │ MCU supply (3.3V or 5V — match MCU)
   8    │ SCL-MC1     │ MCU I2C SCL
   7    │ SDA-MC1     │ MCU I2C SDA
   6    │ RX-Alert    │ MCU pin (optional — ALERT/RDY from ORP ADS1115)
   5    │ pH-Alert    │ MCU pin (optional — ALERT/RDY from pH ADS1115)
   4    │ 12V GND     │ 12V supply negative (tied to pin 2)
   3    │ 12V+        │ 12V supply positive (tied to pin 1)
   2    │ 12V GND     │ 12V supply negative (tied to pin 4)
   1    │ 12V+        │ 12V supply positive (tied to pin 3)
```

Pins 1+3 and 2+4 are paired — connect both pins of each pair to the 12V supply.

### Connector CN2 — KF2510 2-pin (alternative 12V connection)

```
CN2 Pin  │ Signal   │ Connect to
─────────┼──────────┼──────────────────
   1     │ 12V+     │ 12V supply positive
   2     │ 12V/GND  │ 12V supply negative
```

Use either P1 (pins 1–4) or CN2 for the 12V supply — both connect to the same internal rail.

### I2C pullup jumper H3

The board includes onboard 4.7kΩ I2C pullup resistors (R36, R37).  
Enable them with jumper H3 if your MCU board does not already have I2C pullups.  
Disable H3 if external pullups are present to avoid parallel resistance lowering the pullup value.

### Minimal wiring example (Arduino Uno)

```
Arduino Uno → PHX v2 P1
─────────────────────────
GND         → Pin 10 (GND-MC1)
5V          → Pin 9  (Vcc-MC1)
A5 (SCL)    → Pin 8  (SCL-MC1)
A4 (SDA)    → Pin 7  (SDA-MC1)

External PSU → PHX v2 CN2
─────────────────────────
12V+        → Pin 1
GND         → Pin 2
```

H3 jumper: **ON** (enable onboard pullups, unless your Uno shield has them)

---

## Installation

### Arduino IDE (Library Manager)
1. Open **Sketch → Include Library → Manage Libraries**
2. Search for `APAPHX2_ADS1115`
3. Click **Install**

### Arduino IDE (manual)
1. Download the repository as ZIP from [GitHub](https://github.com/apadevices/APAPHX2_ADS1115)
2. Open **Sketch → Include Library → Add .ZIP Library**
3. Select the downloaded ZIP

### PlatformIO
```ini
; platformio.ini
lib_deps =
    https://github.com/apadevices/APAPHX2_ADS1115
```

---

## Quick start

```cpp
#include "APAPHX2_ADS1115.h"

ADS1115_PHX_PH phSensor(0x49);   // pH  — I2C address 0x49
ADS1115_PHX_RX rxSensor(0x48);   // ORP — I2C address 0x48

void setup() {
    Serial.begin(115200);
    phSensor.begin();
    rxSensor.begin();

    if (!phSensor.isCalibrated()) {
        Serial.println("pH not calibrated — run calibration first");
    }
    if (!rxSensor.isCalibrated()) {
        Serial.println("ORP not calibrated — run calibration first");
    }
}

void loop() {
    // Non-blocking measurement
    static PHXConfig cfg;
    static bool cfgReady = false;
    if (!cfgReady) {
        cfg.samples    = 10;
        cfg.delay_ms   = 5;
        cfg.avg_buffer = 1;
        cfgReady = true;
    }

    phSensor.startReading(cfg);
    while (phSensor.getState() != PHXState::IDLE) {
        phSensor.updateReading();
    }

    rxSensor.startReading(cfg);
    while (rxSensor.getState() != PHXState::IDLE) {
        rxSensor.updateReading();
    }

    Serial.print("pH: ");
    Serial.print(phSensor.getLastReading(), 3);
    Serial.print("  ORP: ");
    Serial.print(rxSensor.getLastReading(), 1);
    Serial.println(" mV");

    delay(2000);
}
```

> **Note:** `begin()` automatically loads calibration from EEPROM. If no valid calibration is stored, readings return raw mV until calibration is performed.

---

## Calibration

Calibration is the most important step for accurate measurements. The library uses **two-point calibration** — two known reference solutions are measured and the library maps all subsequent readings between them.

### Why calibration matters

Electrochemical probes do not produce a universal voltage for a given pH or ORP value. Each probe has a unique offset and slope that depends on its age, condition and temperature. Without calibration the library returns raw differential mV from the ADC — useful for diagnostics but not for chemistry.

### What you need

**pH calibration:**
- pH 4.0 buffer solution (liquid — avoid powder, insufficient precision)
- pH 7.0 buffer solution (liquid)
- Distilled or deionised water for rinsing

**ORP/RX calibration:**
- 475 mV ORP reference solution
- 650 mV ORP reference solution

Any commercially available liquid standard buffer solution works. No specific brand is required.

### Calibration process — guided API (recommended)

The library provides a simple two-step guided calibration. Each step blocks for approximately 200 seconds while the probe equilibrates in the buffer solution — this wait is mandatory for accurate results.

```
┌─────────────────────────────────────────────────────────────┐
│  Calibration timeline (per point)                          │
│                                                            │
│  Place probe → Press enter → [200s soak] → [stable check] │
│                              ↑                             │
│  "Cal: wait 200s..."         │ probe equilibrates          │
│                              ↓                             │
│                         "Calibration: stable!"             │
│                         Result stored in RAM               │
└─────────────────────────────────────────────────────────────┘
```

```cpp
#include "APAPHX2_ADS1115.h"

ADS1115_PHX_PH phSensor(0x49);

// Route library messages to Serial
void onMessage(const __FlashStringHelper* m) { Serial.println(m); }

void setup() {
    Serial.begin(115200);
    phSensor.begin();
    phSensor.setMessageCallback(onMessage);

    // ── STEP 1: First buffer solution ───────────────────────
    Serial.println("Rinse probe with distilled water.");
    Serial.println("Place probe in pH 4.0 buffer. Press ENTER when ready.");
    waitForEnter();

    float mV1 = phSensor.calibratePoint1(4.0f);
    Serial.print("Point 1 captured: "); Serial.print(mV1, 3);
    Serial.println(" mV");

    // ── STEP 2: Second buffer solution ──────────────────────
    Serial.println("Rinse probe with distilled water.");
    Serial.println("Place probe in pH 7.0 buffer. Press ENTER when ready.");
    waitForEnter();

    bool ok = phSensor.calibratePoint2(7.0f);
    if (ok) {
        phSensor.saveCalibration();   // persist to EEPROM
        Serial.println("Calibration saved.");
    } else {
        Serial.println("Calibration failed — check buffer solutions.");
    }
}

void waitForEnter() {
    while (Serial.available()) Serial.read();
    while (!Serial.available()) { ; }
    while (Serial.available()) Serial.read();
}

void loop() { }
```

### Calibration process — advanced API

For custom workflows, each step can be called individually:

```cpp
// Capture stable mV reading (blocking — ~200s minimum)
float mV = phSensor.calibratePHXReading();

// Build calibration struct manually
PHX_Calibration cal;
cal.ref1_mV    = mV_at_buffer1;
cal.ref1_value = 4.0f;
cal.ref2_mV    = mV_at_buffer2;
cal.ref2_value = 7.0f;

// Validate and store to RAM
bool ok = phSensor.calibratePHX(cal);

// Persist to EEPROM (call explicitly — not automatic)
if (ok) phSensor.saveCalibration();
```

### Important calibration notes

- **The 200-second soak is mandatory.** pH electrodes need 60–180 seconds to equilibrate in a new buffer solution. Starting the stability check too early produces a wrong calibration that looks valid but gives incorrect readings.
- **Calibration is NOT auto-saved.** After `calibratePoint2()` succeeds, you must call `saveCalibration()` explicitly. This is by design — it lets you verify the calibration before committing to EEPROM.
- **Auto-load on power-up.** `begin()` automatically loads the last saved calibration from EEPROM. If no valid calibration is found, `isCalibrated()` returns false and readings return raw mV.
- **Recalibrate after sensor change.** Any probe replacement requires fresh calibration. The saved EEPROM data is specific to each probe.
- **ORP calibration follows the same process** — use `ADS1115_PHX_RX` instance with 475 mV and 650 mV reference solutions.

### Calibration status check

```cpp
phSensor.begin();

if (phSensor.isCalibrated()) {
    PHX_Calibration cal = phSensor.getCalibration();
    Serial.print("ref1: "); Serial.print(cal.ref1_mV);
    Serial.print(" mV → "); Serial.println(cal.ref1_value);
    Serial.print("ref2: "); Serial.print(cal.ref2_mV);
    Serial.print(" mV → "); Serial.println(cal.ref2_value);
} else {
    Serial.println("No calibration found.");
}
```

### EEPROM layout

Calibration data is stored at fixed addresses:

| Instance | Base address | Occupies |
|----------|-------------|---------|
| pH (`ADS1115_PHX_PH`) | 128 | 128–144 |
| ORP (`ADS1115_PHX_RX`) | 161 | 161–177 |

Custom base addresses can be set via the constructor. Minimum EEPROM size required: 178 bytes (all AVR devices with EEPROM satisfy this).

---

## API reference

### Constructors

```cpp
// pH sensor — default address 0x49, EEPROM base 128
ADS1115_PHX_PH phSensor(0x49);

// ORP/RX sensor — default address 0x48, EEPROM base 161
ADS1115_PHX_RX rxSensor(0x48);

// With ALERT pin
ADS1115_PHX_PH phSensor(0x49, A0);

// With custom EEPROM base (advanced — multiple boards)
ADS1115_PHX_PH phSensor(0x49, ADS1115_PHX::NO_ALERT, 200);
```

### Initialisation

```cpp
sensor.begin();              // init I2C, load calibration from EEPROM
sensor.begin(false);         // skip Wire.begin() — if Wire already started
```

### Non-blocking state machine

```cpp
PHXConfig cfg;
cfg.samples    = 10;   // ADC samples per cycle (1–25)
cfg.delay_ms   = 5;    // pause between samples in ms (0 = immediate)
cfg.avg_buffer = 1;    // rolling average window (1 = off, 2–10 = active)

sensor.startReading(cfg);              // begin cycle
sensor.updateReading();                // call repeatedly in loop()
sensor.cancelReading();                // abort current cycle

PHXState s = sensor.getState();        // IDLE / COLLECTING / PROCESSING
bool done  = sensor.isReadingComplete();
float val  = sensor.getLastReading();  // calibrated pH or ORP mV
float raw  = sensor.getLastRawMV();    // raw differential mV (pre-calibration)
```

### Calibration

```cpp
// Guided — recommended
float mV  = sensor.calibratePoint1(4.0f);   // step 1: first buffer
bool  ok  = sensor.calibratePoint2(7.0f);   // step 2: second buffer + finalise

// Low-level
float mV  = sensor.calibratePHXReading();   // blocking stable reading
bool  ok  = sensor.calibratePHX(cal);       // validate + store to RAM
bool  ok  = sensor.saveCalibration();        // write to EEPROM
bool  ok  = sensor.loadCalibration();        // read from EEPROM

// Status
bool            cal = sensor.isCalibrated();
PHX_Calibration cal = sensor.getCalibration();
```

### Temperature compensation (pH only)

```cpp
sensor.enableTemperatureCompensation(true);
sensor.setTemperature(25.0f);          // °C, valid range 0–50°C
float t = sensor.getCurrentTemperature();
bool  e = sensor.isTemperatureCompensationEnabled();
```

### Rolling average

```cpp
sensor.setRollingAverage(5);           // window size 2–10 (1 = off)
sensor.clearRollingAverage();          // reset ring, keep window
bool ready = sensor.isRollingAverageReady();  // true once ring is full
```

### ADC configuration

```cpp
sensor.setGain(ADS1115_GAIN_1);        // default: GAIN_2 for pH, GAIN_1 for ORP
sensor.setDataRate(ADS1115_DR_128);    // default: 128 SPS (~7.8ms/conversion)
int16_t raw = sensor.readADC();        // single blocking read
float range = sensor.getVoltageRange(); // ±V for current gain
```

### Errors

```cpp
PHXError e = sensor.getLastError();
// PHXError::NONE / PH_LOW / PH_HIGH / RX_LOW / RX_HIGH
// TEMP_INVALID / CALIB_INVALID
```

---

## Debug system

The debug system is compile-time gated — it adds zero overhead when disabled.

### Enable debug output

Uncomment the `#define` in `APAPHX2_ADS1115.h`:

```cpp
// In APAPHX2_ADS1115.h:
#define ADS1115_DEBUG   // <-- uncomment to enable debug output
```

Then enable at runtime and optionally redirect the output stream:

```cpp
phSensor.enableDebug(true);
phSensor.setDebugStream(Serial);    // default — any Stream works
                                     // e.g. Serial1, SoftwareSerial
```

When enabled, the library prints detailed diagnostics for every operation — I2C register writes, sample-by-sample ADC values, calibration windows, EEPROM operations and rolling average state. This is intended for Serial Monitor use during development.

> **Note:** Debug output is Stream-based (Serial, UART). It is verbose and not suitable for LCD display. For user-facing messages on LCD, use `setMessageCallback()` instead.

> **Note:** `#define ADS1115_DEBUG` must be set at compile time. The runtime `enableDebug()` toggle only works when the define is active.

---

## Message callback

The library emits three short user-facing messages during calibration. By default the library is completely silent. Register a callback to receive these messages and route them to any output device.

**All messages are ≤ 20 characters — designed to fit one row of a 20×4 LCD.**

| Message | When |
|---------|------|
| `"Cal: wait 200s..."` | Probe soak started |
| `"Calibration: stable!"` | Stable reading captured |
| `"Cal: timeout-check! "` | Stability timeout — check probe |

### Serial example

```cpp
void onMessage(const __FlashStringHelper* m) {
    Serial.println(m);
}
phSensor.setMessageCallback(onMessage);
rxSensor.setMessageCallback(onMessage);
```

### LCD example (LiquidCrystal_I2C)

```cpp
LiquidCrystal_I2C lcd(0x27, 20, 4);

void onMessage(const __FlashStringHelper* m) {
    lcd.setCursor(0, 3);                       // bottom row
    lcd.print(F("                    "));       // clear row
    lcd.setCursor(0, 3);
    lcd.print(m);
}
phSensor.setMessageCallback(onMessage);
```

### Important notes

- The callback receives library **status messages only** — calibration progress, stable confirmation, timeout warning.
- **User interaction prompts** ("place probe in buffer", "press button") are always the responsibility of your sketch, not the library. The library has no knowledge of your input method (Serial, button, touchscreen).
- If `setMessageCallback()` is never called, the library is completely silent — no `Serial.begin()` required.

---

## Platform notes

### AVR (Arduino Uno, Nano, Mega)

Fully supported. Zero heap allocation after `begin()`. Tested on Arduino Mega 2560.

Minimum recommended board: **Arduino Uno / Nano** (32KB flash, 2KB SRAM).  
The library uses ~500 bytes SRAM at runtime (fixed buffers, no heap).

```cpp
#include <Wire.h>
#include "APAPHX2_ADS1115.h"
// Wire runs at 100kHz by default on AVR — no changes needed
```

### ESP8266 / ESP32

Supported. The calibration soak loop includes `yield()` calls to feed the watchdog.

```cpp
#include <Wire.h>
#include "APAPHX2_ADS1115.h"
// Wire.begin(SDA_PIN, SCL_PIN) if using non-default pins
```

### STM32 (STM32duino core)

Supported with STM32duino Arduino core.

```cpp
#include <Wire.h>
#include "APAPHX2_ADS1115.h"
// Wire.setSDA(PB7); Wire.setSCL(PB6);  // if needed for your board
// Wire.begin();
```

### I2C speed

The ADUM1251 isolator supports both Standard-mode (100kHz) and Fast-mode (400kHz).  
The library defaults to 100kHz which is sufficient for all PHX measurement needs.  
To enable 400kHz:

```cpp
Wire.begin();
Wire.setClock(400000);
phSensor.begin(false);   // false = skip Wire.begin() inside library
rxSensor.begin(false);
```

---

## Examples

| # | Name | Class | What it demonstrates |
|---|------|-------|----------------------|
| 01 | `BasicReading` | Simple | `readADC()`, `begin()`, direct blocking read |
| 02 | `StateMachine` | Simple | Non-blocking `startReading()` / `updateReading()` |
| 03 | `DualSensor` | Simple | pH + ORP together, continuous loop pattern |
| 04 | `Calibration` | Middle | Full guided calibration with Serial prompts |
| 05 | `TemperatureComp` | Middle | Temperature compensation for pH |
| 06 | `RollingAverage` | Middle | Rolling average filter, warm-up, window sizing |
| 07 | `DebugOutput` | Advanced | Debug system, `setDebugStream()`, runtime toggle |
| 08 | `LCDDisplay` | Advanced | `setMessageCallback()` with LCD 20×4 |
| 09 | `PoolMonitor` | Advanced | Complete pool automation sketch |

All examples are in the `examples/` folder, organised by class:
```
examples/
  simple/
    01_BasicReading/
    02_StateMachine/
    03_DualSensor/
  middle/
    04_Calibration/
    05_TemperatureComp/
    06_RollingAverage/
  advanced/
    07_DebugOutput/
    08_LCDDisplay/
    09_PoolMonitor/
```

---

## Board quick spec

| Parameter | Value |
|-----------|-------|
| pH ADC | ADS1115, 16-bit, I2C 0x49 |
| ORP ADC | ADS1115, 16-bit, I2C 0x48 |
| pH frontend | LMP7721 precision op-amp |
| ORP frontend | LMP7721 precision op-amp |
| Isolation | ADUM1251 I2C isolator (per channel) |
| MCU supply | 3.3V or 5V (match MCU logic level) |
| Analog supply | 12V DC external |
| Max analog supply | 16V (PolyPTC fuse rated) |
| I2C speed | 100kHz standard, 400kHz fast-mode supported |
| pH gain | Gain 2 (±2.048V), LSB = 62.5μV |
| ORP gain | Gain 1 (±4.096V), LSB = 125μV |
| pH resolution | ~947 counts/pH unit |
| ORP resolution | ~8 counts/mV |
| pH accuracy | ±0.002 pH (calibrated, probe settled) |
| ORP accuracy | ±0.5 mV (calibrated, probe settled) |
| Electrode slope check | 40–70 mV/pH accepted (>95% Nernst = excellent) |
| EEPROM usage | 17 bytes per sensor (pH: 128–144, ORP: 161–177) |
| Onboard I2C pullups | 4.7kΩ × 2, switchable via H3 jumper |
| Reverse polarity protection | Schottky diode on 12V rail |
| Overcurrent protection | PolyPTC fuse 500mA |
| Transient protection | TVS 15V/24.4V on 12V rail |

---

## License

MIT License — see [LICENSE](LICENSE) file.

Copyright © 2025 APADevices [@kecup]

---

## Poznámky pro vývojáře (CZ)

Tato sekce je určena primárně pro interní potřebu APADevices.

**Architektura knihovny:**  
Základní třída `ADS1115_PHX` obsahuje veškerou logiku. Podtřídy `ADS1115_PHX_PH` a `ADS1115_PHX_RX` nastavují pouze výchozí zesílení a typ senzoru — žádná duplikace kódu.

**Kalibrační proces:**  
Funkce `calibratePHXReading()` provede povinný 200sekundový stabilizační čas (soak) a pak tříokenní kontrolu stability (okna A, B, C — každé ~890ms). Všechna tři okna musí souhlasit do 0.5mV. Časový limit je 360 sekund. Pokud probe nestabilizuje, vrátí nejlepší dostupný výsledek s chybou `CALIB_INVALID`.

**Proč Gain 2 pro pH a Gain 1 pro ORP:**  
ADS1115 měří diferenciálně (AIN0−AIN1), nikoli absolutně. Společný ofset 2.5V na obou vstupech se odečte. Signál pH elektrody je ±~500mV — vejde se do ±2.048V rozsahu Gain 2 s lepším rozlišením (62.5μV LSB). ORP elektroda ±2000mV do Gain 2 nevejde — vyžaduje Gain 1 (125μV LSB).

**EEPROM:**  
Verze byte 0xA2. pH na adrese 128, ORP na adrese 161. Každý sensor zabírá 17 bytů (1 verze + 16 bytů struct `PHX_Calibration`). `EEPROM.update()` — wear levelling, zapisuje jen změněné byty.

**Zprávy callbacku:**  
Všechny zprávy jsou ≤20 znaků pro kompatibilitu s LCD 20×4. Callback dostává `const __FlashStringHelper*` — nulová spotřeba SRAM pro řetězce.

**Debug systém:**  
`#define ADS1115_DEBUG` musí být v hlavičkovém souboru nebo v build flags. Makra jsou compile-time gated — bez define nulový overhead. Runtime `enableDebug()` přepíná jen pokud je define aktivní.
