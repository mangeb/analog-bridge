# Tuning Log: 1969 Chevrolet Nova 454 BBC

**Project Status:** Active Tuning Phase
**Last Updated:** February 2026

## Current Diagnostic Observations

### Idle/Cruise
- Stable, but goes rich if primary jets increase beyond 78

### WOT
- **Problem:** Lean spike / sustained lean (AFR 14–15:1) at wide open throttle
- **Root cause hypothesis:** Secondaries not opening sufficiently under street loads — engine effectively running as a 2-barrel at WOT, starving the primary side

### Secondary Operation
- Paperclip test indicates secondaries are not opening under street loads
- Pink (light/medium) spring currently installed (factory stock for 80531)
- Factory secondary power valve: 3.5 inHg (in secondary metering block)

## Action Plan

1. **Fuel pressure test** — Verify 6–7 PSI steady under WOT load
2. **Accelerator pump check** — Monitor for instant AFR drop < 12.0:1 on throttle stabs; if absent, increase squirter from 0.042" to 0.044"
3. **Secondary spring swap** — Move to white (lightest) spring to force secondary engagement
4. **Re-evaluate AFR** — Target 12.5–12.8:1 at sustained WOT
5. **PVCR adjustment** — Once secondaries are opening, reduce PVCR insert from 0.063" to ~0.055" to clean up primary enrichment curve
6. **Evaluate rear (secondary) power valve** — Factory 80531 has a 3.5 inHg PV in the secondary metering block. Forum consensus is mixed: pulling it is common advice on most Holleys, but on big vacuum-secondary carbs like the 80531 it helps the primary-to-secondary transition. Test with and without after secondary spring swap. See: [Speed-Talk 80531 thread](https://www.speed-talk.com/forum/viewtopic.php?t=47525), [Chevelles.com 80531 thread](https://www.chevelles.com/threads/holley-80531.331577/)

## Carb Calibration Analysis: GM 502 HO vs Nova 454

The Holley 80531 was factory-calibrated for the **GM 502 HO crate engine (461 HP)**. Here's how that engine compares to this build and what it means for tuning.

| Spec | GM 502 HO (80531 target) | This Nova 454 .030 |
|------|--------------------------|---------------------|
| Displacement | 502ci (4.470 x 4.000) | ~457ci (4.280 x 4.000) |
| Compression | 8.75:1 | 10.6:1 |
| Heads | Iron rectangular port, 118cc | Alum oval port #60459, 110cc |
| Cam lift (int/exh) | .510/.540 | .625/.639 |
| Cam duration @.050 | 211°/230° | 236°/245° |
| Cam type | Hydraulic roller | Hydraulic roller |
| Intake | (not specified) | Edelbrock Air-Gap #7561 dual plane |
| Est. idle vacuum | ~14-16 inHg | ~10 inHg |
| Max RPM | 5,500 | 6,500 (cam range) |
| Fuel | 89 octane regular | 91 octane premium |

### What the differences mean for carb calibration

**Displacement (457 vs 502 = 9% smaller):** Less airflow at the same RPM means less venturi signal. The vacuum secondaries see a weaker pull and are slower to open. This directly explains why the paperclip test shows secondaries not opening under street loads — the 502 makes ~9% more signal at the same throttle position. The white (lightest) spring swap is the correct fix.

**Cam (236/245 vs 211/230 @.050):** The big Rollin' Thunder cam drops idle vacuum from ~14-16 inHg (502 HO) to ~10 inHg. This has several effects:
- Power valve margin is tighter (4.5 PV vs 10 inHg idle = 5.5 inHg margin). Standard rule is PV = half idle vacuum (10/2 = 5.0), so 4.5 is fine but leaves less room than the 502 had (~7-8 inHg margin). In gear with A/C, if vacuum dips to 6-7 inHg, still OK.
- More cam overlap means more reversion at low RPM — the dual-plane Air-Gap helps isolate this vs a single-plane.
- At WOT the bigger cam flows significantly more air (25° more intake duration), so the engine can use the 850 CFM more effectively than the 502 HO could.

**Compression (10.6 vs 8.75):** Higher CR traps more mixture per cycle, so the engine needs proportionally more fuel at WOT for the same AFR. This supports the +2 secondary jet increase (82→84) and may need more — evaluate after secondaries are opening.

**Head flow (oval port vs rect port):** The oval port #60459 heads flow better at the lift ranges the Rollin' Thunder provides (.625") than the 502 HO's iron rect port heads at .510". More airflow = leaner tendency at WOT if jetting isn't increased.

**CFM sizing check:** Max CFM ≈ (CID × RPM) / 3456. At 5500 RPM: 727 CFM. At 6500 RPM: 859 CFM. The 850 is well-matched for the cam's full RPM range (1500-6500). For the 502 at 5500 RPM: 799 CFM. Both engines are within the carb's range — vacuum secondaries self-regulate, so "oversized" isn't really an issue here.

### Summary: what needs to change from factory 502 calibration

| Parameter | Factory (for 502 HO) | Current | Assessment |
|-----------|----------------------|---------|------------|
| Primary jets | 78 | 78 | OK — cruise AFR is stable |
| Secondary jets | 82 | 84 (+2) | Correct direction, may need +2 more after sec spring swap |
| Primary PV | 4.5 | 4.5 | OK — 10 inHg idle gives 5.5 margin |
| Secondary PV | 3.5 | 3.5 (factory) | Test after spring swap |
| Squirter | .040" | .042" | Correct direction — bigger cam needs more pump shot |
| Sec spring | Pink (med) | Pink (med) | **Too stiff for 457ci** — swap to white |
| Acc pump cam | (factory) | Pink #1 (aggressive) | OK — compensates for softer vacuum signal |
| PVCR | (factory) | .063" | Evaluate after secondaries are opening |

## Recommended Tuning Sequence

White spring → verify secondaries open (paperclip test) → test rear PV in/out (log AFR through sec transition) → adjust PVCR → final WOT AFR eval. Do one change at a time, log each session below.

## Session History

_Log sessions here as they happen_

| Date | Changes Made | Result | AFR (WOT) | Notes |
|------|-------------|--------|-----------|-------|
| | | | | |
