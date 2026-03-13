# Plan: Taktflow CAN Bus Monitor (CANoe-Style Desktop App)

**Status**: DONE
**Created**: 2026-03-05
**Completed**: 2026-03-05

## Context

We have 3 physical STM32 ECUs (CVC, FZC, RZC) on a 500 kbps CAN bus, captured via a Waveshare USB-CAN-A adapter (COM13, 2 Mbps serial). The current `can_monitor_gui.pyw` shows raw hex data in a basic tkinter table. The user wants a professional CANoe-style tool with signal decoding, strip charts, trace logging, and TX capability.

**Key resources already available:**
- `gateway/taktflow.dbc` — complete DBC with 25+ messages, signal definitions, units, enums, ASIL ratings
- `scripts/waveshare_can_sniffer.py` — working Waveshare protocol parser
- `gateway/can_gateway/decoder.py` — cantools-based DBC decoder
- `cantools` + `pyserial` already installed

## UI Framework: PyQt5 + pyqtgraph

| Option | Verdict |
|--------|---------|
| tkinter (current) | No strip charts, ttk styling too limited, lags at 1000 fps |
| customtkinter | Cosmetic-only upgrade, still no charts |
| PyQt5 + pyqtgraph | Industry standard for instrumentation GUIs, 60 fps strip charts, QSS dark theming, tabbed splitter layouts |

Install: `pip install PyQt5 pyqtgraph`

## File Structure

```
tools/can-monitor/
├── __init__.py
├── main.py                    # entry point, argparse, QApplication
├── core/
│   ├── __init__.py
│   ├── waveshare.py           # copied parsing functions from sniffer
│   ├── decoder.py             # adapted from gateway — raw bytes decode
│   ├── bus_reader.py          # ReaderThread — serial → frame queue
│   ├── frame_store.py         # FrameStore — ring buffer + message table
│   ├── recorder.py            # CSV export + replay
│   └── tx_engine.py           # Waveshare TX frame builder
├── ui/
│   ├── __init__.py
│   ├── app_window.py          # QMainWindow + toolbar + tabs
│   ├── theme.py               # purple palette QSS, ASIL colors
│   ├── trace_view.py          # scrolling frame log with filters
│   ├── message_monitor.py     # one row per CAN ID, decoded signals
│   ├── signal_graph.py        # pyqtgraph strip charts
│   ├── bus_stats.py           # bus load, frame rates, errors
│   ├── tx_panel.py            # manual TX + UDS templates
│   └── ecu_health.py          # CVC/FZC/RZC status bar
└── requirements.txt           # PyQt5, pyqtgraph
```

## Phase Status

| Phase | Name | Status |
|-------|------|--------|
| 1 | Core Infrastructure | DONE |
| 2 | PyQt5 Shell + Message Monitor | DONE |
| 3 | Trace View | DONE |
| 4 | Signal Graph | DONE |
| 5 | Stats + TX Panel | DONE |
| 6 | Recording + Polish | DONE |

## Implementation Phases

### Phase 1: Core Infrastructure
- [ ] Create directory structure
- [ ] Copy Waveshare parser functions into `core/waveshare.py`
- [ ] Write `core/decoder.py` — adapt gateway decoder, `decode_raw(arb_id, data)`, remove python-can dep
- [ ] Write `core/frame_store.py` — ParsedFrame, MessageState, FrameStore with lock
- [ ] Write `core/bus_reader.py` — ReaderThread with serial + protocol detect
- **DONE**: Decoded CAN signals print correctly from live bus

### Phase 2: PyQt5 Shell + Message Monitor
- [ ] `pip install PyQt5 pyqtgraph`
- [ ] Write `ui/theme.py` — QSS stylesheet
- [ ] Write `ui/app_window.py` — QMainWindow, toolbar, tabs, status bar
- [ ] Write `ui/ecu_health.py` — ECU status widgets
- [ ] Write `ui/message_monitor.py` — QTableWidget, signal decode display
- [ ] Wire DispatchThread signal → refresh slot
- [ ] Update `CAN Monitor.bat` launcher
- **DONE**: Double-click opens app, shows decoded signals updating live

### Phase 3: Trace View
- [ ] Write `ui/trace_view.py` — scrolling QTableWidget, auto-scroll, pause
- [ ] Filter controls: CAN ID text, ECU dropdown
- [ ] Decoded column: top 2-3 signals (skip E2E by default)
- [ ] Export CSV button
- **DONE**: Filter to CVC-only, export 60s trace to CSV

### Phase 4: Signal Graph
- [ ] Write `ui/signal_graph.py` — pyqtgraph PlotWidget with signal selector
- [ ] Signal tree from DBC (message → signals with units)
- [ ] Strip chart with rolling time window (5/10/30/60s)
- [ ] Freeze/unfreeze
- **DONE**: Select MotorSpeed_RPM, see smooth 60fps chart

### Phase 5: Stats + TX Panel
- [ ] Write `ui/bus_stats.py` — load %, per-ID rates, error counters
- [ ] Write `ui/tx_panel.py` — raw frame builder + UDS templates
- [ ] Write `core/tx_engine.py` — build Waveshare TX frame
- [ ] Test: send UDS DefaultSession to CVC, see response
- **DONE**: TX works, bus load displayed correctly

### Phase 6: Recording + Polish
- [ ] Write `core/recorder.py` — CSV recording, replay mode
- [ ] Window geometry save/restore (QSettings)
- **DONE**: Record session, replay it, same waveforms

## Theme (Taktflow purple palette)

```
BG_DEEP=#1e1b2e  BG_PANEL=#2d2640  ACCENT=#c4b5fd  SELECTED=#4c1d95
CVC=#1a1840  FZC=#2a1530  RZC=#0f2518
ASIL: D=#dc2626 C=#f97316 B=#facc15 A=#84cc16 QM=#6b7280
Font: Consolas (data), Segoe UI (labels)
```

## Key Design Decisions

1. **PyQt5 over tkinter** — pyqtgraph strip charts at 60fps, QSS theming, QTableWidget handles 1000fps updates
2. **Separate ReaderThread + DispatchThread** — serial read must be tight (2Mbps), decoder adds ~50us per frame
3. **Copy waveshare.py, don't import** — sniffer has sys.exit() and print(), not a library
4. **decode_choices=True** — show enum strings ("SAFE_STOP" not 3) for human readability
5. **ECU from DBC transmitter field** — `msg_def.senders[0]` instead of CAN ID range heuristic
6. **E2E signals hidden by default** — structural, not operational; toggle checkbox to show
