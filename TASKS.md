# Tasks

## Active
- [ ] **Calibrate ISP2 aux conversions** - Replace placeholder voltage-to-unit macros (AUX_COOLANT_F, AUX_OILP_PSI, AUX_MAP_INHG) with actual sensor calibration curves. VSS done.
- [ ] **Build log analysis tool** - Python script in tools/analysis/ to load CSV logs and generate AFR vs RPM/MAP scatter plots for tuning
- [ ] **ESP32 port** - Set up PlatformIO project in firmware/esp32/ with shared code structure, targeting ESP32 as primary platform
- [ ] **Fuel pressure test** - Verify 6-7 PSI steady under WOT load
- [ ] **Accelerator pump check** - Monitor for instant AFR drop below 12.0:1 on throttle stabs; if absent, increase squirter from 0.042 to 0.044
- [ ] **Secondary spring swap** - Move to white (lightest) spring to force secondary engagement at WOT
- [ ] **Re-evaluate WOT AFR** - Target 12.5-12.8:1 at sustained WOT after secondary spring swap
- [ ] **PVCR adjustment** - Once secondaries open, reduce PVCR insert from 0.063 to 0.055 to clean up primary enrichment curve
- [ ] **Evaluate rear power valve** - Factory 80531 has 3.5 inHg PV in secondary metering block. Test with/without after spring swap
- [ ] **Verify SSI-4 frequency range** - Check LM Programmer setting for VSS channel max frequency (currently assumed 1500 Hz)
- [ ] **Merge tuning CSV into tuning-log** - Import RPM/AFR/vacuum performance table from tuning CSV
- [ ] **Build tuning log app** - HTML/JS app in tools/ to log tuning sessions, visualize jetting history, track progress against targets

## Waiting On

## Someday

## Done
- [x] **Fill in engine spec** - Completed with full carb calibration, ignition profile, and tuning targets (2026-02-08)
- [x] **Implement real ISP2 parsing** - Replaced random() stub with header-sync state machine, LC-1 AFR decoding, and aux channel extraction (2026-02-08)
- [x] **Confirm head chamber cc and valve sizes** - Edelbrock 60459: 110cc chamber, 2.19/1.88 valves, 290cc intake runner, 26 deg valve angle (2026-02-08)
- [x] **Build torque/HP estimator** - HTML/JS tool with semi-empirical model from head flow and cam profile, pre-loaded with Nova 454 specs (2026-02-08)
- [x] **Import VSS calibration into firmware** - Replaced AUX_RPM placeholder with AUX_VSS_MPH using 235/70R15 + 3.73 + 17-tooth reluctor derivation (2026-02-09)
