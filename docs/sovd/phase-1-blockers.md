# Phase 1 Blockers

## 2026-04-14 - 0x31 routine registration

Status: partially resolved

Resolved in this slice:

- generic `0x31` `RoutineControl` framework is implemented in
  `firmware/bsw/services/Dcm/`
- a shared `0xF018` `Platform Status` DID is now defined on `CVC`, `FZC`, and
  `RZC`
- `stationary` is defined as local motor speed below `50 RPM`
- `brake_secured` is defined as local or received brake position at least `90 %`
- `service_session` is defined from local DCM extended session plus unlocked
  security
- `service_mode_enabled` is defined as the conjunction of
  `stationary + brake_secured + service_session`
- `RZC` and `FZC` now register safe non-actuating `0x31` stubs using that
  contract

Remaining blocker:

### Blocker 1: merge gate still closed pending full safety review

Requirement:

- the Phase 1 instructions and `T1.Sf.1` require full HARA delta review and
  safety-engineer signoff before any `0x31` routine handler is merged

Current status:

- the preliminary delta has been updated to reflect the new `0xF018` contract
- no signed full HARA delta is present in this checkout

Why this still matters:

- the current handlers are intentionally non-actuating
- any future routine step that commands motor or brake hardware remains blocked
  until the safety review signs off the thresholds, interlocks, and failure
  handling

Impacted tasks:

- merge readiness for `T1.E.13`
- merge readiness for `T1.E.14`
- full closure of `T1.Sf.1`

## Stop condition

Do not claim the `0x31` path merge-ready until the full HARA delta is approved
by the safety engineer.
