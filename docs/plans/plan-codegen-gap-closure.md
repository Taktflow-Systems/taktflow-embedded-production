# Plan: Close 4 Codegen Gaps + Build Fix

**Status:** DRAFT
**Created:** 2026-03-26
**Author:** Claude + An Dao

## Overview

The audit of ThreadX OS port revealed 4 gaps where hand-written code
drifts from generated configs, plus a build system bug that masked a
critical RTE buffer overflow. This plan closes all 4 gaps through
codegen improvements and a BSW runtime change, eliminating the
hand-written `Swc_*Com.c` bridge code that caused the OperatingMode=0
bug on hardware.

Root cause chain: `make clean` didn't remove escaped `.o` files
-> stale `Rte.o` with `RTE_MAX_SIGNALS=48` -> `Rte_Write(187,...)` silently
failed -> Vehicle State never propagated -> FZC stuck in INIT on ThreadX.

## Phase Table

| Phase | Name | Status | Risk |
|-------|------|--------|------|
| 0 | Build system hardening | PENDING | LOW |
| 1 | TX auto-pull bridge | PENDING | MEDIUM |
| 2 | E2E SM params codegen | PENDING | LOW |
| 3 | Dem DTC config codegen | PENDING | LOW |
| 4 | NvM/BswM config (stub) | PENDING | LOW |
| 5 | HW verification | PENDING | MEDIUM |

## Phase 0: Build System Hardening

### Tasks
- [x] Fix `threadx-can/Makefile` — strip `../../` from object paths
- [x] Fix `threadx-l5/Makefile` — same pattern
- [ ] Change RTE fallback from `48u` to `#error`
- [ ] Add `.gitignore` rule for `firmware/**/*.o` (catch future escapes)
- [ ] Verify `make clean && make` produces correct `rte_signal_buffer` size

### Files Changed
- `firmware/bsw/rte/include/Rte.h` line 25 — change fallback to `#error`
- `experiments/threadx-can/Makefile` — already done (strip_dotdot)
- `experiments/threadx-l5/Makefile` — already done (strip_dotdot)
- `.gitignore` — add `firmware/**/*.o`

### DONE Criteria
- `make clean && make` in threadx-can: `Rte.o` is at `build/firmware/...` (not `../../firmware/...`)
- `rte_signal_buffer` size in ELF matches `RTE_MAX_SIGNALS` from `Rte_PbCfg.h`
- Compiling without `Rte_PbCfg.h` in include path produces a hard `#error`, not silent 48

---

## Phase 1: TX Auto-Pull Bridge (Critical Path)

### Problem
TX signals in `Com_Cfg.c` have `rteSignalId = COM_RTE_SIGNAL_NONE`.
`Com_MainFunction_Tx` reads from shadow buffers (written by `Com_SendSignal`).
Nobody calls `Com_SendSignal` automatically — hand-written `Swc_*Com.c` does it
but misses signals (OperatingMode, FaultStatus, Commanded_Angle, etc.).

### Solution
1. Set `rteSignalId` on TX signals in `Com_Cfg.c.j2` (same pattern as RX)
2. Add auto-pull in `Com_MainFunction_Tx` — before packing each PDU, read
   from RTE buffer for signals that have `rteSignalId` set
3. E2E signals (DataID, AliveCounter, CRC) keep `COM_RTE_SIGNAL_NONE` — they
   are computed by `E2E_Protect`, not sourced from RTE

### Tasks
- [ ] Template change: `Com_Cfg.c.j2` line 34 — for TX signals, set
      `rteSignalId` to `{{ ecu.prefix }}_SIG_{{ sig.name | upper_snake }}`
      UNLESS signal is an E2E overhead field (DataID, AliveCounter, CRC8)
- [ ] Filter in template: skip E2E signals (detect by name pattern
      `*_E_2_E_*` or `*_E2E_*` — already present in signal names)
