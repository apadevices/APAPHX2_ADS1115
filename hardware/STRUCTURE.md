# APAPHX2_ADS1115 — Repository Structure

```
APAPHX2_ADS1115/
│
├── APAPHX2_ADS1115.h               # Library header — class declarations, enums, structs, constants
├── APAPHX2_ADS1115.cpp             # Library implementation
│
├── library.properties              # Arduino Library Manager metadata
├── keywords.txt                    # Arduino IDE syntax highlighting
├── CHANGELOG.md                    # Version history
├── LICENSE                         # MIT License
├── README.md                       # Main documentation (installation, API, examples, wiring)
│
├── examples/
│   ├── simple/
│   │   ├── PH_Simple/
│   │   │   └── PH_Simple.ino       # Minimal pH reading, no calibration
│   │   ├── RX_Simple/
│   │   │   └── RX_Simple.ino       # Minimal ORP/RX reading, no calibration
│   │   └── PH_RX_Simple/
│   │       └── PH_RX_Simple.ino    # Both channels, minimal setup
│   │
│   ├── middle/
│   │   ├── PH_Calibration/
│   │   │   └── PH_Calibration.ino  # pH two-point calibration + EEPROM save
│   │   ├── RX_Calibration/
│   │   │   └── RX_Calibration.ino  # RX two-point calibration + EEPROM save
│   │   └── PH_RX_Calibration/
│   │       └── PH_RX_Calibration.ino  # Both channels, calibration, rolling average
│   │
│   └── advanced/
│       ├── PH_Advanced/
│       │   └── PH_Advanced.ino     # pH with temperature compensation + debug
│       ├── RX_Advanced/
│       │   └── RX_Advanced.ino     # RX with full config, EEPROM, debug
│       └── PH_RX_Advanced/
│           └── PH_RX_Advanced.ino  # Full dual-channel production-ready sketch
│
└── hardware/
    ├── tests/
    │   ├── TEST_calibration_stability.md   # HWT-002: Post-calibration noise floor (30 readings)
    │   └── TEST_rolling_avg_stability.md   # HWT-003: Gain verify + 464-reading long-run drift
    │
    ├── docs/
    │   └── PHX2.0-Declaration-of-Conformity.pdf   # CE Declaration of Conformity — PHX v2 board
    │
    └── spec/
        ├── PHX2.0-Engineering-Specs.md             # Full engineering specification — pinout, electrical ratings, analog frontend
        └── PHX2.0-Quick-Spec.md                    # Quick reference card — key parameters at a glance
```

---

## Folder Notes

### `hardware/tests/`
Real hardware test reports captured from serial monitor during library development. Each report includes full raw data, statistical analysis and pass/fail conclusion. These are development records — not runnable sketches (those live in `examples/`).

Test IDs follow the pattern `HWT-NNN` (Hardware Test). HWT-001 was Phase 1 ADC validation (I2C, gain switching, data rate timing — no report file kept). HWT-002 and HWT-003 are the first formally documented reports.

### `hardware/docs/`
Board-level documentation. Currently contains the CE Declaration of Conformity for the PHX v2 board (`PHX2.0-Declaration-of-Conformity.pdf`).

### `hardware/spec/`
PHX v2 board specifications in two formats:
- `PHX2.0-Engineering-Specs.md` — full engineering reference: pinout tables (P1/CN12 connectors), absolute maximum ratings, power supply requirements, probe input impedance, analog frontend gain and offset, ADS1115 configuration details.
- `PHX2.0-Quick-Spec.md` — condensed quick-reference card with the key parameters needed for wiring and integration at a glance.
```
