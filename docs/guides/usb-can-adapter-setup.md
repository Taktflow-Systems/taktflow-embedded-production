# USB-CAN Adapter + can-utils Setup Guide

> **Prerequisite**: BOM #8 (USB-CAN adapters) must show Delivered in [`hardware/bom.md`](../../hardware/bom.md)
> **Last updated**: 2026-03-01

## Hardware Requirements

> **Delivery status for all items**: see [`hardware/bom.md`](../../hardware/bom.md)
>
> Before starting this guide, verify BOM #8 (USB-CAN adapters) shows **Delivered** in `bom.md`.

## 1. Adapter Details

The BOM originally specified USB-CAN adapter 2.0 (from canable.com / Tindie), but substitutes were ordered instead (see BOM Q-002):

| # | Adapter | Vendor | Interface | Driver | Notes |
|---|---------|--------|-----------|--------|-------|
| 1 | **Ecktron UCAN** | Eckstein GmbH (German) | USB | `gs_usb` or `slcan` (check docs) | Primary adapter |
| 2 | **Waveshare USB-CAN** | Waveshare | USB | `slcan` (typical) | Backup â€” may have stock delay |

**Acceptance criteria (from BOM Q-002):** Must support SocketCAN workflow + stable CAN 500 kbps operation. Verify on arrival during bring-up Phase 3.

### Firmware Mode (depends on adapter)

| Mode | Protocol | Driver | How to tell |
|------|----------|--------|-------------|
| **gs_usb** (native CAN) | USB native | `gs_usb` kernel module | `dmesg` shows "gs_usb" on plug-in â†’ interface shows as `can0` |
| **slcan** (serial) | Serial USB CDC | `slcand` daemon | `dmesg` shows ttyACM0/ttyUSB0 on plug-in â†’ need `slcand` to create `slcan0` |

**Prefer gs_usb** if the adapter supports it â€” native SocketCAN, same as our SIL `vcan0` setup. If only slcan is available, see Section 3b below.

## 2. Firmware Flashing (only if adapter supports candleLight)

If the Ecktron UCAN supports candleLight firmware (STM32-based):

```bash
# 1. Put adapter in DFU mode: hold BOOT button while plugging in USB
# 2. Install dfu-util
sudo apt install dfu-util

# 3. Download candleLight firmware matching your adapter
#    https://github.com/candle-usb/candleLight_fw/releases

# 4. Flash
sudo dfu-util -d 0483:df11 -c 1 -i 0 -a 0 -s 0x08000000 -D candleLight_fw.bin

# 5. Unplug and replug â€” done
```

**Note:** Not all adapters support candleLight. Check adapter docs first. Waveshare USB-CAN typically uses slcan only.

## 3a. Linux Setup â€” gs_usb Mode (native SocketCAN)

If `dmesg` shows `gs_usb` on plug-in:

```bash
# Install can-utils
sudo apt install can-utils

# Plug in adapter via USB
# Verify detection
dmesg | tail -20
# Should show: "gs_usb ... CAN device registered"

# Bring up CAN interface at 500 kbps (matches our bus config)
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0

# Verify
ip -details link show can0
```

### Persistent Setup (survives reboot)

Create `/etc/network/interfaces.d/can0`:

```
auto can0
iface can0 inet manual
    pre-up /sbin/ip link set can0 type can bitrate 500000
    up /sbin/ip link set up can0
    down /sbin/ip link set down can0
```

## 3b. Linux Setup â€” slcan Mode (serial/USB CDC)

If `dmesg` shows `ttyACM0` or `ttyUSB0` on plug-in (typical for Waveshare):

```bash
# Install can-utils (includes slcand)
sudo apt install can-utils

# Create SocketCAN interface from serial device
# -s6 = 500 kbps (speed index: s0=10k, s1=20k, s2=50k, s3=100k, s4=125k, s5=250k, s6=500k, s7=800k, s8=1M)
sudo slcand -o -c -s6 /dev/ttyACM0 slcan0

# Bring up the interface
sudo ip link set up slcan0

# Verify
ip -details link show slcan0

# Now use slcan0 instead of can0 in all candump/cansend commands
candump slcan0
```

### Persistent slcan Setup

Create `/etc/systemd/system/slcan.service`:

```ini
[Unit]
Description=SLCAN interface for USB-CAN adapter
After=dev-ttyACM0.device

[Service]
Type=forking
ExecStart=/usr/bin/slcand -o -c -s6 /dev/ttyACM0 slcan0
ExecStartPost=/sbin/ip link set up slcan0
ExecStop=/sbin/ip link set down slcan0

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable slcan
sudo systemctl start slcan
```

## 4. Basic CAN Operations