- [ ] Runtime change: `Com.c` `Com_MainFunction_Tx` — add auto-pull loop
      before the PDU packing at line ~520 (inside the `should_send` block):
      ```c
      /* Auto-pull TX signals from RTE buffer (mirrors RX auto-push) */
      for (j = 0u; j < com_config->signalCount; j++) {
          const Com_SignalConfigType* sig = &com_config->signalConfig[j];
          if ((sig->PduId == pdu_id) &&
              (sig->RteSignalId != COM_RTE_SIGNAL_NONE)) {
              uint32 rte_val = 0u;
              (void)Rte_Read((Rte_SignalIdType)sig->RteSignalId, &rte_val);
              /* Pack into TX PDU buffer using existing bit-pack logic */
              com_pack_signal_to_pdu(sig, pdu_id, rte_val);
          }
      }
      ```
- [ ] Extract `com_pack_signal_to_pdu()` helper from `Com_SendSignal` lines
      205-238 (the bit-packing code) — reuse for both SendSignal and auto-pull
- [ ] Unit test: `test_Com_TxAutoPull` — write to RTE, call
      `Com_MainFunction_Tx`, verify PDU buffer has correct bits
- [ ] Regenerate all ECU configs: `python -m tools.arxmlgen`
- [ ] Verify: FZC heartbeat on CAN shows OperatingMode=1 without any
      `Swc_FzcCom` code

### Files Changed
- `tools/arxmlgen/templates/com/Com_Cfg.c.j2` line 34 — TX rteSignalId
- `firmware/bsw/services/Com/src/Com.c` — add auto-pull + extract helper
- `firmware/ecu/*/cfg/Com_Cfg_*.c` — regenerated (TX signals get rteSignalId)
- `test/unit/test_Com_TxAutoPull.c` — new test

### DONE Criteria
- Generated `Com_Cfg_Fzc.c` TX signals have `FZC_SIG_*` as rteSignalId
  (except E2E fields which stay `COM_RTE_SIGNAL_NONE`)
- SWC calls `Rte_Write(FZC_SIG_VEHICLE_STATE, 1u)` → FZC heartbeat on CAN
  shows OperatingMode=1 — without any `Swc_FzcCom_TransmitSchedule` call
- POSIX SIL build still passes all tests
- `Swc_FzcCom_TransmitSchedule` and `Swc_CvcCom_TransmitSchedule` are
  no longer needed for signal bridging (can be reduced to application-only
  logic like fault gating, safe-state overrides)

---

## Phase 2: E2E SM Params Codegen

### Problem
E2E State Machine parameters (`WindowSizeValid`, `WindowSizeInvalid`) are
hand-written in `Com_Cfg_*.c` (currently hardcoded 0 → defaults in Com.c).
`MaxDeltaCounter` is generated from `reader.py:998` but SM windows are not.

### Solution
Add `e2e_sm_valid` and `e2e_sm_invalid` fields to the RX PDU config in
the Com template. Compute from sidecar or derive from FTTI formula:
`WindowSizeInvalid = max(2, FTTI_ms / cycle_ms)`

### Tasks
- [ ] Add to sidecar schema: optional `e2e_sm_params` per ECU with per-PDU
      overrides (WindowSizeValid, WindowSizeInvalid)
- [ ] Add to `reader.py`: compute SM params from cycle time if not in sidecar.
      Default: `WindowSizeValid=3`, `WindowSizeInvalid=max(2, 100/cycle_ms)`
- [ ] Update `Com_Cfg.c.j2` line 72: emit `smValid` and `smInvalid` from model
      instead of hardcoded `0u, 0u`
- [ ] Update `Com.c` E2E SM block: use config values instead of defaults
- [ ] Regenerate all ECU configs
- [ ] Unit test: verify SM transitions with generated params

### Files Changed
- `model/ecu_sidecar.yaml` — add optional `e2e_sm_params` section
- `tools/arxmlgen/reader.py` — compute SM window defaults
- `tools/arxmlgen/model.py` — add `e2e_sm_valid`, `e2e_sm_invalid` to Pdu
- `tools/arxmlgen/templates/com/Com_Cfg.c.j2` — emit SM params
- `firmware/bsw/services/Com/src/Com.c` — read SM params from config
- `firmware/ecu/*/cfg/Com_Cfg_*.c` — regenerated

