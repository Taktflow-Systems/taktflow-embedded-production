# Lessons Learned — SIL Demo Integration

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-23 to 2026-03-01
**Scope:** Building a live SIL demo with 7 Docker ECUs, plant simulator, MQTT gateway, WebSocket bridge, ML anomaly detection, and fault injection
**Result:** Live demo at sil.taktflow-systems.com

---

## 1. Docker Compose v2 Syntax

Docker Compose v2 uses `docker compose` (space), not `docker-compose` (hyphen). CI and scripts broke when mixing the two.

**Lesson:** Always use `docker compose` (v2). Check all scripts, CI workflows, and docs for the old hyphenated form.

---

## 2. NET_RAW Capability for CAN in Containers

Docker containers need `cap_add: [NET_RAW]` to create and use CAN sockets. Without it, `socket(AF_CAN, ...)` fails silently or with a cryptic permission error.

**Fix:** Add to docker-compose.yml:
```yaml
cap_add:
  - NET_RAW
```

**Lesson:** CAN socket operations require NET_RAW. This is not obvious from the error message.

---

## 3. vcan Module Auto-Load and Error Clearing

The virtual CAN setup script must:
1. `modprobe vcan` — load the kernel module
2. `ip link add vcan0 type vcan` — create the interface
3. `ip link set vcan0 up` — bring it up
4. Handle restarts: `ip link set vcan0 down && ip link delete vcan0` before recreating

**Lesson:** Always handle the "already exists" case. Idempotent setup scripts prevent restart failures.

---

## 4. Heartbeat Alive Counter Wrap-Around

The E2E alive counter is 4-bit (0-15), not 8-bit. Code that checked `counter + 1 == expected` broke at the 15→0 wrap boundary.

**Fix:** `(counter + 1) % 16 == expected` or `(counter + 1) & 0x0F`.

**Lesson:** Always use modular arithmetic for wrap-around counters. Define the wrap constant, don't assume 8-bit.

---

## 5. DBC File vs Firmware Signal Encoding Mismatch

The DBC file defined steering angle with offset 0, but firmware encoded with offset -45.0. The SIL dashboard showed wrong steering values.

**Lesson:** DBC files must be auto-generated from the CAN message matrix, not hand-written. Any manual DBC = drift.

---

## 6. Duplicate Heartbeat Monitors Cause Flickering

Two components (SC and dashboard) both running heartbeat timeout detection. When one detected timeout slightly before the other, the dashboard flickered between ALIVE and TIMEOUT states.

**Fix:** Single source of truth for heartbeat state. Dashboard reads SC's determination, doesn't run its own.

**Lesson:** One monitor per signal. Duplicate monitors with different timing create oscillation.

---

## 7. RTE_MAX_SIGNALS Too Small — Silent Failure

RTE was configured with `RTE_MAX_SIGNALS = 32`. When the 33rd signal was registered, `Rte_Init()` silently failed — no error return, just dropped signals.

**Fix:** Increased to 48. Added runtime assertion on signal count.

**Lesson:** Configuration limits must be validated at init time with a clear error. Silent overflow is the worst failure mode.

---

## 8. Plant Simulator Thermal Model

Copying textbook thermal equations doesn't work. The plant simulator's thermal model caused:
- Thermal runaway after overcurrent (thermal mass too low)
- Cooling never completed (ambient coupling too weak)

**Fix:** Tune thermal time constants to match the demo's 10-second timescale, not real motor physics.

**Lesson:** Simulation models serve the demo, not physics. Tune for observability, not accuracy.

---

## 9. ML Anomaly Score Stuck at Baseline

The ML anomaly detector output 0.53 (baseline) for all states because the feature vector was state-unaware — it used the same thresholds for idle and full-load.

**Fix:** State-aware thresholds: different baselines for INIT, RUN, DEGRADED.

**Lesson:** ML models need state context. A one-size-fits-all threshold detects nothing.

---

## 10. E-STOP Event Spam on Reset

When clearing an E-stop, the system re-entered the E-stop state because the clear message triggered a state transition that re-evaluated the E-stop condition.

**Fix:** Proper fault clear sequence: (1) clear fault flag, (2) wait one cycle, (3) re-evaluate.

**Lesson:** Fault clear and fault detect must not run in the same cycle. Sequence matters.

---

## 11. Controller-Viewer Lock for Multi-User

Multiple users controlling the demo simultaneously caused conflicting commands. Added a lock: first user to send a control command gets exclusive control for 2 minutes.

**Lesson:** Multi-user demos need access control. Default lock duration: short enough to not block, long enough to complete a scenario.

---

## 12. Caddyfile Host Network Mode

When Docker uses `network_mode: host`, Caddy must listen on `localhost`, not `0.0.0.0`, or it conflicts with the host's port 80.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Docker CAN | NET_RAW capability required; not obvious from errors |
| vcan setup | Idempotent scripts — handle "already exists" |
| Counter wrap | Always use modular arithmetic; define the modulus |
| DBC files | Auto-generate from matrix, never hand-write |
| Duplicate monitors | One monitor per signal — no parallel detection |
| Config limits | Validate at init, fail loudly on overflow |
| Simulation models | Tune for demo timescale, not physics |
| ML thresholds | State-aware, not one-size-fits-all |
| Fault sequencing | Clear and detect must not run in the same cycle |