```bash
# Monitor all traffic
candump can0

# Monitor with absolute timestamps + error frames
candump -t a -e can0

# Send a test frame (ID=0x100, 4 bytes)
cansend can0 100#DEADBEEF

# Send periodic heartbeat (ID=0x100, 8 bytes, every 100ms)
cangen can0 -I 100 -L 8 -D AABBCCDD11223344 -g 100

# Log to file for offline analysis
candump -l can0
# Creates candump-<timestamp>.log

# Replay a log file
canplayer -I candump-2026-03-10_123456.log

# Bus load statistics
canbusload can0@500000

# Filter: only show specific CAN ID (e.g., 0x100)
candump can0,100:7FF

# Filter: show range of IDs (e.g., 0x700-0x7FF for safety messages)
candump can0,700:700
```

## 5. Our CAN Message ID Ranges

From `docs/aspice/system/can-message-matrix.md` â€” 31 messages total:

| Range | ECU | Example Messages |
|-------|-----|-----------------|
| `0x100`-range | CVC (Central Vehicle Computer) | Heartbeat, commands |
| `0x200`-range | FZC (Front Zone Controller) | Sensor data, status |
| `0x300`-range | RZC (Rear Zone Controller) | Sensor data, status |
| `0x700`-range | SC (Safety Controller) | Safety-critical, watchdog |

### Useful Filter Commands

```bash
# Monitor only CVC traffic
candump can0,100:F00

# Monitor only safety-critical traffic
candump can0,700:F00

# Monitor everything except heartbeats (use inverse match)
candump can0,0:0 -e  # then grep out what you don't need
```

## 6. Physical Wiring

Per `hardware/schematics/03-can-bus.md`:

```
CAN Bus Trunk (twisted pair CAN_H + CAN_L + GND reference wire)

  120 ohm                                            120 ohm
    |                                                   |
   CVC â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ FZC â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ RZC â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ SC
         ~200mm          ~200mm          ~300mm
                                                 (~700mm total)

   USB-CAN adapter #1 â”€â”€â”  (tap via <100mm stub)
                 â”œâ”€â”€ Screw terminal on bus trunk
   USB-CAN adapter #2 â”€â”€â”˜  (tap via <100mm stub)
```

- **USB-CAN adapter #1** â†’ Raspberry Pi 4 USB (gateway / live monitoring)
- **USB-CAN adapter #2** â†’ Dev PC USB (debugging with candump)
- **Do NOT add 120 ohm on USB-CAN adapter** â€” bus already terminated at CVC and SC ends
- All ECU connections use screw terminals (CAN_H, CAN_L, GND)

### Pin Assignments (ECU side)

| ECU | TX Pin | RX Pin | Transceiver |
|-----|--------|--------|-------------|
| CVC (STM32G474) | PA12 (FDCAN1_TX) | PA11 (FDCAN1_RX) | TJA1051T/3 |
| FZC (STM32G474) | PA12 (FDCAN1_TX) | PA11 (FDCAN1_RX) | TJA1051T/3 |
| RZC (STM32G474) | PA12 (FDCAN1_TX) | PA11 (FDCAN1_RX) | TJA1051T/3 |
| SC (TMS570) | DCAN1TX | DCAN1RX | SN65HVD230 |

## 7. Troubleshooting

```bash
# Check error counters (TX errors, RX errors, bus-off state)
ip -details -statistics link show can0

# If bus-off (too many errors) â€” auto-restart after 100ms:
sudo ip link set can0 type can restart-ms 100

# Manual restart:
sudo ip link set down can0 && sudo ip link set up can0

# Common issues:
# - No traffic: CAN_H / CAN_L swapped â†’ swap wires
# - No traffic: wrong bitrate â†’ all nodes must be 500000
# - Measure termination: should be ~60 ohm between CAN_H and CAN_L at any point
# - Error frames flooding: check for loose connections or missing GND reference wire
# - Bus-off loops: one node has wrong bit timing or hardware fault
```

## 8. Windows / WSL Notes

SocketCAN does **not** work in WSL (no kernel CAN modules). Options for Windows dev:

1. **SSH into Raspberry Pi** (recommended) â€” run candump remotely
2. **SavvyCAN** â€” Windows GUI tool, works with USB-CAN adapter in slcan mode
3. **PCAN-View** â€” if you have a PEAK adapter (not our case)

Since our Raspberry Pi is already in the HIL setup, SSH is the path of least resistance:

```bash
ssh pi@<raspberry-pi-ip>
candump can0
```

## 9. Integration with SIL

Our SIL environment (`test/sil/`) uses virtual CAN (`vcan0`). Same tools, same commands â€” just different interface name:

| Environment | Interface | Setup |
|-------------|-----------|-------|
| SIL (Docker) | `vcan0` | `modprobe vcan && ip link add dev vcan0 type vcan && ip link set up vcan0` |
| HIL (real HW) | `can0` | `ip link set can0 type can bitrate 500000 && ip link set up can0` |

Scripts that accept a CAN interface parameter work in both environments.

---

## References

- CAN bus schematic: `hardware/schematics/03-can-bus.md`
- Hardware bringup plan: `docs/plans/plan-hardware-bringup.md`
- SIL test environment: `test/sil/apps-web-overview.md`
- CAN message matrix: `docs/aspice/system/can-message-matrix.md`
- Pin mapping: `hardware/pin-mapping.md`
- BOM: `hardware/bom.md`