### DONE Criteria
- Generated `Com_Cfg_Fzc.c` RX PDU config has non-zero `smValid` and
  `smInvalid` values derived from cycle time
- E2E SM uses generated params, not hardcoded defaults
- No hand-written SM overrides in platform headers

---

## Phase 3: Dem DTC Config Codegen

### Problem
28 `Dem_SetDtcCode()` calls hand-written in `main.c` (FZC: 16, RZC: 12).
The `dtc_events` data already exists in `ecu_sidecar.yaml` but only
generates `#define` constants — not the DTC code mapping.

### Solution
Add a `dem_cfg` generator that produces `Dem_Cfg_*.c` with a static table
of event→DTC mappings. Dem_Init reads the table instead of main.c calling
`Dem_SetDtcCode` per event.

### Tasks
- [ ] Add DTC codes to sidecar: extend `dtc_events` with `dtc_code` field
      ```yaml
      dtc_events:
        FZC_DTC_STEER_PLAUSIBILITY:
          id: 0
          dtc_code: 0x00D001
        FZC_DTC_STEER_RANGE:
          id: 1
          dtc_code: 0x00D002
      ```
- [ ] Add `dem_cfg` generator: produces `Dem_Cfg_*.c` with:
      ```c
      static const Dem_DtcMapType fzc_dtc_map[] = {
          { FZC_DTC_STEER_PLAUSIBILITY, 0x00D001u },
          { FZC_DTC_STEER_RANGE,        0x00D002u },
      };
      ```
- [ ] Add Jinja2 template: `templates/dem/Dem_Cfg.c.j2`
- [ ] Update `Dem_Init` to accept config with DTC table pointer
- [ ] Remove hand-written `Dem_SetDtcCode` calls from `main.c`
- [ ] Regenerate and verify `Dem_ReportErrorStatus` still produces correct
      DTC codes on UDS `ReadDTCInfo` (0x19)

### Files Changed
- `model/ecu_sidecar.yaml` — extend dtc_events with dtc_code
- `tools/arxmlgen/generators/dem_cfg.py` — new generator
- `tools/arxmlgen/templates/dem/Dem_Cfg.c.j2` — new template
- `tools/arxmlgen/generators/__init__.py` — register `dem_cfg`
- `firmware/bsw/services/Dem/src/Dem.c` — accept config table
- `firmware/bsw/services/Dem/include/Dem.h` — config type with DTC table
- `firmware/ecu/*/cfg/Dem_Cfg_*.c` — new generated files
- `firmware/ecu/{fzc,rzc}/src/main.c` — remove Dem_SetDtcCode calls

### DONE Criteria
- `Dem_SetDtcCode` calls removed from all `main.c` files
- `Dem_Cfg_Fzc.c` generated with 16 DTC mappings matching removed calls
- UDS ReadDTCInfo returns same DTC codes as before

---

## Phase 4: NvM/BswM Config (Stub Generators)

### Problem
No NvM block config or BswM mode rules exist. Both modules are init'd
with `NULL_PTR` config. Low priority — doesn't block OS port.

### Tasks
- [ ] Add placeholder `nvm_cfg` generator that produces empty config struct
- [ ] Add placeholder `bswm_cfg` generator that produces empty mode rule table
- [ ] Wire into `__init__.py` generator registry
- [ ] Document expected sidecar schema for future NvM blocks and BswM rules

### Files Changed
- `tools/arxmlgen/generators/nvm_cfg.py` — new (minimal)
- `tools/arxmlgen/generators/bswm_cfg.py` — new (minimal)
- `tools/arxmlgen/templates/nvm/NvM_Cfg.c.j2` — new (empty config)
- `tools/arxmlgen/templates/bswm/BswM_Cfg.c.j2` — new (empty config)
- `tools/arxmlgen/generators/__init__.py` — register new generators

