# Plan: SIL + HIL + Cloud Endgame

**Date:** 2026-03-21
**Status:** PLANNING
**Goal:** Both SIL (Docker on Netcup VPS) and HIL (physical boards on laptop) running simultaneously, HIL connected to cloud via MQTT, live dashboard at sil.taktflow-systems.com

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Netcup VPS (152.53.245.209)          sil.taktflow-systems.com │
│  ┌──────────────────────────────────┐                        │
│  │  Docker Compose (SIL)            │                        │
│  │  ┌─────┐ ┌─────┐ ┌─────┐       │                        │
│  │  │ CVC │ │ FZC │ │ RZC │  vcan0│                        │
│  │  └──┬──┘ └──┬──┘ └──┬──┘       │                        │
│  │  ┌──┴───────┴───────┴──┐       │                        │
│  │  │     CAN Gateway     │───MQTT─┤──┐                     │
│  │  └─────────────────────┘       │  │                     │
│  │  ┌─────┐ ┌─────┐ ┌─────┐ ┌───┐│  │  ┌──────────────┐  │
│  │  │ BCM │ │ ICU │ │ TCU │ │ SC ││  ├──│  MQTT Broker  │  │
│  │  └─────┘ └─────┘ └─────┘ └───┘│  │  └──────┬───────┘  │
│  │  ┌──────────┐ ┌────────────┐   │  │         │          │
│  │  │ PlantSim │ │ FaultInject│   │  │  ┌──────┴───────┐  │
│  │  └──────────┘ └────────────┘   │  │  │  WS Bridge   │  │
│  └──────────────────────────────────┘  │  └──────┬───────┘  │
│                                        │         │          │
│  ┌──────────┐  ┌───────────┐          │  ┌──────┴───────┐  │
│  │ ML Infer │  │ SAP QM    │          │  │    Caddy      │  │
│  └──────────┘  └───────────┘          │  │  (HTTPS/WSS)  │  │
│                                        │  └──────┬───────┘  │
└────────────────────────────────────────┘         │          │
                                                    │          │
                               ┌────────────────────┘          │
                               ▼                               │
                    Browser: sil.taktflow-systems.com          │
                    (TelemetryDashboard + IsoCar)              │
                                                               │
