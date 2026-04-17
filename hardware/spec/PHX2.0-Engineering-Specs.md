# APADevices PHX Board 2.0 — Engineering Specification

**Document type:** Hardware engineering specification  
**Product:** APADevices PHX Board v2.0  
**Revision:** 1.1

---

## 1. Power and Mechanical Parameters

| Parameter | Value |
|-----------|-------|
| Input voltage | 10–15 V DC (nominal 12 V) |
| Current consumption | 45 mA (typical) |
| PCB dimensions | 90 × 65 mm |
| Input protection | TVS diode 15 V + reverse polarity protection + 500 mA PolyFuse |

### 1.1 Power Budget

Current consumption is determined by the sum of both isolated channel loads and the efficiency of the B1212XT-1WR3 DC/DC converters.

**Per isolated channel:**

| Sub-circuit | Current |
|-------------|---------|
| Analog section (LMP7721, ADR4525, ADP7118) | ~2.6 mA |
| Digital section (ADuM1251, ADS1115, LED) | ~8.5 mA |
| **Total per channel** | **11.1 mA at 12 V** |

**Total input current (two channels, converter efficiency 80%):**

```
I_in = (11.1 mA × 2) / 0.80 = 27.75 mA
```

The specified 45 mA provides a safe margin for transient peaks and losses in the LC input filter.

---

## 2. pH Channel Measurement Specification

| Parameter | Value |
|-----------|-------|
| Amplifier | LMP7721 (femtoampere-input precision op-amp) |
| Measurement range | 0 – 14 pH |
| Reference offset | 2.500 V (stability: 2 ppm/°C via ADR4525 voltage reference) |
| ADC | ADS1115, 16-bit, FSR ±2.048 V (Gain 2) |
| Resolution (LSB) | 62.5 μV/step |

> At Gain 2 (FSR ±2.048 V) the ADS1115 LSB = 2048 mV / 32768 = 62.5 μV.
> Yields approximately 947 counts per pH unit at the Nernst electrode slope of 59.16 mV/pH.

### 2.1 System Error Budget (pH)

Accuracy is determined by the cascaded voltage reference and op-amp error chain.

| Error source | Value |
|-------------|-------|
| Voltage reference (ADR4525B) initial accuracy | 0.02% = 500 μV |
| Op-amp offset (LMP7721) | 150 μV max |
| **Total RSS error** | **√(500² + 150²) ≈ 522 μV** |

**Converted to pH at 25°C:**

```
Error = 0.522 mV / 59.16 mV/pH ≈ 0.0088 pH
```

After two-point calibration the system demonstrates stability below **±0.002 pH** — the calibration process eliminates the systematic component of the error, leaving only the noise floor.

### 2.2 Temperature Drift

The ADR4525 voltage reference (2 ppm/°C) shifts the 2.500 V reference point by only **50 μV per 10°C** of ambient temperature change.

```
Drift = 2500 mV × 2 ppm/°C × 10°C = 50 μV
pH error = 50 μV / 59.16 mV/pH ≈ 0.0008 pH per 10°C
```

This is negligible under normal pool and aquarium operating conditions. Software temperature compensation (Pasco 2001 formula) is available but not required for typical use.

---

## 3. ORP / RX Channel Measurement Specification

| Parameter | Value |
|-----------|-------|
| Measurement range (standard) | ±1500 mV (Gain 2, FSR ±2.048 V) |
| Measurement range (full) | ±2000 mV (Gain 1, FSR ±4.096 V) |
| ADC | ADS1115, 16-bit, FSR ±4.096 V (Gain 1, default) |
| Resolution (LSB) | 125 μV/step |

> **Default library setting:** Gain 1 (±4.096 V) is used as default to accommodate the full
> ±2000 mV ORP probe range without clipping.

### 3.1 ORP Dynamic Range and Headroom

The LMP7721 op-amp operates in rail-to-rail output mode with 5.0 V supply:

| Limit | Calculation | Value |
|-------|-------------|-------|
| Reference offset | — | 2.500 V |
| Maximum positive output | 5.0 V − 2.5 V | +2500 mV |
| Maximum negative output | 0.0 V − 2.5 V | −2500 mV |
| Required range | — | ±2000 mV |
| **Headroom (each side)** | 2500 − 2000 | **500 mV** |

The required ±2000 mV range sits 500 mV inside the op-amp linear output region on both sides, eliminating distortion at extreme ORP values.

---

## 4. Digital Interface and Isolation

| Parameter | Value |
|-----------|-------|
| Communication | I2C |
| I2C isolator | ADuM1251 bidirectional isolator (per channel) |
| I2C speed | Standard-mode 100 kHz / Fast-mode 400 kHz supported |
| pH channel address | 0x49 |
| ORP channel address | 0x48 |
| PCB isolation gap | 5–8 mm milled slots beneath all isolators |
| Temperature compensation | External (software only) — reference temperature 25 °C |

---

## 5. Protection and Safety

| Feature | Implementation |
|---------|----------------|
| Overvoltage transient | TVS diode, clamping at 15 V |
| Reverse polarity | Schottky diode protection |
| Overcurrent | PolyFuse 500 mA (self-resetting) |
| Galvanic isolation | ADuM1251 per channel — MCU and analog sides fully isolated |
| PCB creepage | 5–8 mm milled isolation slots beneath all isolators |

---

## Notice

PHX Board 2.0 is a module intended for professional integration. The final equipment
manufacturer is responsible for ensuring that the complete system complies with all
applicable directives and regulations.
