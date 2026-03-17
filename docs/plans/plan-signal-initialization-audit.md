# Signal Initialization Audit — Production SIL Pipeline Fix

**Status**: PENDING
**Date**: 2026-03-17
**Context**: Production SIL (taktflow-embedded-production) fails 18/18 tests. Root cause: the codegen pipeline was never validated end-to-end. Each layer (DBC → ARXML → sidecar → generated configs → SWC code → plant-sim) has independent hand-written values that diverge.

## Problem Summary

The production SIL test suite cannot pass SIL-001 (Normal Startup) because multiple signal mismatches cause spurious faults at boot:

1. **E2E DataID mismatch**: CVC sends heartbeat with DataID=0x04 (sidecar), SC expects 0x02 (DBC) → E2E_FAIL → relay kill
2. **Virtual sensor routing**: FZC_App.h hardcodes Com signal IDs 14/15/16 that collide with generated TX signal IDs → sensor feeder reads wrong buffer → steer=-45°
3. **DBC signal offset**: SteerAngleCmd had offset=-45, so raw=0 (C zero-init) = -45° physical → spurious steer commands at boot
4. **ARXML drift**: ARXML was generated once, then DBC evolved independently. Regenerating ARXML from current DBC changes signal bit positions and PDU names
5. **Missing cantools**: Without cantools installed, codegen falls back to ARXML-only mode with different naming conventions → compile errors

**Root cause**: No single-command pipeline that validates DBC → ARXML → codegen → compile → test. Each step was done manually and diverged over time.

## Fixes Already Committed (fix/sil-deployment branch)

| Commit | Fix | Layer |
|--------|-----|-------|
| `9a606bb` | Switch e2e_source from sidecar to DBC | codegen config |
| `437dc3d` | Add missing E2E_DataID for ICU/TCU/BCM/Battery | DBC |
| `cc28700` | Align sidecar e2e_data_ids with DBC | sidecar |
| `99f253a` | set -e in Dockerfile build loop | Docker |
| `f13da96` | Guard RTE dispatch for RTE_MAX_RUNNABLES==0 | BSW |
| `57e6b1f` | SteerAngleCmd: signed int16, offset=0 | DBC |
| `fc0e2f2` | Plant_Sim sender node for 0x600/0x601 | DBC |
| `46595f4` | FZC/RZC App.h → use generated signal IDs | SWC |

## What Still Needs Fixing

### Phase 1: Freeze DBC as Single Source of Truth

**Goal**: One authoritative DBC that defines ALL CAN communication including virtual sensors.

1. ✅ **Audit every signal in the DBC** — DONE 2026-03-17
   - Ran `cantools` audit: 0 signals with raw=0 ≠ neutral (SteerAngleCmd was the only one, already fixed)
   - All signals: raw=0 = 0.0 physical (safe default)

2. ✅ **Add all SIL-only frames to DBC** — DONE 2026-03-17
   - ✅ 0x600 FZC_Virtual_Sensors: sender=Plant_Sim
   - ✅ 0x601 RZC_Virtual_Sensors: sender=Plant_Sim
   - ✅ 0x500 DTC_Broadcast: sender=CVC (already present)

3. **Signal naming convention**: DBC signal names must match what the codegen produces for `#define` names. Audit and fix any that diverge.

### Phase 2: Regenerate ARXML from Frozen DBC — DONE 2026-03-17

**Goal**: ARXML is 100% derived from DBC, no manual edits.

1. ✅ Ran `dbc2arxml.py` on the frozen DBC
2. ✅ Diffed: 167→174 signal mappings, 0 changed bit positions, 7 added (virtual sensors), 0 removed
3. ✅ No converter fixes needed — all existing signals preserved
4. ✅ Committed `909b897`

### Phase 3: Validate Codegen Output — IN PROGRESS 2026-03-17

**Goal**: All generated configs compile without error against existing SWC code.

1. ✅ `cantools` installed on VPS (required for DBC routing in codegen)
2. ✅ Full codegen: `python3 -m tools.arxmlgen --config project.yaml` — 51 files written
3. ✅ Build 6/7 ECUs clean (ICU needs ncurses from Docker — verified in Docker build)
4. ✅ Monolithic SWC skeletons (`Swc_CVC.c`, `Swc_FZC.c`, `Swc_RZC.c`) must be deleted after regen — they clash with individual SWC files
5. **TODO**: Add `cantools` to Dockerfile.vecu builder stage
6. **TODO**: Add `.gitignore` for monolithic SWC skeletons (or fix codegen to skip them)
7. **TODO**: Docker build + SIL-001 test

### Phase 4: Plant-Sim Alignment

**Goal**: Plant-sim encodes/decodes using cantools with the same DBC.

1. Replace all `struct.pack_into` calls with `db.encode_message()` where possible
2. Virtual sensor frames (0x600/0x601) — verify plant-sim's raw encoding matches DBC signal definitions
3. Reset handler: `reset_state()` must clear ALL physics to neutral (verified via DBC raw=0 = neutral)

### Phase 5: End-to-End Pipeline Script

**Goal**: One command to validate the entire chain.

Create `scripts/validate-pipeline.sh`:
```
1. Check cantools installed
2. Run dbc2arxml.py → diff against committed ARXML
3. Run arxmlgen → verify generated files match committed
4. Build all 7 ECUs (POSIX) → 0 errors
5. Start Docker SIL
6. Run SIL-001 → must PASS
7. Run full suite → report pass rate
```

### Phase 6: CI Enforcement

1. Add `cantools` to CI requirements
2. CI step: regenerate → diff → fail if generated files changed without committing
3. CI step: build all ECUs before Docker build (catches compile errors early)

## Definition of Done

- [ ] SIL-001 passes on VPS (Netcup)
- [ ] SIL-001 through SIL-015 pass (core firmware scenarios)
- [ ] SIL-016 through SIL-018 pass (gateway scenarios)
- [ ] `scripts/validate-pipeline.sh` exists and passes
- [ ] Zero hardcoded Com signal IDs in `*_App.h` files
- [ ] All virtual sensor frames defined in DBC
- [ ] CI enforces generated file consistency

## Lessons Learned (to append after completion)

- `cantools` is a build dependency, not optional — without it, codegen produces wrong output silently
- DBC signal offset must ensure raw=0 = neutral physical value — C zero-init depends on this
- Virtual sensor frames (SIL-only) must be in the DBC like any other message — no special cases
- Hand-written signal IDs in App.h will inevitably collide with generated IDs — always alias from generated defines
- ARXML must be regenerated from DBC, never hand-edited — drift is guaranteed