┌──────────────────────────────────────────────────────────────┘
│
│  Laptop (192.0.2.30)                      HIL Bench
│  ┌──────────────────────────────────────────────────────┐
│  │  Physical CAN Bus (500kbps, 120Ω terminated)         │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐             │
│  │  │CVC G474 │  │FZC G474 │  │RZC F413 │  ←STM32     │
│  │  │ SN:001A │  │ SN:0027 │  │ SN:0670 │             │
│  │  └────┬────┘  └────┬────┘  └────┬────┘             │
│  │       └─────────────┴─────────────┘                  │
│  │                    │                                  │
│  │              ┌─────┴─────┐                           │
│  │              │ PCAN-USB  │                           │
│  │              └─────┬─────┘                           │
│  │                    │                                  │
│  │  ┌─────────────────┴──────────────────┐              │
│  │  │  CAN Gateway (laptop)              │              │
│  │  │  PCAN → MQTT → Netcup VPS         │              │
│  │  │  (same protocol as SIL gateway)    │              │
│  │  └────────────────────────────────────┘              │
│  │                                                       │
│  │  ┌──────────────────┐                                │
│  │  │ TCP CAN Bridge   │ ← Windows CAN Monitor         │
│  │  │ port 9876        │                                │
│  │  └──────────────────┘                                │
│  └──────────────────────────────────────────────────────┘
```

## Phases

### Phase 0: SC Audit + Plant-Sim/Fault-Inject Rewrite
**Prereq:** SC code reviewed (DONE — E2E matches, CAN config correct)
**Goal:** SC integration verified, plant-sim/fault-inject work with new firmware

Steps:
1. Run SC 11 unit tests — verify all compile and pass
2. Run SC alongside CVC+FZC+RZC on vcan — verify SC heartbeat (0x013) + E2E cross-check
3. Rewrite plant-sim to use cantools DBC encode (not hardcoded bytes)
4. Rewrite fault-inject to use cantools DBC encode + proper E2E headers
5. Both must generate valid E2E with correct sub-byte packing

**Gate:** SC E2E interop verified, plant-sim/fault-inject produce DBC-valid frames

**SC Audit Results (2026-03-21):**
- E2E CRC: matches BSW (poly=0x1D, init=0xFF, xor_out=0xFF) ✓
- E2E byte layout: [counter:4|dataId:4][CRC][payload] ✓
- CanIf config: matches DBC (0x013 TX, 7 RX heartbeats + E-Stop) ✓
- Heartbeat monitoring: timeout + confirmation + latch + recovery ✓
- Relay: kill latch (power-cycle only reset) ✓
- GAP: no cross-ECU integration test yet

### Phase 1: Docker SIL on Netcup (rebuild with new firmware)
**Prereq:** Phase 0 complete
**Goal:** 16 SIL scenarios pass on Netcup

Steps:
1. Update Dockerfiles to build from current firmware (new Com, E2E, CanSM, etc.)
2. `docker compose build --no-cache` on Netcup
3. `docker compose up` — verify 27 CAN IDs on vcan0
4. Run 16 SIL scenarios via `test/sil/run_sil.py`
5. Verify web dashboard shows live telemetry at sil.taktflow-systems.com

**Gate:** 16/16 SIL scenarios PASS, dashboard live

### Phase 2: Fix FTTI violations in DBC
**Prereq:** FTTI analysis (DONE — `docs/safety/analysis/ftti-complete.md`)
**Goal:** 0 FTTI violations

Steps:
1. Update DBC `BA_ "E2E_MaxDeltaCounter"` per message (table in FTTI doc)
2. Run pipeline: DBC → ARXML → codegen → validate → syntax check
3. Rebuild all 7 ECUs on laptop
4. Rerun Layer 5b comprehensive (71 tests)
5. Redeploy to Netcup Docker

**Gate:** Pipeline green, all tests pass, FTTI analysis shows 0 violations

### Phase 3: HIL bench on laptop
**Prereq:** STM32 boards (CVC G474, FZC G474, RZC F413), PCAN-USB adapter
**Goal:** 3 ECUs on physical CAN bus, heartbeats exchanging

Steps:
1. Flash CVC to SN:001A (G474RE) via ST-LINK
2. Flash FZC to SN:0027 (G474RE)
3. Flash RZC to SN:0670 (F413ZH)
4. Connect 3 boards + PCAN-USB on same CAN bus (120Ω termination)
5. Verify with CAN monitor app (select PCAN serial port)
6. Run HIL smoke test: all 3 heartbeats, E2E counters, timing

**Gate:** 3-ECU CAN communication on physical bus, CAN monitor shows decoded signals

### Phase 4: HIL → Cloud via MQTT
**Prereq:** HIL bench working, MQTT broker on Netcup
**Goal:** Physical CAN data appears on sil.taktflow-systems.com dashboard

Steps:
1. Run CAN gateway on laptop: PCAN-USB → MQTT → Netcup
2. Gateway uses same protocol as SIL Docker gateway (JSON over MQTT)
3. Web dashboard switches between "SIL" and "HIL" data source
4. Verify: flash LED on board → see state change on dashboard

**Gate:** Physical board data visible on web dashboard in real-time

### Phase 5: Parallel SIL + HIL
**Prereq:** Both SIL and HIL working independently
**Goal:** Both running simultaneously, dashboard shows both

Steps:
1. SIL on Netcup Docker (vcan0) — always running
2. HIL on laptop (physical CAN) — connected when bench is powered
3. Dashboard has SIL/HIL toggle or split view
4. 16 SIL scenarios run nightly on Netcup
5. HIL scenarios run manually on bench

**Gate:** Both data sources visible on dashboard, no interference

### Phase 6: Full test automation
**Prereq:** Phase 5 complete
**Goal:** CI triggers SIL tests, manual HIL tests with report

Steps:
1. GitHub Actions workflow: `sil-nightly.yml` SSH to Netcup, run scenarios
2. HIL test script: `test/hil/run_hil.py` — connects to PCAN, runs scenarios
3. Test reports: HTML/Markdown with pass/fail, signal traces
4. Coverage report: which safety goals are tested by SIL vs HIL

**Gate:** Nightly SIL green, HIL test report generated

## Infrastructure

| Component | Location | Status |
|-----------|----------|--------|
| Netcup VPS | 152.53.245.209 | Running (8GB RAM, 4 vCPU) |
| MQTT Broker | Netcup Docker | Running (Mosquitto) |
| WS Bridge | Netcup Docker | Running |
| Caddy (HTTPS) | Netcup Docker | Running |
| Web Dashboard | sil.taktflow-systems.com | Running |
| CAN Bridge (TCP) | Laptop :9876 | Running (systemd) |
| STM32 boards | Laptop USB | Available (3 boards) |
| PCAN-USB | Laptop USB | Available |

## Timeline

| Phase | Effort | Blocks |
|-------|--------|--------|
| 1: Docker SIL rebuild | 2-3 hours | Docker build issues |
| 2: FTTI fix | 1-2 hours | Pipeline validation |
| 3: HIL bench | 2-3 hours | Flash issues, CAN wiring |
| 4: HIL → Cloud | 1-2 hours | MQTT config |
| 5: Parallel | 1 hour | Dashboard toggle |
| 6: Automation | 2-3 hours | CI SSH, test scripts |

## Key Decisions

1. **SIL and HIL use the SAME firmware** — only the platform layer (POSIX vs STM32) differs
2. **SIL and HIL use the SAME CAN gateway protocol** — JSON over MQTT
3. **Dashboard shows BOTH** — no separate SIL/HIL dashboards
4. **FTTI must be fixed BEFORE HIL** — wrong timing parameters on real hardware is dangerous
5. **Docker rebuild BEFORE anything** — current Netcup Docker has old firmware
