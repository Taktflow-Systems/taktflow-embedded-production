# Plan: Dashboard E2E Test Runner with Live Verdicts

## Context

The SIL dashboard at `taktflow-systems.com/embedded` currently supports **manual** fault injection only — click a button, visually observe the result. The existing `verdict_checker.py` + `run_sil.sh` runs 19 YAML scenarios in CI with pass/fail verdicts, but has no visual interface. For demos and engineering validation, the dashboard should run scenarios automatically with real-time pass/fail verdicts — turning it from a passive observation tool into an active E2E test platform.

**Value:**
- **Demo impact** — a visitor watches automated tests run with live pass/fail, proving the safety architecture works
- **Engineering** — real E2E verification with timing measurements, accessible without SSH/Docker
- **ASPICE evidence** — exportable test reports with traceability to safety goals (SWE.5)

## Architecture

No new Docker services. The fault-inject service (already has MQTT client) runs test scenarios, monitors MQTT for expected outcomes, and publishes progress to `taktflow/test/progress`. The ws_bridge naturally relays it to the dashboard at 10Hz. The frontend renders results reactively.

```
Dashboard                fault-inject :8091              MQTT broker
   |                          |                              |
   |-- POST /api/test/run --> |                              |
   |                          |-- trigger scenario (CAN) --> [vcan0]
   |                          |                              |
   |                          |<-- subscribe taktflow/#  ----|
   |                          |    (monitor verdicts)        |
   |                          |                              |
   |                          |-- publish test/progress ---->|
   |                          |                              |
   |<--- 10Hz WebSocket ------+--------- ws_bridge <--------|
   |    (data.test.progress)  |                              |
```

## Phases

| Phase | Name | Status |
|-------|------|--------|
| 1 | Backend — Test Specs + Runner | DONE |
| 2 | Backend — ws_bridge relay | DONE |
| 3 | Frontend — Test Runner UI | DONE |
| 4 | Fix — UDP pedal override for normal_drive | DONE |

### Phase 1: Backend — Test Specs + Runner (fault-inject service)

**New files:**

1. **`gateway/fault_inject/test_specs.py`** — verdict specifications for 8 testable scenarios

Each spec defines: scenario ID, safety goal reference (SG/HE/ASIL), prep scenario, observe window, and verdict checks (expected state transitions, DTCs, fault flags).

8 scenarios selected (injectable via CAN + produce clear MQTT-observable verdicts within ~10s):

| # | Scenario | Primary Verdict | Observe |
|---|----------|----------------|---------|
| 1 | overcurrent | DEGRADED + DTC 0xE301 | 5s |
| 2 | estop | SAFE_STOP within 200ms | 3s |
| 3 | steer_fault | SteerFaultStatus=1 | 3s |
| 4 | brake_fault | BrakeFaultStatus=1 | 3s |
| 5 | battery_low | DTC 0xE401 + DEGRADED/LIMP | 8s |
| 6 | motor_reversal | SAFE_STOP | 4s |
| 7 | runaway_accel | DEGRADED (torque limited) | 4s |
| 8 | creep_from_stop | SAFE_STOP | 4s |

Excluded: `heartbeat_loss`/`can_loss` (require `docker stop`), `babbling_node` (no clean MQTT verdict), `normal_drive`/`reset` (control primitives).

Total suite: ~60s with 2s reset between each.

2. **`gateway/fault_inject/test_runner.py`** — MQTT verdict monitor + test execution

- `MQTTVerdictMonitor`: subscribes to `taktflow/#`, tracks vehicle state, DTCs, fault flags, anomaly score
- `DashboardTestRunner`: runs scenarios sequentially in a daemon thread
  - For each scenario: prep (normal_drive) → inject fault → observe for verdicts → reset → publish progress
  - Publishes to `taktflow/test/progress` (retain=False) after each verdict check
  - Publishes to `taktflow/test/result` (retain=True) on completion
  - Mutex: only one run at a time (409 if already running)

**Modified files:**

