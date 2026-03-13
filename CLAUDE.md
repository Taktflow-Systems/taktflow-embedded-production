# Taktflow Embedded Production

ISO 26262 ASIL D zonal vehicle platform — 7 ECUs, AUTOSAR-like BSW, CAN 500k, E2E protection.

## Architecture

**Zonal topology**: CVC (central) + FZC (front) + RZC (rear) + SC (safety, TMS570) + BCM/ICU/TCU (body/instrument/transmission).

**DBC-first workflow**: `gateway/taktflow.dbc` is the single source of truth for CAN communication.
`tools/arxml/` converts DBC → ARXML → generated C configs. Never hand-edit generated files.

## Build & Test

```
make -f firmware/platform/posix/Makefile.posix build    # SIL (Docker/Linux)
make -f firmware/platform/stm32/Makefile.stm32 build    # STM32 physical ECUs
make -f firmware/platform/tms570/Makefile.tms570 build   # TMS570 Safety Controller
make test                                                 # Run all tests
```

## Project Layout

```
firmware/
  bsw/                   — AUTOSAR-like BSW stack (shared across ECUs)
    mcal/                 — MCAL: Can, Spi, Adc, Pwm, Dio, Gpt, Uart
    ecual/                — ECUAL: CanIf, PduR, IoHwAb
    services/             — Services: Com, Dcm, Dem, E2E, WdgM, BswM, NvM, SchM, Det, CanTp
    rte/                  — Runtime Environment
    os/                   — OS abstraction (bare-metal scheduler / POSIX shim)
  ecu/{cvc,fzc,rzc,sc,bcm,icu,tcu}/  — Per-ECU application code
    src/                  — SWC source files
    include/              — SWC headers
    cfg/                  — GENERATED config (Com_Cfg, Rte_Cfg) — do not hand-edit
    test/                 — Unit tests
  lib/vendor/             — Third-party / vendor libraries (wrapped)
  platform/{stm32,tms570,posix}/  — Platform-specific MCAL implementations + makefiles
gateway/                  — Edge gateway services (MQTT, CAN bridge, plant-sim, SAP QM)
docker/                   — Dockerfiles and compose for SIL/HIL
test/{sil,hil,mil,pil}/   — xIL test scenarios, fixtures, reports
test/framework/           — Shared test framework
tools/
  arxml/                  — DBC→ARXML converter, SWC extractor, C codegen
  codegen/                — Jinja2-based code generator (templates + driver)
  ci/                     — CI/CD scripts
  misra/                  — MISRA C:2012 checker configs
  trace/                  — Requirements traceability tools
docs/
  safety/                 — ISO 26262 (concept, plan, analysis, requirements, validation)
  aspice/                 — ASPICE deliverables
  plans/                  — Implementation plans (plan first, code second)
  reference/              — Process playbook, ASIL-D reference
  lessons-learned/        — Post-incident and post-fix notes
  api/                    — API documentation
hardware/                 — Schematics, pin maps, BOM
scripts/                  — Build, deploy, debug utilities
```

## Workflow

Plan first → `docs/plans/` → approve → code. Update plan before next phase.

## Standards

- All rules in `.claude/rules/` — see filenames for topics
- Naming: `docs/reference/naming-conventions.md`
- Safety: ISO 26262 ASIL D, MISRA C:2012
- Process: ASPICE Level 2

## Git Flow

`main` (protected, tagged vX.Y.Z) ← `release/` ← `develop` ← `feat/`, `fix/`, `hotfix/`.
Commits: Conventional Commits, imperative, <50 chars.

## Key Principles

1. **DBC is truth** — all CAN config flows from `gateway/taktflow.dbc`
2. **Generate, don't copy** — `ecu/*/cfg/` files are generated, never hand-edited
3. **Platform abstraction** — same SWC code compiles for STM32, TMS570, and POSIX
4. **Fail-closed safety** — faults → safe state, never ignore errors
5. **Vendor independence** — wrap vendor SDKs, prefer generic protocols