### DONE Criteria
- `python -m tools.arxmlgen` generates `NvM_Cfg_*.c` and `BswM_Cfg_*.c`
  for all ECUs (even if empty)
- Existing builds still link and run with empty configs

---

## Phase 5: Hardware Verification

### Tasks
- [ ] Regenerate all ECU configs: `python -m tools.arxmlgen`
- [ ] Build FZC ThreadX: `rm -rf build && make -j4` in `experiments/threadx-can`
- [ ] Build CVC ThreadX: `rm -rf build && make build` in `experiments/threadx-cvc`
- [ ] Build POSIX SIL: `make -f firmware/platform/posix/Makefile.posix build`
- [ ] Flash FZC (SN `001A...`) — verify:
      - FZC heartbeat (0x011) shows OperatingMode=1 (from forced RTE stub)
      - All TX signals populated (Steering, Brake, Lidar, Safety)
      - UART shows `vs=1 fault=0`
- [ ] Flash CVC (SN `0027...`) — verify:
      - CVC heartbeat (0x010) shows OperatingMode=1 and FaultStatus
      - Vehicle_State (0x100) shows Mode=1 on CAN bus
      - No more hand-written `Swc_CvcCom_BridgeRxToRte` needed
- [ ] Run `fzc_heartbeat_test.py` on Pi — all 20 frames Mode=1
- [ ] Run hop_test on Pi with correct UDS IDs (0x7E1/0x7E9 for FZC)
- [ ] Run POSIX SIL Docker test suite — all pass

### DONE Criteria
- Both ECUs in RUN state on CAN bus without hand-written Com bridges
- UDS TesterPresent responds
- POSIX SIL tests pass (no regression)
- `Swc_FzcCom_TransmitSchedule` reduced to safe-state override logic only
- `Swc_CvcCom_TransmitSchedule` reduced to safe-state override logic only

---

## Security Considerations
- No new external inputs or attack surfaces
- E2E protection remains active on all safety-critical PDUs
- RTE fallback change (`#error` instead of 48) prevents silent buffer
  overflow — improves memory safety
- DTC codes in generated config (not source) — no credential exposure

## Testing Plan

### Unit Tests
- `test_Com_TxAutoPull.c` — Rte_Write → Com_MainFunction_Tx → verify PDU bits
- `test_Com_TxAutoPull_E2E_Skip.c` — E2E signals NOT pulled from RTE
- `test_Dem_CfgTable.c` — Dem_Init with generated table, verify DTC lookup
- `test_Rte_PbCfg_Error.c` — verify `#error` fires without Rte_PbCfg.h

### Integration Tests (POSIX SIL)
- Full 7-ECU SIL: CVC→FZC heartbeat → state transition to RUN
- UDS ReadDTCInfo: verify DTC codes match sidecar YAML

### Hardware Tests (ThreadX)
- `fzc_heartbeat_test.py` — OperatingMode=1 on all frames
- `hop_test.py` (adjusted IDs) — UDS services respond
- Multi-ECU CAN bus: FZC+CVC+RZC all in RUN

## Open Questions
1. Should `Swc_*Com.c` files be completely removed, or kept for
   application-level safe-state overrides (e.g., force brake=100% in
   SAFE_STOP)? **Recommendation:** keep them for override logic only,
   remove all `Com_SendSignal`/`Com_ReceiveSignal`/`Rte_Write` bridging.
2. For the TX auto-pull, should we add a `COM_TX_SIGNAL_FROM_RTE` flag
   per signal, or just use `rteSignalId != NONE` as the trigger?
   **Recommendation:** use `rteSignalId != NONE` — simpler, consistent
   with RX path.
3. Should E2E SM window defaults be `FTTI / cycle_ms` or a fixed value?
   **Recommendation:** default to `max(3, 100ms / cycle_ms)` for valid,
   `max(2, 50ms / cycle_ms)` for invalid. Override from sidecar.
