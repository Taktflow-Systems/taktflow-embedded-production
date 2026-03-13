## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.

# Live Demo Dashboard — Web-Based Fault Injection & Streaming

**Status**: PENDING
**Created**: 2026-02-22
**Replaces**: Phase 11e (tkinter Fault Injection GUI) + Phase 14a (Demo Scenarios)
**Dependencies**: Phase 11a-d (Pi gateway, cloud, ML, SAP QM mock)
**Effort**: ~3 days (integrated into Phase 11e + Phase 14)

---

## Purpose

A web dashboard served from the Raspberry Pi that lets anyone — interviewer, recruiter, or assessor — interact with the live bench remotely. They press a button, watch the hardware react on camera, see the fault propagate through the system, read the root cause, and watch a SAP QM quality notification get created in real-time.

This is the single artifact that turns 35 documents and 11,000 lines of code into an undeniable proof of competence.

---

## What the User Sees

```
┌─────────────────────────────────────────────────────────────────────┐
│  TAKTFLOW ZONAL VEHICLE PLATFORM — Live Demo                   [●] │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌───────────────────────┐  ┌────────────────────────────────────┐ │
│  │                       │  │  SYSTEM STATUS                     │ │
│  │   LIVE VIDEO FEED     │  │                                    │ │
│  │                       │  │  Vehicle State:  [RUN]  ● green    │ │
│  │   (Pi Camera / USB    │  │  CVC Heartbeat:  OK     32ms ago   │ │
│  │    webcam pointed     │  │  FZC Heartbeat:  OK     28ms ago   │ │
│  │    at the bench)      │  │  RZC Heartbeat:  OK     31ms ago   │ │
│  │                       │  │  SC Status:      MONITORING         │ │
│  │   720p, 15fps,        │  │  Kill Relay:     CLOSED ● green    │ │
│  │   ~200ms latency      │  │  E-Stop:         INACTIVE          │ │
│  │                       │  │  CAN Bus:        OK (247 msg/s)    │ │
│  │   [shows LEDs,        │  │  CAN Errors:     0                 │ │
│  │    relay, motor,      │  │                                    │ │
│  │    OLED, servos]      │  │  SENSOR READINGS                   │ │
│  │                       │  │  Pedal A:        42.3%             │ │
│  └───────────────────────┘  │  Pedal B:        42.1%             │ │
│                              │  Torque Cmd:     38.7%             │ │
│                              │  Motor Current:  2.4A              │ │
│  ┌───────────────────────┐  │  Motor Temp:     34°C              │ │
│  │  FAULT INJECTION      │  │  Battery:        24.1V             │ │
│  │                       │  │  Lidar:          1.82m             │ │
│  │  SAFETY FAULTS        │  │  Steering Angle: 0.0°              │ │
│  │  ┌─────────────────┐  │  │  Speed:          0 RPM             │ │
│  │  │ Kill Heartbeat  │  │  └────────────────────────────────────┘ │
│  │  └─────────────────┘  │                                         │
│  │  ┌─────────────────┐  │  ┌────────────────────────────────────┐ │
│  │  │ Pedal Disagree  │  │  │  FAULT LOG                        │ │
│  │  └─────────────────┘  │  │                                    │ │
│  │  ┌─────────────────┐  │  │  14:23:01.342  HEARTBEAT TIMEOUT  │ │
│  │  │ CAN Bus-Off     │  │  │    Source: SC (Safety Controller)  │ │
│  │  └─────────────────┘  │  │    Reason: CVC heartbeat missed   │ │
│  │  ┌─────────────────┐  │  │      3 consecutive cycles (150ms) │ │
│  │  │ E-Stop Activate │  │  │    Action: Kill relay OPENED      │ │
│  │  └─────────────────┘  │  │    Safe state: ALL_STOP           │ │
│  │  ┌─────────────────┐  │  │    DTC: 0xD10002 (HB timeout)     │ │
│  │  │ Overcurrent     │  │  │    ASIL: D — SG-001 violated      │ │
│  │  └─────────────────┘  │  │    Response time: 142ms            │ │
│  │                       │  │                                    │ │
│  │  SENSOR FAULTS        │  │  14:23:01.484  SAP QM TRIGGERED   │ │
│  │  ┌─────────────────┐  │  │    QN: QN-2026-00142              │ │
│  │  │ Lidar Blocked   │  │  │    Type: Q1 (Safety)              │ │
│  │  └─────────────────┘  │  │    Priority: 1 (Immediate)        │ │
│  │  ┌─────────────────┐  │  │    DTC: 0xD10002 → QMNUM         │ │
│  │  │ Steering Lost   │  │  │    ECU: CVC → EQUNR               │ │
│  │  └─────────────────┘  │  │    Status: OPEN → In Process      │ │
│  │  ┌─────────────────┐  │  │                                    │ │
│  │  │ Temp Sensor Fail│  │  │  14:23:02.100  8D REPORT CREATED  │ │
│  │  └─────────────────┘  │  │    D1: Zonal Platform Safety Team │ │
│  │                       │  │    D2: CVC heartbeat loss caused   │ │
│  │  ML / PREDICTIVE      │  │        system kill via SC          │ │
│  │  ┌─────────────────┐  │  │    D3: Kill relay = containment   │ │
│  │  │ Degrade Motor   │  │  │    D4-D8: Pending investigation   │ │
│  │  └─────────────────┘  │  │                                    │ │
│  │  ┌─────────────────┐  │  │  ──────────────────────────────    │ │
│  │  │ CAN Injection   │  │  │  14:22:45.000  SYSTEM STARTED     │ │
│  │  └─────────────────┘  │  │    All ECUs: heartbeat OK          │ │
│  │                       │  │    Vehicle state: INIT → RUN       │ │
│  │  ┌─────────────────┐  │  │    Self-test: PASSED               │ │
│  │  │ ■ RESET SYSTEM  │  │  └────────────────────────────────────┘ │
│  │  └─────────────────┘  │                                         │
│  └───────────────────────┘                                         │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │  SAP QM — Quality Notifications                              │  │
│  │                                                               │  │
│  │  ID            │ Type │ DTC      │ ECU │ Status     │ 8D     │  │
│  │  QN-2026-00142 │ Q1   │ 0xD10002 │ CVC │ In Process │ [View] │  │
│  │  QN-2026-00141 │ Q2   │ P1A40    │ RZC │ Open       │ [View] │  │
│  │  QN-2026-00140 │ Q1   │ 0xC20001 │ RZC │ Completed  │ [View] │  │
│  │  QN-2026-00139 │ Q3   │ 0xD30001 │ FZC │ In Process │ [View] │  │
│  │                                                               │  │
│  │  Total: 4 │ Open: 1 │ In Process: 2 │ Completed: 1          │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  Architecture: SG→FSR→TSR→SSR→Code→Test │ 548 Reqs │ 440   │   │
│  │  Traced │ 7 ECUs │ 31 CAN Messages │ ASIL D │ ASPICE L2    │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Architecture

```
                        INTERNET
                           │
                    ┌──────┴──────┐
                    │  BROWSER    │
                    │  (anywhere) │
                    └──────┬──────┘
                           │ HTTPS + WebSocket (wss://)
                           │
                    ┌──────┴──────────────────────────┐
                    │  RASPBERRY PI 4                   │
                    │                                   │
                    │  ┌─────────────────────────────┐ │
                    │  │  FastAPI Server (port 8080)  │ │
                    │  │  ├─ /           → dashboard  │ │
                    │  │  ├─ /ws/telemetry → live CAN │ │
                    │  │  ├─ /ws/faults  → fault log  │ │
                    │  │  ├─ /ws/video   → MJPEG      │ │
                    │  │  ├─ /api/inject → fault cmd  │ │
                    │  │  ├─ /api/reset  → system rst │ │
                    │  │  └─ /api/qm/*   → SAP QM     │ │
                    │  └────────┬────────────────────┘ │
                    │           │                       │
                    │  ┌────────┴────────┐             │
                    │  │  CAN Listener    │   USB cam  │
                    │  │  (python-can)    │   (v4l2)   │
                    │  └────────┬────────┘             │
                    │           │ CANable 2.0 (gs_usb) │
                    └───────────┼───────────────────────┘
                                │
    ════════════════════════════╪═══════════ CAN Bus 500kbps ═══════
        │           │          │           │
    ┌───┴───┐  ┌────┴────┐ ┌──┴────┐  ┌───┴────┐
    │  CVC  │  │   FZC   │ │  RZC  │  │   SC   │
    │STM32  │  │ STM32   │ │ STM32 │  │ TMS570 │
    └───────┘  └─────────┘ └───────┘  └────────┘
```

### Data Flow — Fault Injection to SAP QM

```
User clicks [Kill Heartbeat] in browser
  │
  ├─→ POST /api/inject {"fault": "heartbeat_suppress", "target": "cvc"}
  │
  ├─→ Pi sends CAN command to CVC: suppress heartbeat TX
  │     (CAN ID 0x7DF, UDS extended session + routine control)
  │     OR: Pi stops forwarding CVC heartbeat frames (man-in-the-middle)
  │     OR: Pi sends jam frames to corrupt CVC heartbeat (realistic)
  │
  ├─→ SC detects 3× missed CVC heartbeats (150ms window)
  │     SC state: MONITORING → FAULT
  │     SC action: open kill relay (GPIO HIGH → MOSFET off → relay de-energizes)
  │     SC action: broadcast SAFETY_FAULT on CAN (0x090)
  │     SC action: set CVC LED to RED
  │
  ├─→ Pi CAN listener captures:
  │     - Missing CVC heartbeat frames (absence detection)
  │     - SAFETY_FAULT broadcast from SC (0x090)
  │     - Vehicle state change CVC → FAULT (0x100)
  │     - E-stop echo from CVC (0x080, if CVC still alive)
  │
  ├─→ Pi WebSocket pushes to browser:
  │     {
  │       "event": "FAULT",
  │       "source": "SC",
  │       "reason": "CVC heartbeat missed 3 consecutive cycles (150ms)",
  │       "action": "Kill relay OPENED",
  │       "safe_state": "ALL_STOP",
  │       "dtc": "0xD10002",
  │       "asil": "D",
  │       "safety_goal": "SG-001",
  │       "response_time_ms": 142,
  │       "timestamp": "2026-02-22T14:23:01.342Z"
  │     }
  │
  ├─→ Browser updates:
  │     - Video: relay LED goes OFF, motor stops (visible)
  │     - System status: Vehicle State → FAULT (red), Kill Relay → OPEN (red)
  │     - Fault log: detailed entry with reason, DTC, ASIL, timing
  │
  ├─→ Pi triggers SAP QM mock API:
  │     POST /api/qm/notification
  │     {
  │       "type": "Q1",
  │       "dtc": "0xD10002",
  │       "ecu": "CVC",
  │       "priority": 1,
  │       "description": "CVC heartbeat loss — SC killed system",
  │       "sensor_context": {"last_hb_age_ms": 158, "sc_state": "FAULT"},
  │       "safety_goal": "SG-001",
  │       "eight_d_auto": true
  │     }
  │
  └─→ Browser SAP QM panel updates:
        - New row: QN-2026-00142, Q1, 0xD10002, CVC, In Process
        - 8D report link active: D1-D3 auto-filled, D4-D8 pending
```

---

## Fault Injection Catalog

Each button maps to a specific CAN-level action the Pi executes.

| # | Button Label | Pi Action | What Happens on Bench | DTC | SAP QM Type |
|---|-------------|-----------|----------------------|-----|-------------|
| 1 | Kill Heartbeat | Stop forwarding CVC heartbeat | SC opens kill relay in 150ms | 0xD10002 | Q1 Safety |
| 2 | Pedal Disagree | Inject conflicting pedal CAN values | CVC enters LIMP, zero torque | 0xC10001 | Q2 Internal |
| 3 | Pedal Stuck | Inject constant pedal value (no change for 500ms) | CVC stuck detect, zero torque latch | 0xC10003 | Q2 Internal |
| 4 | CAN Bus-Off | Flood bus with error frames | All ECUs detect bus-off, SC kills | 0xD30001 | Q3 Supplier |
| 5 | E-Stop | Send E-stop CAN broadcast (0x080) | CVC latches, 4× broadcast, all stop | 0xD10001 | Q1 Safety |
| 6 | Motor Overcurrent | Inject high current ADC value via CAN | RZC cuts motor, DTC stored | 0xC20001 | Q1 Safety |
| 7 | Motor Overtemp | Inject high temp ADC value via CAN | RZC derates then stops | 0xC20002 | Q2 Internal |
| 8 | Lidar Blocked | Inject 0cm lidar reading | FZC emergency brake | 0xC30001 | Q2 Internal |
| 9 | Steering Lost | Stop steering angle CAN messages | FZC return-to-center | 0xC30002 | Q3 Supplier |
| 10 | Battery Low | Inject low voltage ADC value | RZC reports, CVC degrades | 0xC20004 | Q2 Internal |
| 11 | Degrade Motor (ML) | Ramp current noise to trigger ML anomaly | Pi ML score drops, Soft DTC P1A40 | P1A40 | Q2 Predictive |
| 12 | CAN Injection | Send unknown CAN ID (security demo) | Pi anomaly detector flags intrusion | N/A | Q3 Security |
| **R** | **RESET SYSTEM** | **Clear faults, restart heartbeats** | **All ECUs return to RUN** | — | — |

---

## Tech Stack

| Component | Choice | Why |
|-----------|--------|-----|
| **Web server** | FastAPI (Python) | Already on Pi for gateway, async WebSocket native, same language as CAN listener |
| **Frontend** | Vanilla HTML + CSS + JS | No build step, no npm, instant load, works offline. Single `index.html` + `app.js` |
| **Real-time** | WebSocket (native) | Sub-100ms latency for telemetry, no polling overhead |
| **Video stream** | MJPEG over HTTP | Universal browser support, no WebRTC complexity, Pi Camera / USB webcam via `picamera2` or OpenCV `VideoCapture` |
| **CAN interface** | python-can + gs_usb | Already used in gateway, CANable 2.0 with candleLight firmware |
| **SAP QM mock** | Flask (existing from Phase 11d) | Already planned, add WebSocket bridge to push notifications to dashboard |
| **Styling** | Dark theme, monospace | Looks like an engineering tool, not a marketing page. Matches terminal/Grafana aesthetic |

### Why NOT React/Next.js

- Pi serves this locally — no Node.js runtime needed
- No build step = debug by editing files directly on Pi
- WebSocket + vanilla JS is ~200 lines, React adds 50MB of node_modules for zero benefit
- The audience is engineers, not consumers — they respect simplicity

<!-- DECISION: ADR-TBD — Live demo tech stack (FastAPI + vanilla JS + MJPEG) -->

---

## Video Streaming Detail

```python
# Pi serves MJPEG stream at /video_feed
# Browser: <img src="/video_feed" /> — native MJPEG support, no JS needed

# Option A: Pi Camera Module (if available)
from picamera2 import Picamera2
camera = Picamera2()
camera.configure(camera.create_video_configuration(main={"size": (1280, 720)}))

# Option B: USB Webcam (more likely for bench setup)
import cv2
camera = cv2.VideoCapture(0)
camera.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

# Serve as MJPEG: yield JPEG frames with multipart boundary
@app.get("/video_feed")
async def video_feed():
    return StreamingResponse(generate_frames(), media_type="multipart/x-mixed-replace; boundary=frame")
```

Target: 720p at 15fps, ~200ms glass-to-glass latency. Good enough to see relay click, LED change, motor stop.

---

## WebSocket Protocol

### Telemetry Channel (`/ws/telemetry`) — 10 Hz

```json
{
  "ts": 1708617781342,
  "vehicle_state": "RUN",
  "heartbeats": {
    "cvc": {"alive": 142, "age_ms": 32, "ok": true},
    "fzc": {"alive": 89, "age_ms": 28, "ok": true},
    "rzc": {"alive": 91, "age_ms": 31, "ok": true}
  },
  "sc": {"state": "MONITORING", "relay": "CLOSED"},
  "sensors": {
    "pedal_a": 42.3,
    "pedal_b": 42.1,
    "torque_cmd": 38.7,
    "motor_current_a": 2.4,
    "motor_temp_c": 34,
    "battery_v": 24.1,
    "lidar_m": 1.82,
    "steering_deg": 0.0,
    "speed_rpm": 0
  },
  "can": {"msg_per_sec": 247, "error_count": 0, "bus_off": false},
  "ml": {"motor_health": 87, "anomaly_score": 0.12}
}
```

### Fault Channel (`/ws/faults`) — Event-driven

```json
{
  "event": "FAULT",
  "ts": 1708617781342,
  "source": "SC",
  "reason": "CVC heartbeat missed 3 consecutive cycles (150ms)",
  "action": "Kill relay OPENED",
  "safe_state": "ALL_STOP",
  "dtc": "0xD10002",
  "dtc_name": "Heartbeat Timeout — CVC",
  "asil": "D",
  "safety_goal": "SG-001: Prevent unintended vehicle motion",
  "response_time_ms": 142,
  "sensor_snapshot": {
    "last_cvc_hb_age_ms": 158,
    "sc_state": "FAULT",
    "relay_state": "OPEN"
  }
}
```

### SAP QM Channel (piggyback on `/ws/faults`) — Event-driven

```json
{
  "event": "SAP_QM",
  "ts": 1708617781484,
  "notification": {
    "id": "QN-2026-00142",
    "type": "Q1",
    "type_name": "Safety — Customer Complaint",
    "dtc": "0xD10002",
    "ecu": "CVC",
    "priority": 1,
    "status": "In Process",
    "description": "CVC heartbeat loss caused system kill via Safety Controller",
    "eight_d": {
      "d1_team": "Zonal Platform Safety Team",
      "d2_problem": "CVC heartbeat transmission ceased, SC detected 3 missed cycles within 150ms FTTI, kill relay opened per SG-001 safe state definition",
      "d3_containment": "Kill relay de-energized (fail-safe), all actuators stopped, vehicle in ALL_STOP safe state",
      "d4_root_cause": "Pending investigation",
      "d5_corrective": "Pending",
      "d6_implemented": "Pending",
      "d7_preventive": "Pending",
      "d8_closure": "Pending"
    }
  }
}
```

---

## File Structure

```
gateway/
  dashboard/
    index.html          — Single-page dashboard (dark theme)
    app.js              — WebSocket handlers, DOM updates, fault injection API calls
    style.css           — Dark engineering theme, grid layout

  server.py             — FastAPI: WebSocket endpoints, MJPEG stream, fault injection API
  can_decoder.py        — Decode raw CAN frames → structured telemetry dict
  fault_injector.py     — Map fault button → CAN command sequence (upgraded from Phase 11e)
  video_stream.py       — MJPEG capture and streaming (USB webcam or Pi Camera)

  sap_qm_mock/          — (existing from Phase 11d, add WebSocket push)
    app.py              — Flask REST API for SAP QM
    ws_bridge.py        — NEW: push QM events to dashboard WebSocket
    ...
```

---

## Implementation Phases

### Phase A: Skeleton (0.5 day)

- [ ] FastAPI server with static file serving (`gateway/dashboard/`)
- [ ] Basic `index.html` with dark theme grid layout (no live data yet)
- [ ] WebSocket echo test (browser connects, server echoes)
- [ ] MJPEG test stream (static image or webcam if available)

### Phase B: Live Telemetry (0.5 day)

- [ ] CAN listener → WebSocket telemetry bridge (10 Hz)
- [ ] `can_decoder.py` parses all 31 CAN message types
- [ ] Dashboard JS updates DOM from WebSocket telemetry
- [ ] System status panel: vehicle state, heartbeats, relay, sensors
- [ ] CAN bus stats: messages/sec, error count

### Phase C: Fault Injection (1 day)

- [ ] `/api/inject` endpoint with fault catalog (12 fault types)
- [ ] `fault_injector.py` maps each fault to CAN command sequence
- [ ] Fault buttons in dashboard call API, show loading state
- [ ] Fault channel WebSocket pushes fault events with full context
- [ ] Fault log panel: scrolling log with reason, DTC, ASIL, timing
- [ ] Reset button clears faults, restarts normal operation
- [ ] Fault response time measurement (inject timestamp → detection timestamp)

### Phase D: SAP QM Integration (0.5 day)

- [ ] Fault events auto-trigger SAP QM mock API
- [ ] WebSocket pushes QM notification to dashboard
- [ ] SAP QM table panel: notification list with status, 8D link
- [ ] 8D report detail view (modal or expandable row)
- [ ] Predictive: ML Soft DTC (P1A40) creates Q2 early warning

### Phase E: Polish (0.5 day)

- [ ] Video stream integration (MJPEG `<img>` tag)
- [ ] Mobile-responsive layout (interviewer on phone)
- [ ] Connection status indicator (WebSocket connected/reconnecting)
- [ ] Footer: architecture stats (548 reqs, 440 traced, 7 ECUs, ASIL D)
- [ ] Error handling: CAN bus down, camera unavailable, Pi overloaded
- [ ] README section with screenshot and access instructions

---

## Interview Demo Script (3 minutes)

This is how you present it:

> **[0:00–0:30] Introduction**
> "This is a 7-ECU zonal vehicle platform I built. 4 physical ECUs on CAN bus, 3 simulated in Docker, a Raspberry Pi edge gateway, and AWS cloud. You're looking at the live bench right now through this camera."
>
> **[0:30–1:00] Normal Operation**
> "The system is in RUN state. All heartbeats are green, the safety controller is monitoring. You can see live sensor readings — pedal position, motor current, temperature, battery voltage. The CAN bus is running at 247 messages per second."
>
> **[1:00–1:45] Fault Injection — The Money Shot**
> "Now I'm going to kill the CVC heartbeat. Watch the bench."
> *clicks [Kill Heartbeat]*
> "See that? The safety controller detected the missing heartbeat in 142 milliseconds. The kill relay opened — you can see the LED change on camera. The vehicle state went to FAULT. The system is now in its safe state."
>
> "Look at the fault log — it tells you exactly what happened: CVC heartbeat missed 3 cycles, SC opened the kill relay, DTC 0xD10002 stored, ASIL D safety goal SG-001 was the trigger."
>
> **[1:45–2:30] SAP QM — Business Integration**
> "And here's what makes this different from a hobby project. The DTC automatically created a SAP QM quality notification — QN-2026-00142, type Q1 safety, priority 1. And it auto-generated the first three sections of an 8D report: team, problem description, and containment action. In a real OEM, this is how field failures flow into the quality management system."
>
> **[2:30–3:00] Reset & Architecture**
> *clicks [RESET SYSTEM]*
> "System is back to RUN. All heartbeats green. I built the full V-model for this — HARA, 548 requirements, traceability from safety goals down to unit tests, ASPICE Level 2 process. 16 BSW modules, 283 unit tests, and you just watched the safety chain work live."

---

## Why This Wins the Interview

| What they see | What it proves |
|---------------|---------------|
| Hardware reacting on camera | Not paper architecture — real system |
| 142ms fault response time | Safety timing requirements met (FTTI) |
| Fault reason with ASIL + safety goal | Deep ISO 26262 understanding |
| SAP QM notification auto-created | Understands Tier 1 quality systems |
| 8D report auto-generated | Bridges engineering → business process |
| ML predictive fault (Soft DTC) | Modern predictive maintenance capability |
| All from one web page | Full-stack: embedded C → CAN → Python → web → cloud → SAP |
| Reset and repeat | Reproducible, not a one-shot demo |

---

## Access Modes

| Mode | How | Use Case |
|------|-----|----------|
| **Local** | `http://pi-ip:8080` on same network | In-person interview, bench in the room |
| **Remote (ngrok)** | `ngrok http 8080` → public URL | Remote interview, share link in chat |
| **Remote (Tailscale)** | Tailscale mesh VPN | Persistent remote access, no port forwarding |
| **Recorded** | Screen record the demo session | Portfolio video, LinkedIn, GitHub README |

For remote interviews: start ngrok before the call, share the URL. Interviewer interacts with live hardware from their browser.

---

## Non-Goals

- Not a production web app — no auth, no HTTPS (local network only, ngrok handles TLS for remote)
- Not real SAP — mock API demonstrates the data flow and mapping, not RFC/BAPI integration
- Not real-time video (<200ms is fine, not targeting <50ms)
- Not mobile-first — desktop is primary, mobile is nice-to-have
- No database — in-memory state, resets on Pi reboot (this is a demo, not a product)

