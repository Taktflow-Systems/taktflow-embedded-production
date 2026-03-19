# Plan: GIL (Godot-in-the-Loop) on Raspberry Pi

**Status**: IN PROGRESS
**Date**: 2026-03-15
**Goal**: Replicate the full VPS SIL stack onto the Raspberry Pi, replacing `plant-sim` with `godot-bridge` so Godot (on Windows PC) provides pedal and steering input over LAN UDP.

## Naming Convention

Two distinct simulation systems — never mix terminology:

| | **SIL** (Software-in-the-Loop) | **GIL** (Godot-in-the-Loop) |
|---|---|---|
| **Host** | VPS (Netcup, `sil.taktflow-systems.com`) | Raspberry Pi (`192.168.0.195`) |
| **Compose file** | `docker-compose.dev.yml` | `docker-compose.gil.yml` |
| **Physics source** | `plant-sim` (autonomous Python) | `godot-bridge` (CAN↔UDP relay to Godot) |
| **CAN interface** | `vcan0` (virtual) | `can0` (physical, Waveshare HAT) |
| **User input** | None (autonomous) | Godot keyboard/controller (pedal + steering) |
| **Network** | Loopback (host mode) | LAN UDP to Windows PC |
| **cloud-connector** | No | Yes (AWS IoT Core) |
| **can-setup** | Creates vcan0 | Not needed (physical can0) |
| **Domain** | `sil.taktflow-systems.com` | `192.168.0.195` (LAN only) |
| **Old compose (retired)** | — | `docker-compose.pi.yml` (3 vECUs + plant-sim) |

## Context

- VPS runs full SIL: 7 vECUs + plant-sim + gateway services on `vcan0`
- Pi previously ran: 3 vECUs (BCM/ICU/TCU) + plant-sim on `can0` (old `docker-compose.pi.yml`)
- `taktflow-vehicle-sim/bridge/bridge.py` implements CAN↔UDP relay for Godot
- Pi and Windows PC on same LAN (~1ms latency): Pi=`192.168.0.195`, PC=`192.168.0.158`
- Godot IS the plant-sim — it provides full vehicle physics (motor, battery, steering, brake, lidar) via VehicleBody3D, relayed through the bridge

## Architecture

```
Windows PC (192.168.0.158)          Raspberry Pi (192.168.0.195)
┌─────────────────────┐             ┌──────────────────────────────┐
│  Godot 4.6          │   UDP       │  godot-bridge (bridge.py)    │
│  - 3D rendering     │◄──5002──►  │  - CAN↔UDP relay             │
│  - VehicleBody3D    │   5001      │  - can0 (SocketCAN)          │
│  - pedal/steering   │             │                              │
│    user input       │             │  7 vECUs (Docker)            │
│                     │             │  CVC, FZC, RZC, SC,          │
│  fault_panel.gd ────┼──HTTP────►  │  BCM, ICU, TCU               │
│                     │   8091      │                              │
│                     │             │  Gateway services:           │
│                     │             │  mqtt, can-gw, ws-bridge,    │
│                     │             │  caddy, sap-qm-mock,         │
│                     │             │  ml-inference, fault-inject,  │
│                     │             │  cloud-connector              │
└─────────────────────┘             └──────────────────────────────┘
```

## Data Flow

1. Godot physics tick (60 Hz) → sensor JSON over UDP:5001 → godot-bridge
2. godot-bridge writes sensor CAN frames (0x600, 0x601) → can0
3. vECU firmware reads sensors, runs control logic, writes actuator CAN frames (0x101, 0x102, 0x103)
4. godot-bridge reads actuator CAN frames → actuator JSON over UDP:5002 → Godot
5. Godot applies actuator values to VehicleBody3D (engine_force, steering, brake)

Pedal and steering are user input in Godot (keyboard/controller) — CVC reads them via SPI pedal UDP override (port 9100) and steering virtual sensor (0x600).

## Phases

### Phase 1: Prepare godot-bridge container — DONE

1. Created `gateway/godot_bridge/Dockerfile` — Python 3.11-slim, installs `python-can`, `cantools`
2. Copied `bridge.py` from `taktflow-vehicle-sim/bridge/`, adapted for GIL:
   - Reads `CAN_INTERFACE` env var (default `can0`) for single-car mode
   - `GODOT_HOST` env var (default `192.168.0.158`)
   - SPI pedal port starts at 9100 (not 9101)
   - Docker restart points to `docker-compose.gil.yml`
   - All log prefixes: `[godot-bridge]`
3. Created `gateway/godot_bridge/requirements.txt`

### Phase 2: Create docker-compose.gil.yml — DONE

New file `docker/docker-compose.gil.yml` — full stack:
- 7 vECUs (CVC, FZC, RZC, SC, BCM, ICU, TCU) on `can0`
- `godot-bridge` replaces `plant-sim`
- All gateway services: mqtt, can-gw, ws-bridge, caddy, sap-qm-mock, ml-inference, fault-inject
- `cloud-connector` for AWS IoT Core
- NvM volumes for CVC, FZC, RZC
- No `can-setup` service (physical can0)
- `GODOT_HOST` configurable via env var (default `192.168.0.158`)

### Phase 3: Update Godot config — DONE

1. `udp_client.gd`: `BRIDGE_HOST = "192.168.0.195"` — already correct
2. `fault_panel.gd`: Fixed port mismatch `8092` → `8091` (matches fault-inject service)
3. UDP ports: TX=5001, RX=5002 — match bridge config

### Phase 4: Deploy + test — PENDING

1. Deploy to Pi: `scp` or `rsync` the repo
2. Build: `docker compose -f docker-compose.gil.yml build`
3. Start: `docker compose -f docker-compose.gil.yml up -d`
4. Verify: `docker ps` (expect 16 containers)
5. Verify CAN: `candump can0`
6. Test on PC: Godot → press V → pedal/steer → check vECU response
7. Test fault injection: Godot fault panel → fault-inject API → vECU state transition

## Risks

- **Pi 4 RAM (4GB)**: 7 vECU containers + gateway services. Each vECU ~10-15MB, gateway ~20-50MB each. Estimate ~500MB total. Should fit.
- **can0 bus load**: 7 vECUs + bridge all on 500k CAN bus. Same traffic as VPS SIL — should be fine.
- **Bridge single-car mode**: `CAN_INTERFACE` env var overrides vcan{N} construction when `--cars 1`.
