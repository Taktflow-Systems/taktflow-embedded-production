# Lessons Learned — Fault Injection and Live Demo

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-23 to 2026-03-01
**Scope:** Building fault injection, DTC broadcast, SAP QM integration, ML anomaly detection, and live demo infrastructure
**Result:** 6 injectable faults, SAP QM mock, ML anomaly scoring, live at sil.taktflow-systems.com

---

## 1. Fault Injection Must Be Deterministic

Early fault injection was random — inject a fault, hope the system detects it. This made demo unreliable.

**Fix:** Each fault has a defined injection → detection → reaction → recovery sequence:

| Fault | Inject | Detect (ms) | Reaction | Recovery |
|-------|--------|-------------|----------|----------|
| Overcurrent | Set motor current > 25A | < 20 | Motor disable | Manual reset |
| Steering fault | Command/feedback deviation > 5° | < 50 | Return to center | Fault clear |
| Brake fault | Servo disconnect (no current) | < 20 | Motor cutoff request | Manual reset |
| E-Stop | Button press | < 10 | All actuators off | Power cycle |
| CAN loss | Stop heartbeat TX | < 200 | Kill relay open | Power cycle |
| Lidar emergency | Obstacle < 20 cm | < 30 | Emergency brake | Obstacle removed |

**Lesson:** Deterministic fault sequences are demo-able and testable. Random faults are neither.

---

## 2. DTC Broadcast Needs Arbitration Backoff

When multiple ECUs detect faults simultaneously, they all try to broadcast DTCs on CAN ID 0x500. CAN arbitration handles it, but rapid-fire DTCs from multiple ECUs flood the bus.

**Fix:** Randomized backoff (0-50 ms) before DTC transmission. Event-driven with exponential backoff on collision.

**Lesson:** Shared CAN IDs for event messages need backoff. Design for the "everything fails at once" scenario.

---

## 3. SAP QM Field Name Mismatch

The SAP QM mock expected `notification_type`, but the gateway sent `notificationType` (camelCase vs snake_case).

**Fix:** Standardize on snake_case for all SAP QM API fields (matching SAP's actual API convention).

**Lesson:** API field naming mismatches are the #1 integration bug. Define the contract (schema) before writing any code.

---

## 4. Fault Detection Timing vs FTTI

The demo validates that fault detection time stays within FTTI:
- Overcurrent: 20 ms detection < 100 ms FTTI ✓
- CAN loss: 200 ms detection < 500 ms FTTI ✓

**Lesson:** Fault injection is not just a demo feature — it's safety validation evidence. Log timestamps for FDTI measurement.

---

## 5. Vehicle State Machine Must Handle All Faults

The vehicle state machine (VehicleState) initially only transitioned on explicit commands. Subsystem faults (motor, steering, brake) didn't trigger state transitions.

**Fix:** Added fault-triggered transitions:
- Motor overcurrent → DEGRADED
- Steering fault → SAFE_STOP (no mechanical fallback)
- Brake fault → SAFE_STOP
- CAN loss → SAFE_STOP
- E-Stop → SAFE_STOP

**Lesson:** Every fault path must have a defined state transition. Faults that don't change the vehicle state are invisible to the operator.

---

## 6. ML Anomaly Detection Needs State Context

The Isolation Forest model produced a fixed anomaly score (0.53) for all operating states because it was trained on combined normal/fault data without state labels.

**Fix:** State-aware scoring:
- INIT: baseline 0.2 (low activity normal)
- RUN: baseline 0.3 (active operation normal)
- DEGRADED: baseline 0.5 (elevated readings expected)

**Lesson:** Anomaly detection without context is noise detection. The model must know what "normal" means for each operating state.

---

## 7. Live Demo Infrastructure (Netcup VPS)

**Stack:** Caddy (reverse proxy) → Docker Compose → 7 ECU containers + gateway + plant simulator

**Gotchas:**
- Caddy needs `localhost` binding with host network mode
- Docker Compose v2 syntax (`docker compose` not `docker-compose`)
- NET_RAW capability for CAN sockets
- Controller-viewer lock for multi-user access (2-minute timeout)

**Lesson:** A live demo is 10x more impressive than a video. The infrastructure investment pays off in every interview and application.

---

## 8. Reset Sequence Matters

After clearing a fault, the system must follow a specific sequence:
1. Clear the fault flag
2. Wait one control cycle (10 ms)
3. Re-evaluate fault conditions
4. Only then allow state transition

**Without this:** Clearing a fault immediately re-triggers it because the clear action itself generates a state change event that re-evaluates the still-present condition.

**Lesson:** Fault clear and fault detect must be temporally separated. Never clear and evaluate in the same cycle.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Deterministic faults | Defined inject→detect→react→recover sequence for every fault |
| DTC arbitration | Shared CAN IDs need randomized backoff for simultaneous events |
| API contracts | Define field naming convention before writing integration code |
| FTTI validation | Fault injection is safety validation evidence — log timestamps |
| State transitions | Every fault must trigger a defined vehicle state change |
| ML context | Anomaly detection without state context is noise detection |
| Live demo | 10x more impressive than a video — worth the infrastructure investment |
| Reset sequence | Separate fault clear and fault evaluate by at least one cycle |
