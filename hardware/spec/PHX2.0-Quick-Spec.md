# APADevices PHX Board 2.0 — Quick Specification

| Parameter | Value |
|-----------|-------|
| **POWER** | |
| Input voltage | 10–15 V DC (nominal 12 V) |
| Current consumption | 45 mA typical (27.75 mA calculated + margin) |
| PCB dimensions | 90 × 65 mm |
| **pH CHANNEL** | |
| Amplifier | LMP7721 femtoampere-input op-amp |
| Reference offset | 2.500 V via ADR4525 (2 ppm/°C) |
| Measurement range | 0 – 14 pH |
| ADC | ADS1115 16-bit, Gain 2 (±2.048 V) |
| Resolution | 62.5 μV/step · ~947 counts/pH |
| System error (pre-cal) | ±0.0088 pH (RSS: reference + op-amp offset) |
| Accuracy (post-cal) | ±0.002 pH (calibrated, probe settled) |
| Temp drift | 0.0008 pH per 10°C (negligible, ADR4525 2 ppm/°C) |
| **ORP / RX CHANNEL** | |
| Amplifier | LMP7721 femtoampere-input op-amp |
| Measurement range | ±2000 mV (Gain 1) · ±1500 mV (Gain 2) |
| Op-amp headroom | 500 mV each side at ±2000 mV — full linear region |
| ADC | ADS1115 16-bit, Gain 1 (±4.096 V) |
| Resolution | 125 μV/step · ~8 counts/mV |
| Accuracy | ±0.5 mV (calibrated, probe settled) |
| **INTERFACE** | |
| Protocol | I2C |
| pH I2C address | 0x49 |
| ORP I2C address | 0x48 |
| MCU supply | 3.3 V or 5 V (match MCU logic level) |
| I2C speed | 100 kHz standard / 400 kHz fast-mode |
| Isolation | ADuM1251 per channel (bidirectional) |
| PCB isolation gap | 5–8 mm milled slots |
| Onboard I2C pullups | 4.7 kΩ × 2, switchable via H3 jumper |
| **PROTECTION** | |
| Overvoltage | TVS diode 15 V |
| Reverse polarity | Schottky diode |
| Overcurrent | PolyFuse 500 mA self-resetting |
| **OTHER** | |
| Temperature compensation | Software only, reference 25 °C |
| Calibration | Two-point, EEPROM persistent |
| EEPROM usage | 17 bytes per channel |
