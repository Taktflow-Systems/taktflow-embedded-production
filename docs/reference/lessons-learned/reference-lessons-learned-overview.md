# Lessons Learned â€” Consolidated Index

All lessons-learned documents live in this folder. One file per topic.

## Naming Convention

| Prefix | Scope | Example |
|--------|-------|---------|
| `PROCESS-` | Workflow, tooling, process decisions | `PROCESS-security-hardening.md` |
| `SYS-NNN-` | System requirement HITL review | `SYS-001-dual-pedal-sensing.md` |

## Index

### Safety

| File | Topic | Date | Status |
|------|-------|------|--------|
| [PROCESS-safety-case-development.md](safety/PROCESS-safety-case-development.md) | Safety case â€” HARA, FMEA, DFA, FSRâ†’TSR flow-down, ASIL decomposition | 2026-02-27 | Closed |
| [PROCESS-hil-gap-analysis.md](safety/PROCESS-hil-gap-analysis.md) | HIL gap analysis, fault injection safety, preemptive protection | 2026-03-01 | Closed |
| [PROCESS-sc-safety-critical-dataflow-state-transition-investigation.md](safety/PROCESS-sc-safety-critical-dataflow-state-transition-investigation.md) | SC safety-path investigation â€” dataflow reconstruction, transition model, and verification gaps | 2026-03-08 | Open |
| [PROCESS-sil-hil-divergence-preflight.md](safety/PROCESS-sil-hil-divergence-preflight.md) | SIL/HIL divergence preflight â€” boundary failures, bench-readiness gating, and closure rules | 2026-03-10 | Open |
| [SYS-001-dual-pedal-sensing.md](safety/SYS-001-dual-pedal-sensing.md) | ASIL D pedal sensing, shared SPI bus CCF, 1oo2D architecture | 2026-02-27 | Closed |

### Testing

| File | Topic | Date | Status |
|------|-------|------|--------|
| [PROCESS-ci-test-hardening.md](testing/PROCESS-ci-test-hardening.md) | CI test hardening â€” LP64/ILP32, source inclusion, 99 failures in 4 rounds | 2026-02-28 | Closed |
| [PROCESS-bsw-tdd-development.md](testing/PROCESS-bsw-tdd-development.md) | BSW TDD â€” 16 modules, test-first hook, AUTOSAR layer ordering | 2026-02-22 | Closed |
| [PROCESS-sil-nightly-ci.md](testing/PROCESS-sil-nightly-ci.md) | SIL nightly CI â€” integration tests, COM bridge, verdict checker | 2026-03-01 | Closed |

### Infrastructure

| File | Topic | Date | Status |
|------|-------|------|--------|
| [PROCESS-misra-pipeline.md](infrastructure/PROCESS-misra-pipeline.md) | MISRA C:2012 pipeline â€” 1,536â†’0 violations, cppcheck CI gotchas | 2026-02-24 | Closed |
| [PROCESS-posix-vcan-porting.md](infrastructure/PROCESS-posix-vcan-porting.md) | POSIX/vCAN porting â€” HAL abstraction, SocketCAN, Docker per ECU | 2026-02-23 | Closed |
| [PROCESS-sil-demo-integration.md](infrastructure/PROCESS-sil-demo-integration.md) | SIL demo integration â€” Docker CAN, heartbeat wrap, plant sim tuning | 2026-03-01 | Closed |
| [PROCESS-fault-injection-demo.md](infrastructure/PROCESS-fault-injection-demo.md) | Fault injection & demo â€” deterministic faults, DTC, SAP QM, ML anomaly | 2026-03-01 | Closed |
| [PROCESS-rzc-heartbeat-overtransmit.md](infrastructure/PROCESS-rzc-heartbeat-overtransmit.md) | RZC heartbeat 5x over-transmit â€” Com_SendSignal signal ID vs PDU ID, timing bugs | 2026-03-01 | Closed |
| [PROCESS-simulated-relay-sil.md](infrastructure/PROCESS-simulated-relay-sil.md) | Simulated relay â€” SIL CAN broadcast, BSW routing tax, POSIX test guards, Docker as fault injection | 2026-03-02 | Closed |
| [PROCESS-asil-d-verification-gaps-local-execution.md](infrastructure/PROCESS-asil-d-verification-gaps-local-execution.md) | ASIL-D verification execution gaps â€” no-op root command, shell incompatibility, missing SIL deps | 2026-03-08 | Open |

### Process

| File | Topic | Date | Status |
|------|-------|------|--------|
| [PROCESS-hitl-review-methodology.md](process/PROCESS-hitl-review-methodology.md) | HITL review methodology â€” 443 comments, structured review, improvement cycle | 2026-03-01 | Closed |
| [PROCESS-cross-document-consistency.md](process/PROCESS-cross-document-consistency.md) | Cross-document consistency â€” CAN IDs, bit timing, sensor specs | 2026-02-26 | Closed |
| [PROCESS-traceability-automation.md](process/PROCESS-traceability-automation.md) | Traceability automation â€” trace-gen.py, CI enforcement, suspect links | 2026-02-25 | Closed |
| [PROCESS-architecture-decisions.md](process/PROCESS-architecture-decisions.md) | Architecture decisions â€” ADR framework, hybrid ECUs, file-based ALM | 2026-02-28 | Closed |
| [PROCESS-claude-rules-consolidation.md](process/PROCESS-claude-rules-consolidation.md) | Claude rules consolidation â€” 30â†’26 files, 3374â†’1999 lines, context window optimization | 2026-03-01 | Closed |
| [PROCESS-commercial-sil-vs-custom-sil-fidelity.md](process/PROCESS-commercial-sil-vs-custom-sil-fidelity.md) | Commercial SIL vs custom SIL â€” structural limits, fidelity classes, 30/60/90 closure backlog | 2026-03-10 | Open |

### Hardware

| File | Topic | Date | Status |
|------|-------|------|--------|
| [PROCESS-hardware-procurement.md](hardware/PROCESS-hardware-procurement.md) | Hardware procurement â€” BOM management, datasheet verification, budget | 2026-03-01 | Closed |
| [PROCESS-stm32-cubemx-bringup.md](hardware/PROCESS-stm32-cubemx-bringup.md) | STM32 CubeMX bringup â€” board selection, HSE 24 MHz, FDCAN bit timing, clock tree | 2026-03-03 | Open |

### Security

| File | Topic | Date | Status |
|------|-------|------|--------|
| [PROCESS-security-hardening.md](security/PROCESS-security-hardening.md) | 10-phase security hardening (web project, carried forward) | 2026-02-20 | Closed |

## Rules

- Every process topic or system requirement that undergoes HITL review MUST get a lessons-learned file here
- One file per topic â€” never combine unrelated lessons
- SYS-NNN files follow the standard template (see [ASPICE lessons-learned framework](../../aspice/system/lessons-learned/apps-web-overview.md))
- PROCESS files are free-form but must include: date, scope, key decisions, and key takeaways

