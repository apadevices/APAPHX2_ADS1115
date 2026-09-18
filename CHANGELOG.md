# Changelog

All notable changes to APAPHX2_ADS1115 are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [1.2.0] — 2026-09-18

### Added
- `setTickCallback(TickCallback)` — registers a callback fired periodically during
  `calibratePHXReading()`'s blocking waits: once per second during the mandatory
  200-second soak, and once per round of the stability-check loop. Purely
  additive — default is `nullptr` (no-op), so calibration timing, sample counts,
  and the stability algorithm are byte-for-byte unchanged for any caller who
  doesn't opt in. Intended for feeding an external hardware watchdog, or any
  other lightweight periodic housekeeping that needs to happen during the
  multi-minute wait that `setMessageCallback()`'s two milestone messages
  (start of soak, stable/timeout) are too infrequent to support.
  Real-world motivation: an external 555-timer watchdog circuit (no software
  visibility into calibration's internal state) needs feeding roughly every
  60-70 seconds or it resets the MCU — impossible without this hook, since the
  soak's 200 seconds and the stability loop's up-to-360-second timeout both
  fully block with no prior escape point. Verified on real Mega 2560 + PHX v2
  hardware: LED-blink + Serial-dot test confirmed the tick fires as expected
  throughout both phases, with calibration itself unaffected.

### Verified
- Full calibration run (both pH and ORP, both points, EEPROM save + reload)
  confirmed unaffected on real hardware — same timing, same prompts, same
  results as before this change, with the tick callback registered.

---

## [1.1.2] — 2026-04-18

### Fixed
- `09_PoolMonitor` example: parallel sensor state machine race condition.
  The original `if (!phRunning)` guard reset `phDone = false` on every
  `loop()` iteration immediately after each sensor completed, so the report
  gate condition `phDone && rxDone` was never simultaneously true when the
  10-second window fired — causing the sketch to produce no output.
  Both sensors now started once in `setup()` and restarted explicitly only
  after the report fires. True parallel operation confirmed — each sensor
  instance has fully independent state, separate I2C address, and separate
  sample buffers. No library changes required.
- `setup()`: replaced blocking `while (!Serial)` with 3-second timeout
  to prevent indefinite hang on native USB boards (Leonardo, ESP32, STM32)
  when Serial Monitor is not open.

---

## [1.1.1] — 2026-04-18

### Fixed
- `library.properties`: removed `depends=Wire,EEPROM` — Wire and EEPROM
  are built-in Arduino core libraries not listed in the Library Manager
  index. Declaring them as dependencies caused Library Manager to fail
  installation with "No valid dependencies solution found: dependency
  'Wire' is not available".

---

## [1.1.0] — 2026-04-17

Initial release of the ADS1115 (16-bit) library for APADevices PHX v2 board.
Replaces APAPHX_ADS1015 (12-bit ADS1015) with full rewrite for 16-bit performance.

### New vs APAPHX_ADS1015

- **16-bit ADC** — ADS1115 replacing ADS1015: 4× better resolution
- **Separate sensor classes** — `ADS1115_PHX_PH` and `ADS1115_PHX_RX` replace type string approach
- **Non-blocking state machine** — `startReading()` / `updateReading()` replace blocking loop
- **Fixed memory** — zero heap allocation after `begin()`, AVR safe
- **Guided calibration** — `calibratePoint1()` / `calibratePoint2()` easy two-step API
- **Three-window stability check** — more robust calibration than two-window v1 approach
- **200s mandatory soak** — prevents premature calibration acceptance
- **Rolling average** — `setRollingAverage()` with configurable window 1–10
- **Message callback** — `setMessageCallback()` for LCD/Serial agnostic output
- **Debug system** — compile-time gated with runtime toggle and stream redirect
- **Temperature compensation** — Pasco 2001 formula, pH only
- **EEPROM version protection** — version byte 0xA2 guards against corrupt data
- **`getLastRawMV()`** — access raw differential mV independently of calibration state
- **ESP watchdog** — `yield()` in calibration soak loop for ESP8266/ESP32
- **STM32duino** — added to supported platforms

### Known differences from ADS1015

- EEPROM calibration data format changed — recalibration required after upgrade
- `PHXConfig.type` field removed — sensor type is implicit from class
- `calibratePHX()` now returns `bool` — check return value
- `ADS1115_DEBUG` define must be in header or build flags (not just sketch)

---

*Library by APADevices [@kecup] — [github.com/apadevices/APAPHX2_ADS1115](https://github.com/apadevices/APAPHX2_ADS1115)*
