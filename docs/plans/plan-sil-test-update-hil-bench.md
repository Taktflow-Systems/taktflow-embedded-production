# Next Session Plan

**Date:** 2026-03-21 (written end of marathon session)
**Status:** READY TO EXECUTE

## Current State (verified working)

- QNX Pi 4: CVC + FZC + RZC + SC (4 safety ECUs, native QNX processes)
- Docker laptop: BCM + TCU + ICU + plant-sim + MQTT (6 containers)
- TCP bridge: QNX port 9879 → laptop relay → vcan0
- All 8 heartbeats verified at correct rates
- 27 CAN IDs, 862 frames/s
- 1,021 unit tests in CI (GREEN)
- CAN monitor app: select "QNX Pi 4 SIL" (update port to 9879)

## How to Start the SIL

### QNX Pi (192.168.137.184, user: qnxuser)
```bash
ssh qnxuser@192.168.137.184
# Kill any leftovers
slay -f -s SIGKILL cvc_qnx fzc_qnx rzc_qnx sc_qnx python3 2>/dev/null
sleep 2
# Start 4 safety ECUs
/tmp/cvc_qnx udp &
/tmp/fzc_qnx udp &
/tmp/rzc_qnx udp &
/tmp/sc_qnx &
sleep 1
# Start TCP bridge (port 9879)
python3 /tmp/can_bridge_qnx.py &
```

### Laptop (192.168.0.158, user: an-dao)
```bash
ssh an-dao@192.168.0.158
# Ensure eno1 has IP for QNX Pi network
sudo ip addr add 192.168.137.100/24 dev eno1 2>/dev/null
# Start relay (QNX → vcan0)
nohup python3 -u ~/tcp_to_vcan.py > /tmp/relay.log 2>&1 &
# Start Docker QM ECUs
cd ~/taktflow-embedded-production
sudo docker compose -f docker/docker-compose.laptop.yml up -d
```

### Verify
```bash
# On laptop:
timeout 3 candump vcan0 | grep " 010 \| 013 \| 016 "
# Should see CVC (0x010), SC (0x013), BCM (0x016)
```

## Task 1: Update SIL Test Scripts (2 hours)

**Blocker:** SIL tests read raw CAN bytes with hardcoded offsets.
Sub-byte packing fix changed byte layout. Tests see wrong values.

**Fix:** Replace all raw byte reads with `cantools.database.decode()`:
```python
# OLD (broken):
mode = data[2]  # Gets Mode + FaultMask mixed

# NEW (correct):
import cantools
db = cantools.database.load_file("gateway/taktflow_vehicle.dbc")
decoded = db.get_message_by_frame_id(0x100).decode(data, decode_choices=False)
mode = decoded["Vehicle_State_Mode"]
```

**Files to update:**
- `test/sil/test_vsm_fault_transitions.py` — MQTT fault injection + CAN check
- `test/sil/test_battery_chain.py` — battery fault chain
- `test/sil/test_overtemp_hops.py` — overtemp fault chain
- `test/sil/verdict_checker.py` — generic verdict checking

**Pattern:** Use `gateway.lib.dbc_encoder.CanEncoder` (already written, 19/19 tests).

## Task 2: Run 16 SIL Scenarios (1 hour)

After test scripts updated:
```bash
cd ~/taktflow-embedded-production
python3 test/sil/test_vsm_fault_transitions.py
python3 test/sil/test_battery_chain.py
python3 test/sil/test_overtemp_hops.py
```

## Task 3: HIL Bench (2-3 hours)

Flash STM32 boards with current firmware:
- CVC: G474RE (SN:001A, COM3) — `make flash TARGET=cvc`
- FZC: G474RE (SN:0027, COM7) — `make flash TARGET=fzc`
- RZC: F413ZH (SN:0670, COM15) — `make flash TARGET=rzc`

Verify on physical CAN bus with PCAN-USB + CAN monitor app.

## Task 4: HIL → Cloud (1 hour)

Run CAN gateway on laptop: PCAN-USB → MQTT → Netcup VPS → dashboard.

## Known Issues

| Issue | Severity | Notes |
|-------|----------|-------|
| QNX bridge port changes (TIME_WAIT) | LOW | Use port 9879, or wait 60s after kill |
| Duplicate ECU processes on QNX | LOW | Use `slay -f -s SIGKILL` to clean |
| Laptop eno1 IP resets on reboot | LOW | `sudo ip addr add 192.168.137.100/24 dev eno1` |
| SIL tests use MQTT for fault inject | MEDIUM | MQTT broker running in Docker |
| Docker ECU entrypoint needs LF endings | FIXED | Already committed |

## Key Ports

| Service | Host | Port |
|---------|------|------|
| QNX CAN bridge | 192.168.137.184 | 9879 |
| Laptop CAN bridge | 192.168.0.158 | 9876 |
| MQTT broker | localhost (laptop) | 1883 |
| CAN monitor (QNX) | 192.168.137.184 | 9879 |
| CAN monitor (laptop) | 192.168.0.158 | 9876 |
