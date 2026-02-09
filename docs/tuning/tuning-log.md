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

### Ignition Timing
- **Current curve:** 16° initial + 18° mechanical = 34° total, all-in by 2900 RPM
- **Vacuum advance:** Airtex 4V1053 (B28 canister), manifold vacuum, adds 16° at crank → 50° total cruise
- **Distributor:** MSD #8361 Pro-Billet Street, magnetic pickup, adjustable mechanical advance
- **Advance limiter:** Blue 21° bushing installed (MSD supplies multiple bushings: 18°, 21°, 25°, 28°)
- **MSD 6AL #6420:** Controls dwell, multi-spark below ~3000 RPM, rev limiter capable
- **Spark plugs:** NGK BKR5E-11 (heat range 5 — mid-range, appropriate for street/strip 10.6:1)
- 34° total is aggressive for 10.6:1 but aluminum heads reject heat faster than iron — detonation margin is reasonable on 91 octane
- 502 HO runs 32° total at 4000 RPM for comparison — Magnus is +2° and all-in 1100 RPM sooner, which suits the bigger cam's appetite for timing at low RPM
- All-in by 2900 RPM is early; watch for detonation under load at 2500-3500 RPM (heavy throttle, low RPM, high gear). If knock occurs, either retard initial 2° or stiffen advance springs to push all-in to ~3500 RPM
- 50° total cruise is high but typical for BBC with vacuum advance on manifold vacuum. If pinging on decel or light throttle, switch vacuum source to ported vacuum (eliminates advance at idle/decel)

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

## Key Conclusions

- The 80531 was designed for a 502 HO (461 HP) with a much milder cam (.510/.540, 211/230) and 9% more displacement. Every tuning issue on this 454 traces back to that mismatch.
- The WOT lean condition (14-15:1 AFR) is almost certainly the secondaries not opening, not a primary-side fuel delivery problem. The primaries are well-calibrated (78 jets, stable cruise AFR).
- Pink secondary spring is factory stock for the 80531 — it was sized for a 502's stronger vacuum signal. The 457ci simply doesn't pull hard enough to overcome it under street driving.
- The +2 secondary jet bump (82→84) and larger squirter (.040→.042) were correct moves, but won't matter until the secondaries actually open.
- After spring swap, secondary jets may need another +2 (to 86) given the higher CR (10.6 vs 8.75) and better-flowing oval port heads at .625" lift.
- Rear secondary PV (3.5 inHg) should be tested after spring swap — on big vac-sec carbs it aids the pri-to-sec transition, unlike smaller Holleys where pulling it is standard advice.
- 850 CFM is well-matched: engine needs 859 CFM at 6500 RPM (cam's upper range). Vacuum secondaries self-regulate, so "oversized" is not a concern.
- Primary PV (4.5 inHg) is correctly sized: rule of thumb is half idle vacuum (10/2 = 5.0), 4.5 gives 5.5 inHg margin.
- Mechanical advance all-in at 2900 RPM is aggressive — 502 HO doesn't reach total until 4000 RPM. The bigger cam wants timing early, but this puts full advance under heavy load in low gears (2500-3500 RPM zone). Monitor for detonation under load; if knock occurs, stiffen advance springs to push all-in to ~3500 RPM or pull 2° initial (14° BTDC). Aluminum heads buy margin here but 10.6:1 on 91 octane isn't unlimited.

## Recommended Tuning Sequence

1. White spring → verify secondaries open (paperclip test)
2. Test rear PV in/out (log AFR through secondary transition)
3. Re-evaluate secondary jetting (84 may need +2 to 86)
4. Adjust PVCR (reduce from .063" toward ~.055")
5. Final WOT AFR eval — target 12.5-12.8:1

One change at a time. Log each session below.

## Session History

_Log sessions here as they happen_

| Date | Changes Made | Result | AFR (WOT) | Notes |
|------|-------------|--------|-----------|-------|
| | | | | |
