# Changelog

All notable changes to APAPHX2_ADS1115 are documented here.

## [1.1.0] — 2025

Initial release of the ADS1115 (16-bit) library for APADevices PHX v2 board.
Replaces APAPHX_ADS1015 (12-bit ADS1015) with full rewrite for 16-bit performance.

### New vs APAPHX_ADS1015

- **16-bit ADC** — ADS1115 replacing ADS1012: 4× better resolution
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
