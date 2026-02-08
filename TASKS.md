# Tasks

## Active
- [ ] **Implement real ISP2 parsing** - Replace random data stub in `readISP2()` with actual Innovate Serial Protocol 2 parser using existing byte-level notes in `docs/protocols/`
- [ ] **Build log analysis tool** - Python script in `tools/analysis/` to load CSV logs and generate AFR vs RPM/MAP scatter plots for tuning
- [ ] **ESP32 port** - Set up PlatformIO project in `firmware/esp32/` with shared code structure, targeting ESP32 as primary platform
- [ ] **Fuel pressure test** - Verify 6–7 PSI steady under WOT load
- [ ] **Accelerator pump check** - Monitor for instant AFR drop < 12.0:1 on throttle stabs; if absent, increase squirter from 0.042" to 0.044"
- [ ] **Secondary spring swap** - Move to white (lightest) spring to force secondary engagement at WOT
- [ ] **Re-evaluate WOT AFR** - Target 12.5–12.8:1 at sustained WOT after secondary spring swap
- [ ] **PVCR adjustment** - Once secondaries open, reduce PVCR insert from 0.063" to ~0.055" to clean up primary enrichment curve

## Waiting On

## Someday

## Done
- [x] ~~Fill in engine spec~~ (2026-02-08) - Completed with full carb calibration, ignition profile, and tuning targets
