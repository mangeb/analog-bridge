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

## Session History

_Log sessions here as they happen_

| Date | Changes Made | Result | AFR (WOT) | Notes |
|------|-------------|--------|-----------|-------|
| | | | | |