3. **`gateway/fault_inject/app.py`** — add 3 endpoints:

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/api/test/run` | Start test suite (requires control lock). Body: `{"tests": [...]}` optional |
| `GET` | `/api/test/status` | Current run state (running/idle/complete) |
| `GET` | `/api/test/result` | Last completed run result JSON |

### Phase 2: Backend — ws_bridge relay (~15 lines)

**Modified file:**

4. **`gateway/ws_bridge/bridge.py`** — add 2 MQTT topic handlers + 2 snapshot fields:

- `taktflow/test/progress` → `state.test_progress`
- `taktflow/test/result` → `state.test_result`
- Add `test: { progress, result }` to `to_snapshot()`

### Phase 3: Frontend — Test Runner UI

**Modified file:**

5. **`apps/web/lib/hooks/useTelemetry.ts`** — extend `TelemetrySnapshot` with test types:

```typescript
test?: {
  progress: TestProgress | null;
  result: TestResult | null;
};
```

**New file:**

6. **`apps/web/app/embedded/components/TestRunner.tsx`** — self-contained panel with 3 states:

- **Idle**: "Run Test Suite (~60s)" button (disabled without control lock), last result summary if available
- **Running**: progress bar, current scenario + phase badge, growing results list with per-verdict details
- **Complete**: pass/fail summary, full results table, "Export JSON" button, "Run Again" button

Each scenario row shows: label, ASIL badge, SG/HE reference, pass/fail icon, timing. Expandable to show individual verdict checks (expected vs observed).

**Modified file:**

7. **`apps/web/app/embedded/components/TelemetryDashboard.tsx`** — import + render `TestRunner` between fault injection panel and IsoCar:

```tsx
<TestRunner testData={data.test ?? null} isController={isController} sessionId={sessionId} />
```

**Export**: client-side JSON download with ASPICE metadata (SWE.5 reference, ISO 26262 Part 6 Section 10, run_id, timestamp, all verdict evidence).

## Data Structures

### Progress message (`taktflow/test/progress`, retain=False)

```json
{
  "state": "running",
  "run_id": "uuid",
  "total": 8,
  "current_index": 2,
  "current_id": "overcurrent",
  "current_label": "Overcurrent",
  "current_phase": "observing",
  "elapsed_sec": 12.4,
  "results": [
    {
      "id": "overcurrent", "label": "Overcurrent",
      "sg": "SG-001", "asil": "D", "he": "HE-001",
      "passed": true, "duration_sec": 8.2,
      "verdicts": [
        { "description": "DTC 0xE301 broadcast", "expected": "DTC received",
          "observed": "DTC 0xE301 at t=3.1s", "passed": true, "elapsed_ms": 3100 }
      ]
    }
  ]
}
```

### Final result (`taktflow/test/result`, retain=True)

Same as progress but with `state: "complete"` and full `summary: { passed, failed, total, duration_sec, timestamp }`.

## Phase 4: Fix — UDP Pedal Override for normal_drive

**Problem**: `normal_drive` (and all torque-injecting scenarios) didn't work because
the CVC sends Torque_Request (0x101) every 10ms from its pedal sensor input.
`Spi_Posix.c` hardcoded pedal angle in dead zone (200-800) → torque=0.
Fault-inject's one-shot CAN frames got overwritten within 10ms.

**Fix**: Added UDP socket to `Spi_Posix.c` (env-var gated via `SPI_PEDAL_UDP_PORT`)
that accepts 2-byte uint16 LE packets to override the simulated pedal angle.
Fault-inject sends pedal overrides at the MCAL layer; the CVC processes the value
through its full pipeline (Swc_Pedal plausibility, ramp limit, torque lookup).

**Protocol**: 2 bytes uint16 LE — angle 0-16383 (14-bit AS5048A), or 0xFFFF to clear.

| File | Action |
|------|--------|
| `firmware/shared/bsw/mcal/posix/Spi_Posix.c` | MODIFY (UDP socket + override logic) |
| `gateway/fault_inject/pedal_udp.py` | CREATE (UDP helper: send/clear/convert) |
| `gateway/fault_inject/scenarios.py` | MODIFY (normal_drive + reset use pedal override) |
| `docker/docker-compose.yml` | MODIFY (add SPI_PEDAL_UDP_PORT=9100 to CVC) |

## File Changes Summary

| File | Action |
|------|--------|
| `gateway/fault_inject/test_specs.py` | CREATE |
| `gateway/fault_inject/test_runner.py` | CREATE |
| `gateway/fault_inject/pedal_udp.py` | CREATE |
| `gateway/fault_inject/app.py` | MODIFY (3 endpoints) |
| `gateway/ws_bridge/bridge.py` | MODIFY (~15 lines) |
| `apps/web/lib/hooks/useTelemetry.ts` | MODIFY (types) |
| `apps/web/app/embedded/components/TestRunner.tsx` | CREATE |
| `apps/web/app/embedded/components/TelemetryDashboard.tsx` | MODIFY (import + render) |
| `firmware/shared/bsw/mcal/posix/Spi_Posix.c` | MODIFY (UDP pedal override) |
| `docker/docker-compose.yml` | MODIFY (CVC env var) |

## What Does NOT Change

- Existing manual fault injection buttons (untouched)
- Existing control lock mechanism (reused — test run requires the lock)
- Existing 19 YAML scenarios + `verdict_checker.py` (CI mechanism stays separate)
- WebSocket URL, protocol, 10Hz rate (unchanged)
- No new Docker services, no new Python packages

## Verification

- [ ] Deploy updated fault-inject + ws_bridge containers to Netcup
- [ ] Deploy updated web app to Vercel
- [ ] Open dashboard, take control, click "Run Test Suite"
- [ ] All 8 scenarios run sequentially with real-time progress
- [ ] Pass/fail verdicts match expected behavior for each scenario
- [ ] "Export JSON" produces valid ASPICE evidence file
- [ ] Manual fault injection still works independently
- [ ] Test runner rejects requests without control lock (403)
- [ ] Concurrent test run attempts return 409
