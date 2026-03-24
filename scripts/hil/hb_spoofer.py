#!/usr/bin/env python3
"""
@file       hb_spoofer.py
@brief      Spoof ECU heartbeat on CAN bus while that ECU is being flashed.

Keeps the SC relay energized during HIL reflash by sending fake heartbeat
frames at 50ms on behalf of the ECU being flashed.  With PLATFORM_HIL the
SC bypasses E2E validation, so any frame with the correct CAN ID and DLC
resets the heartbeat timeout counter.

Usage:
    # Spoof CVC heartbeat on laptop's can0 while CVC is being flashed:
    python3 hb_spoofer.py cvc

    # Spoof CVC + FZC at the same time (e.g. shared build/stm32 reflash):
    python3 hb_spoofer.py cvc fzc

    # Use a different CAN interface:
    python3 hb_spoofer.py --channel vcan0 cvc

    # Run for a fixed duration then exit:
    python3 hb_spoofer.py --duration 30 cvc

Press Ctrl+C to stop when the ECU is back on the bus.

Requires: python-can (pip install python-can)
Run on the laptop (192.168.0.158) with gs_usb can0 up.

@aspice     SWE.6 — Software Qualification Testing (HIL support tooling)
"""

import argparse
import signal
import sys
import time

try:
    import can
except ImportError:
    print("ERROR: python-can not installed. Run: pip install python-can", file=sys.stderr)
    sys.exit(1)

# CAN IDs from gateway/taktflow.dbc (decimal in DBC, hex in firmware)
ECU_HEARTBEATS = {
    "cvc": {"id": 0x010, "data_id": 0x02, "ecu_id": 1, "name": "CVC_Heartbeat"},
    "fzc": {"id": 0x011, "data_id": 0x03, "ecu_id": 2, "name": "FZC_Heartbeat"},
    "rzc": {"id": 0x012, "data_id": 0x04, "ecu_id": 3, "name": "RZC_Heartbeat"},
}

# Heartbeat period from DBC: GenMsgCycleTime = 50ms
HB_PERIOD_S = 0.050

# SC heartbeat timeout: SC_HB_TIMEOUT_TICKS=10 @ 10ms = 100ms
# So 50ms period gives 2x margin.


def crc8_sae_j1850(data: bytes) -> int:
    """CRC-8/SAE-J1850: poly=0x1D, init=0xFF, XOR-out=0xFF.

    Must match firmware/bsw/services/E2E/src/E2E.c E2E_ComputePduCrc()
    and firmware/ecu/sc/src/sc_e2e.c sc_crc8().
    """
    crc = 0xFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x1D) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc ^ 0xFF


def build_heartbeat_frame(ecu_cfg, alive_counter):
    """Build a 4-byte heartbeat frame matching the DBC layout.

    DBC layout (little-endian bitwise):
        byte 0: [E2E_AliveCounter:4 | E2E_DataID:4]
        byte 1: E2E_CRC8 (SAE-J1850 over payload + DataId)
        byte 2: ECU_ID
        byte 3: [FaultStatus:4 | OperatingMode:4]  (RUN=1, no faults=0)

    CRC is computed over bytes 2..N-1 (payload) then DataId,
    matching E2E_ComputePduCrc in the BSW.
    """
    data_id = ecu_cfg["data_id"] & 0x0F
    alive = (alive_counter & 0x0F) << 4
    byte0 = alive | data_id
    byte2 = ecu_cfg["ecu_id"]  # ECU_ID
    byte3 = 0x01  # OperatingMode=RUN(1), FaultStatus=0

    # CRC over payload bytes (byte2, byte3) + DataId
    byte1 = crc8_sae_j1850(bytes([byte2, byte3, data_id]))

    return can.Message(
        arbitration_id=ecu_cfg["id"],
        data=bytes([byte0, byte1, byte2, byte3]),
        is_extended_id=False,
    )


def main():
    parser = argparse.ArgumentParser(
        description="Spoof ECU heartbeat to keep SC relay energized during flash"
    )
    parser.add_argument(
        "ecus",
        nargs="+",
        choices=list(ECU_HEARTBEATS.keys()),
        help="ECU(s) to spoof: cvc, fzc, rzc",
    )
    parser.add_argument(
        "--channel",
        default="can0",
        help="CAN interface (default: can0)",
    )
    parser.add_argument(
        "--interface",
        default="socketcan",
        help="python-can interface type (default: socketcan)",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=0,
        help="Run for N seconds then exit (0 = run until Ctrl+C)",
    )
    args = parser.parse_args()

    # Graceful shutdown on Ctrl+C
    stop = False

    def on_signal(sig, frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    ecus = [ECU_HEARTBEATS[e] for e in args.ecus]
    ecu_names = ", ".join(e["name"] for e in ecus)

    try:
        bus = can.Bus(channel=args.channel, interface=args.interface)
    except Exception as e:
        print(f"ERROR: Cannot open {args.channel}: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Spoofing: {ecu_names}")
    print(f"Channel:  {args.channel} ({args.interface})")
    print(f"Period:   {HB_PERIOD_S * 1000:.0f}ms")
    if args.duration > 0:
        print(f"Duration: {args.duration}s")
    print("Press Ctrl+C to stop.\n")

    alive_counters = {e["id"]: 0 for e in ecus}
    start_time = time.monotonic()
    tx_count = 0

    try:
        while not stop:
            loop_start = time.monotonic()

            if args.duration > 0 and (loop_start - start_time) >= args.duration:
                print(f"\nDuration reached ({args.duration}s). Stopping.")
                break

            for ecu in ecus:
                msg = build_heartbeat_frame(ecu, alive_counters[ecu["id"]])
                try:
                    bus.send(msg)
                except can.CanError as e:
                    print(f"TX error on {ecu['name']}: {e}", file=sys.stderr)
                alive_counters[ecu["id"]] = (alive_counters[ecu["id"]] + 1) & 0x0F
                tx_count += 1

            # Status every 5s
            elapsed = loop_start - start_time
            if tx_count % (len(ecus) * 100) == 0 and tx_count > 0:
                print(f"  [{elapsed:6.1f}s] {tx_count} frames sent")

            # Sleep remainder of period
            sleep_time = HB_PERIOD_S - (time.monotonic() - loop_start)
            if sleep_time > 0:
                time.sleep(sleep_time)

    finally:
        bus.shutdown()
        elapsed = time.monotonic() - start_time
        print(f"\nStopped. {tx_count} frames sent in {elapsed:.1f}s.")


if __name__ == "__main__":
    main()
