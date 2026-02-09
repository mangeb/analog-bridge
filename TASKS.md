# Tasks

## Active
- [ ] **Calibrate ISP2 aux conversions** - Replace placeholder voltage→unit macros (`AUX_COOLANT_F`, `AUX_RPM`, etc.) with actual sensor calibration curves
- [ ] **Build log analysis tool** - Python script in `tools/analysis/` to load CSV logs and generate AFR vs RPM/MAP scatter plots for tuning
- [ ] **ESP32 port** - Set up PlatformIO project in `firmware/esp32/` with shared code structure, targeting ESP32 as primary platform
- [ ] **Fuel pressure test** - Verify 6–7 PSI steady under WOT load
- [ ] **Accelerator pump check** - Monitor for instant AFR drop < 12.0:1 on throttle stabs; if absent, increase squirter from 0.042" to 0.044"
- [ ] **Secondary spring swap** - Move to white (lightest) spring to force secondary engagement at WOT
- [ ] **Re-evaluate WOT AFR** - Target 12.5–12.8:1 at sustained WOT after secondary spring swap
- [ ] **PVCR adjustment** - Once secondaries open, reduce PVCR insert from 0.063" to ~0.055" to clean up primary enrichment curve
- [ ] **Evaluate rear (secondary) power valve** - Factory 80531 has 3.5 inHg PV in secondary metering block. Test with/without after spring swap — may help or hurt pri-to-sec transition on big vac-sec carbs
- [ ] **Import VSS calibration into AUX_RPM macro** - Use VSS freq calculation from `Docs/Nova - VSS Setup for SSI-4` to set proper RPM scaling
- [ ] **Merge tuning CSV into tuning-log.md** - Import RPM/AFR/vacuum performance table from `Docs/Nova - Tuning - 2024-05-22 #1.csv`
- [ ] **Build tuning log app** - HTML/JS app in `tools/` to log tuning sessions (date, changes, AFR readings, notes), visualize jetting history, and track progress against targets. Load/save session data, chart AFR vs RPM, overlay before/after comparisons

## Waiting On

## Someday

## Done
- [x] ~~Fill in engine spec~~ (2026-02-08) - Completed with full carb calibration, ignition profile, and tuning targets
- [x] ~~Implement real ISP2 parsing~~ (2026-02-08) - Replaced random() stub with header-sync state machine, LC-1 AFR decoding (float math), and aux channel extraction. Fixes: double-indexing bug, integer division AFR=0 bug from InnovateTest. Mega uses Serial2 hardware UART.
- [x] ~~Confirm head chamber cc and valve sizes~~ (2026-02-08) - Edelbrock #60459: 110cc chamber, 2.19"/1.88" valves, 290cc intake runner, 26° valve angle
- [x] ~~Build torque/HP estimator~~ (2026-02-08) - HTML/JS tool in `tools/torque-hp-estimator.html` with semi-empirical model from head flow + cam profile. Pre-loaded with Nova 454 specs, Edelbrock #60459 flow data, old cam overlay comparison
