# Lessons Learned — E-Stop Recovery and Startup Sequencing

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Date:** 2026-03-02
**Scope:** E-stop byte layout bug, power-cycle reset, container restart ordering, INIT hold time
**Result:** Clean E-stop → reset → INIT (5s) → RUN with zero spurious faults

---

## 1. Sub-Byte CAN Signal Packing Is Dangerous When Com Reads Full Bytes

**Problem:** The gateway `_estop_frame()` packed `EStop_Active` (1 bit) and `EStop_Source` (3 bits) into the same byte:

```python
# OLD — packed into byte[2]
payload[2] = (active & 0x01) | ((source & 0x07) << 1)
# active=0, source=1 → byte[2] = 0x02
```

FZC's AUTOSAR Com module was configured with `bitPos=16, bitSize=8` — reading the **full byte** as `estop_active`. Value `0x02 != 0` → E-stop falsely triggered.

**Fix:** Separate signals into distinct bytes:

```python
# NEW — separate bytes
payload[2] = 1 if active else 0   # EStop_Active (byte 2)
payload[3] = source & 0xFF         # EStop_Source (byte 3)
```

**Lesson:** When a CAN signal has `bitSize=8` in the Com config, the entire byte is the signal. Packing multiple logical values into that byte breaks the receiver's interpretation. Always match the gateway frame builder to the receiver's Com signal layout — check `bitPos`, `bitSize`, and `byteOrder` in `*_ComCfg.c` before building frames.

**Corollary:** Update the DBC file whenever the frame layout changes. The DBC is documentation for humans and tools (canutils, CANoe) — if it disagrees with the Com config, bugs hide.

---

## 2. Software Reset Cannot Clear Firmware-Latched Faults

**Problem:** After E-stop, FZC latches `Brake_FaultLatched = TRUE` permanently (by design — ASIL D requires persistent fault memory). Clearing the E-stop CAN frame doesn't clear FZC's internal latch. On next CVC boot, FZC still reports brake fault → CVC enters SAFE_STOP again.

**Fix:** Power-cycle reset = restart Docker containers via Docker API:

```python
client = docker.from_env()
container = client.containers.get("docker-fzc-1")
container.restart(timeout=5)
```

Required Docker socket mount in `docker-compose.yml`:
```yaml
volumes:
  - /var/run/docker.sock:/var/run/docker.sock
```

**Lesson:** In a system with ASIL-rated fault latching, the only reliable reset is a power cycle. A "soft reset" (clearing CAN frames, MQTT commands) is not equivalent to a power cycle because firmware static variables retain their latched state. For SIL environments, container restart is the Docker equivalent of power cycling an ECU.

---

## 3. Container Restart Order Determines Startup Fault Profile

**Problem:** Restarting all containers simultaneously caused CVC to start before zone controllers. During the 2-3 seconds while FZC/RZC were still booting, CVC detected heartbeat timeouts → LIMP → DEGRADED transitions.

**Fix:** Restart zone controllers first, wait 2 seconds, then restart CVC last:

```python
_ZONE_CONTAINERS = [
    "docker-fzc-1", "docker-rzc-1", "docker-sc-1",
    "docker-bcm-1", "docker-icu-1", "docker-tcu-1",
    "docker-plant-sim-1",
]
_CVC_CONTAINER = "docker-cvc-1"

# Restart zones first
for name in _ZONE_CONTAINERS:
    client.containers.get(name).restart(timeout=5)
time.sleep(2)
# CVC last
client.containers.get(_CVC_CONTAINER).restart(timeout=5)
```

**Lesson:** In a master-slave ECU architecture, the master (CVC) should always start **after** its slaves (zone controllers). If the master starts first, it will timeout on heartbeats from slaves that haven't booted yet. This applies to real hardware power-on sequencing too — not just Docker.

---

## 4. INIT Hold Time Absorbs Startup Transients

**Problem:** Even with restart ordering, there's a brief window where heartbeats show `TIMEOUT -> OK` as zone controllers start sending. If CVC transitions to RUN too quickly, any late-starting ECU causes a fault.

**Fix:** Hold CVC in INIT state for 500 cycles (5 seconds) before allowing INIT → RUN:

```c
#define CVC_INIT_HOLD_CYCLES 500u  /* 500 × 10ms = 5 seconds */

if (current_state == CVC_STATE_INIT) {
    if (init_hold_counter < CVC_INIT_HOLD_CYCLES) {
        init_hold_counter++;
    }
    if ((self_test_pass_pending == TRUE)
        && (init_hold_counter >= CVC_INIT_HOLD_CYCLES)
        && (fzc_comm == CVC_COMM_OK)
        && (rzc_comm == CVC_COMM_OK)) {
        current_state = CVC_STATE_RUN;
    }
}
```

The transition requires **both** conditions:
1. Hold timer expired (5 seconds minimum)
2. All heartbeats confirmed OK

**Lesson:** INIT hold time is a simple, effective pattern for absorbing startup transients in multi-ECU systems. In real automotive ECUs, this maps to the ECU "startup phase" where the state machine waits for all dependencies before entering normal operation. The hold time should be longer than the worst-case boot time of the slowest dependent ECU.

---

## 5. Defense in Depth: Three Layers Beat One

The clean startup required all three fixes working together:

| Layer | What it prevents | Alone sufficient? |
|-------|-----------------|-------------------|
| Restart ordering | Zone controllers booting late | No — race conditions still possible |
| INIT hold time | Transient timeouts during boot | No — doesn't help if fault is latched |
| Power-cycle reset | Latched firmware faults persisting | No — still get transient faults on restart |

**Lesson:** For startup reliability, apply defense in depth:
1. **Ordering** — start dependencies before dependents
2. **Hold time** — don't act on signals until system is stable
3. **Clean state** — ensure all components start from a known-good state

Any single layer can be defeated by edge cases. All three together produce a robust startup.

---

## 6. Fault-Inject API: X-Client-Id Header for Lock Validation

**Gotcha:** The fault-inject API uses a control lock (2-minute timeout) to prevent concurrent access. The lock is acquired with a JSON body containing `client_id`, but subsequent requests validate the lock holder via the `X-Client-Id` HTTP header, not a body field.

```bash
# Acquire lock (JSON body)
curl -X POST /api/fault/control/acquire -d '{"client_id":"test"}'

# Use lock (header)
curl -X POST /api/fault/scenario/estop -H 'X-Client-Id: test'
```

**Lesson:** When designing multi-step API flows (acquire → use → release), keep the authentication mechanism consistent. Either always use headers or always use body fields. Mixed approaches cause confusion.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| CAN byte packing | Match frame builder to receiver Com config — check bitPos/bitSize |
| DBC maintenance | Update DBC whenever frame layout changes — it's the human-readable contract |
| Fault latching | Power cycle is the only reliable reset for ASIL-rated latched faults |
| Restart ordering | Start slaves before master in multi-ECU systems |
| INIT hold time | Wait for all dependencies before allowing normal operation |
| Defense in depth | Combine ordering + hold time + clean state for robust startup |
| API consistency | Keep auth mechanism consistent across multi-step API flows |
