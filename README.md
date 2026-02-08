# Analog Bridge

Datalogger and tuning companion for a 1969 Chevrolet Nova.

Evolved from the **CarDuino** project (2020-2021). Bridges analog engine sensors to digital logging for real-time data capture during tuning sessions.

## The Car

- **Year/Make/Model:** 1969 Chevrolet Nova
- **Engine:** 454ci BBC, 0.030" over (~457ci), 11.5:1 CR, aluminum heads, aggressive cam
- **Carburetor:** Holley 850 CFM vacuum secondary
- **Ignition:** MSD mechanical w/ vacuum advance, 16° initial, 34° total mechanical
- **Transmission:** 5-speed manual
- **Fuel:** 91 octane (California pump gas)

## Sensors & Data Channels

| Channel | Sensor/Source | Protocol | Notes |
|---------|--------------|----------|-------|
| AFR (x2) | Innovate LMA-2 wideband O2 | ISP2 (19200 baud) | Dual wideband lambda |
| RPM | Innovate SSI-4 Plus | ISP2 | Via tach signal |
| MAP | Innovate SSI-4 Plus | ISP2 | Manifold vacuum (inHg) |
| Oil Pressure | Innovate SSI-4 Plus | ISP2 | (psig) |
| Coolant Temp | Innovate SSI-4 Plus | ISP2 | (°F) |
| GPS | u-blox (NEO-series) | NMEA via serial | Lat, lon, speed, altitude, heading |
| Accelerometer | MPU-9250 | I2C | 3-axis (g) |
| Gyroscope | MPU-9250 | I2C | 3-axis (deg/s) |

## Firmware Targets

| Target | Status | Notes |
|--------|--------|-------|
| Arduino (Uno/Mega) | Active — running in car | Original CarDuino platform |
| ESP32 | Planned | WiFi/BLE, faster ADC, dual core |

## Log Format

CSV at ~12Hz with columns:
```
time,lat,lon,speed,alt,dir,accx,accy,accz,rotx,roty,rotz,afr,afr1,rpm,map,oilp,coolant
(s),(deg),(deg),(mph),(ft),(deg),(g),(g),(g),(deg/s),(deg/s),(deg/s),(afr),(afr),(rpm),(inHgVac),(psig),(f)
```

## Folder Structure

```
analog-bridge/
├── firmware/
│   ├── arduino/          # Arduino target (current)
│   ├── esp32/            # ESP32 target (planned)
│   └── shared/           # Common code between targets
├── hardware/
│   ├── wiring/           # Wiring diagrams, pinouts
│   └── sensors/          # Sensor datasheets and specs
├── docs/
│   ├── car/              # Engine specs, tuning notes
│   ├── protocols/        # ISP2 protocol docs
│   └── tuning/           # Tuning session notes and targets
├── tools/
│   └── analysis/         # Python/scripts for log analysis
└── logs/                 # Raw log files from sessions
```

## Known Issues

- Aux sensor voltage→unit conversions are placeholders — coolant and RPM need real calibration curves for your specific sensors
- GPS date filename uses hardcoded timezone offset (-7)
- AltSoftSerial on Uno claims pin 10 (PWM) which is also SD CS — may conflict; Mega uses Serial2 and avoids this
- AltSoftSerial 80-byte RX buffer can overflow at 19200 baud between 80ms reads — some ISP2 packets may be dropped (acceptable at ~12Hz)

### Fixed (from CarDuino)
- ~~`readISP2()` random data~~ → real ISP2 parser with header sync, LC-1 AFR, and aux channel decoding
- ~~`firstGPSFix == 1`~~ → assignment `= 1`
- ~~`readGyroXYZ` Z-axis bug~~ → correct `&mRotz`
- ~~String concatenation~~ → `printRow()` with direct `print()` calls
- ~~`logFile.flush()` every 80ms~~ → every 1 second
- ~~MPU9250 `while(1)` halt~~ → graceful degradation
- ~~Loop timing drift~~ → fixed `nextSample += 80` interval
