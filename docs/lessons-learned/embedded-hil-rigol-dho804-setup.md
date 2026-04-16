# Lessons Learned — Rigol DHO804 Oscilloscope Setup

## 2026-03-24 — DHO804 LAN connection: Ethernet doesn't work, USB does

**Context**: Trying to connect Rigol DHO804 to PC for automated SCPI queries (VCC monitoring, CAN signal capture) during SC transceiver debugging.

**Mistake**: Spent 30+ minutes trying every LAN configuration (DHCP, static IP on 192.0.2.x, 198.51.100.x, 169.254.x.x) through the Tenda switch and direct cable. Scope showed "Connected" but PC could never ping it. Windows Firewall rules added, ICS subnet tried — nothing worked.

**Fix**: Connected via **USB** instead. Rigol Ultra Sigma (already installed) provides the USB-TMC driver. NI-VISA backend in pyvisa finds it immediately:
```
USB0::0x1AB1::0x044D::DHO8A273609235::INSTR
```
pyvisa-py backend does NOT find USB-TMC — must use NI-VISA (default `ResourceManager()`, not `ResourceManager('@py')`).

**Working setup**:
```python
import pyvisa
rm = pyvisa.ResourceManager()      # NI-VISA, NOT '@py'
scope = rm.open_resource('USB0::0x1AB1::0x044D::DHO8A273609235::INSTR')
scope.timeout = 5000
print(scope.query('*IDN?'))
# RIGOL TECHNOLOGIES,DHO804,DHO8A273609235,00.01.03
```

**Network topology** (for reference):
```
PC (192.0.2.20 WiFi) ── Router ── Range Extender
PC (203.0.113.1 Ethernet) ── Tenda SG105 ── Laptop, Pi
PC (USB) ── Rigol DHO804  ← this works
```
The Ethernet adapter changes IP between 198.51.100.1 (ICS mode) and 203.0.113.1 depending on Windows sharing state. Unreliable for scope connection.

**Principle**: When LAN doesn't work on a bench instrument, try USB first. USB-TMC is simpler (no IP config, no firewall, no subnet matching). LAN is only needed for remote/headless access. For a scope sitting next to the PC, USB is the right choice.

## 2026-03-24 — Probe attenuation mismatch: 10X probe reads 1/10th voltage

**Context**: Probing SC transceiver VCC (5V). Scope showed 500mV.

**Mistake**: Probe physical switch was set to **10X** but scope channel was set to **1X**. The 10X probe divides the signal by 10 at the tip, and the scope is supposed to multiply it back by 10 — but only if the probe ratio is set correctly on the channel.

**Fix**: Match probe setting on scope: CH3 → Probe → **10X**. Or switch the physical probe to 1X. After matching: scope correctly showed 5V.

**Principle**: Always check probe attenuation ratio matches between the physical probe switch and the scope channel setting. Rigol default is often 10X probe — check `*IDN?` or `:CHANnel3:PROBe?` via SCPI.

## 2026-03-24 — SC transceiver is TJA1051T/3, not SN65HVD230

**Context**: Memory and docs said SC uses SN65HVD230 (3.3V). Probed VCC expecting 3.3V, saw 5V.

**Mistake**: The actual module on the SC is **CJMCU-1051 (TJA1051T/3)**, purchased from Amazon (Youmile, €7.69/2pcs). TJA1051T/3 needs 5V VCC with 3.3V-5V tolerant I/O. VCC=5V is correct. Docs were wrong.

**Fix**: Updated understanding. TJA1051T/3 with 5V VCC and 3.3V TMS570 DCAN TX is a valid configuration — the `/3` variant accepts 3.3V logic levels.

**Principle**: Verify actual hardware against documentation. BOM says one thing, bench may have another. Always check the IC marking on the module before debugging.

## 2026-03-24 — Rigol DHO804 SCPI port and query script

**DHO804 specs**:
- SCPI port: **5555** (TCP, for LAN — not used, USB preferred)
- USB: USB-TMC class, VID=0x1AB1, PID=0x044D
- Firmware: 00.01.03
- Query script: `taktflow-systems-hil-bench/scripts/rigol-query.py` (LAN) or use pyvisa USB directly

**Existing script location**: `H:\tmp\taktflow-systems-hil-bench\scripts\rigol-query.py`
- Uses raw TCP socket to port 5555 — only works over LAN
- For USB: use pyvisa with NI-VISA backend instead
