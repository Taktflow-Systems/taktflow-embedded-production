#!/usr/bin/env python3
"""
@file       sc_eth_hil_scenario.py
@brief      S-UDP-05 CAN scenario driver — heartbeat load + fault injection
            toward the SC over a Waveshare USB-CAN-A adapter on Windows.

Drives the S-UDP-05 bench validation scenario (plan-sc-ethernet-roadmap.md):
generates CVC/FZC/RZC heartbeats (0x010/0x011/0x012 @ 50 ms, valid E2E),
optional rest-bus load (0x600/0x601 @ 10 ms), and scripted fault injection
(per-ECU heartbeat dropout, per-ECU E2E CRC corruption), while logging every
received CAN frame — SC_Status 0x013 is decoded field-by-field — with host
timestamps so SC reaction times can be measured against the injection edge.

The SC relay kill LATCHES until power cycle (SWR-SC-011): run one injection
phase per board power cycle.

Phases:
    nominal        heartbeats (+ optional rest-bus) for --duration seconds
    dropout        nominal until --inject-at, then stop --target-ecu
                   heartbeat, keep the others, hold --hold seconds
    corrupt        nominal until --inject-at, then send --target-ecu
                   heartbeat with corrupted E2E CRC (byte1 ^ 0xFF).
                   Non-HIL builds reject these (kill); PLATFORM_HIL
                   force-accepts them (negative check, no reaction).
    content-fault  nominal until --inject-at, then send --target-ecu
                   heartbeat with two FaultStatus bits set (byte3=0x31).
                   SC_Heartbeat_ValidateContent latches a content fault
                   after SC_HB_FAULT_ESCALATE_MAX (20) receptions — works
                   on HIL builds too (runs after the E2E force-accept).

HIL-build notes (PLATFORM_HIL, the only viable config on the LaunchPad
bench — relay GPIO is muxed to LIN1TX, so non-HIL readback kills ~25 ms
after energize): FZC monitoring is compiled out (target CVC or RZC),
E2E is force-accepted, HB timeout is 500 ms + 10-tick confirm, and
DeEnergize is a no-op so kills surface as mode=FAULT with fault_flags,
never in relay_energized/fault_reason.

Usage:
    # offline self-test, no serial port touched:
    python scripts/hil/sc_eth_hil_scenario.py --dry-run --phase dropout \
        --target-ecu fzc --port NONE

    # 10-minute nominal load:
    python scripts/hil/sc_eth_hil_scenario.py --port COM13 --phase nominal \
        --duration 600 --restbus \
        --tx-log tmp/sudp05_runB1_tx.csv --rx-log tmp/sudp05_runB1_rx.csv

    # FZC heartbeat dropout at T+60 s, hold 30 s:
    python scripts/hil/sc_eth_hil_scenario.py --port COM13 --phase dropout \
        --target-ecu fzc --inject-at 60 --hold 30 \
        --tx-log tmp/sudp05_runB2_tx.csv --rx-log tmp/sudp05_runB2_rx.csv

Frame layouts must match:
    - heartbeat payload: scripts/hil/hb_spoofer.py and
      firmware/bsw/services/E2E/src/E2E.c (E2E_ComputePduCrc)
    - SC_Status 0x013 payload: firmware/ecu/sc/src/sc_monitoring.c
      (mon_build_payload)
    - Waveshare serial framing: scripts/debug/waveshare_can_sniffer.py

Requires: pyserial (already used by scripts/serial-monitor.py). No python-can.

@aspice     SWE.6 — Software Qualification Testing (HIL support tooling)
"""

import argparse
import csv
import datetime as dt
import struct
import sys
import time

SC_STATUS_CAN_ID = 0x013

# CAN IDs from gateway/taktflow_vehicle.dbc — must match hb_spoofer.py
ECU_HEARTBEATS = {
    "cvc": {"id": 0x010, "data_id": 0x02, "ecu_id": 1, "name": "CVC_Heartbeat"},
    "fzc": {"id": 0x011, "data_id": 0x03, "ecu_id": 2, "name": "FZC_Heartbeat"},
    "rzc": {"id": 0x012, "data_id": 0x04, "ecu_id": 3, "name": "RZC_Heartbeat"},
}

HB_PERIOD_TICKS = 5       # 5 x 10 ms = 50 ms (DBC GenMsgCycleTime)
BASE_TICK_S = 0.010

# Rest-bus payloads per scripts/debug/hil_restbus.py nominal defaults
RESTBUS_FRAMES = {
    0x600: struct.pack("<HHHxx", 8191, 0, 0),          # FZC virtual sensors
    0x601: struct.pack("<HHHH", 0, 250, 12600, 0),     # RZC virtual sensors
}

# SC_KILL_REASON_* / SC_STATUS_REASON_* from firmware/ecu/sc/include/Sc_Hw_Cfg.h
KILL_REASON_NAMES = {
    0: "NONE", 1: "HB_TIMEOUT", 2: "PLAUSIBILITY", 3: "SELFTEST", 4: "ESM",
    5: "BUSOFF", 6: "READBACK", 7: "ESTOP", 8: "BUS_SILENCE", 9: "E2E_FAIL",
    10: "CREEP_GUARD",
}

SC_MODE_NAMES = {0: "INIT", 1: "MONITORING", 2: "FAULT", 3: "SAFE_STOP"}

FIXED_FRAME_SIZE = 20


def crc8_sae_j1850(data: bytes) -> int:
    """CRC-8/SAE-J1850: poly=0x1D, init=0xFF, XOR-out=0xFF.

    Must match scripts/hil/hb_spoofer.py, firmware E2E_ComputePduCrc()
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


def build_heartbeat_data(ecu_cfg: dict, alive_counter: int,
                         variant: str = "ok") -> bytes:
    """Build the 4-byte heartbeat payload (layout per hb_spoofer.py).

    byte 0: [E2E_AliveCounter:4 | E2E_DataID:4]
    byte 1: E2E_CRC8 over (byte2, byte3, DataId) — inverted for "corrupt"
    byte 2: ECU_ID
    byte 3: [FaultStatus:4 | OperatingMode:4] — RUN(1) no faults, or
            0x31 (two FaultStatus bits, mode RUN) for "content-fault"
    """
    data_id = ecu_cfg["data_id"] & 0x0F
    byte0 = ((alive_counter & 0x0F) << 4) | data_id
    byte2 = ecu_cfg["ecu_id"]
    byte3 = 0x31 if variant == "content-fault" else 0x01
    byte1 = crc8_sae_j1850(bytes([byte2, byte3, data_id]))
    if variant == "corrupt":
        byte1 ^= 0xFF
    return bytes([byte0, byte1, byte2, byte3])


def adapter_settings_frame(emission: str, baud: int = 0x03,
                           frame_type: int = 0x01, mode: int = 0x00) -> bytes:
    """USB-CAN-A init command (Waveshare serial-conversion protocol).

    AA 55 <proto> <baud> <type> filter[4] mask[4] <mode> 01 00 00 00 00 <sum>
    proto byte selects the adapter->PC emission framing (bench-verified
    2026-07-07: 0x12 switches emission to variable; persisted default was
    fixed): "fixed" -> 0x02, "variable" -> 0x12. baud 0x03 = 500 kbps.
    mode 0x00 = normal (0x01 loopback, 0x02 silent). checksum over [2:19].
    """
    frame = bytearray(20)
    frame[0] = 0xAA
    frame[1] = 0x55
    frame[2] = 0x02 if emission == "fixed" else 0x12
    frame[3] = baud
    frame[4] = frame_type
    frame[13] = mode
    frame[14] = 0x01
    frame[19] = sum(frame[2:19]) & 0xFF
    return bytes(frame)


# ----------------------------------------------------------------------
# Waveshare USB-CAN-A serial framing (mirror of waveshare_can_sniffer.py)
# ----------------------------------------------------------------------

def encode_fixed(can_id: int, data: bytes) -> bytes:
    """Encode a standard data frame in the fixed 20-byte protocol."""
    frame = bytearray(FIXED_FRAME_SIZE)
    frame[0] = 0xAA
    frame[1] = 0x55
    frame[2] = 0x01                       # type: data
    frame[3] = 0x01                       # frame type: standard
    frame[4] = 0x01                       # frame format: data
    struct.pack_into("<I", frame, 5, can_id & 0x7FF)
    frame[9] = len(data)
    frame[10:10 + len(data)] = data
    frame[19] = sum(frame[2:19]) & 0xFF   # checksum
    return bytes(frame)


def encode_variable(can_id: int, data: bytes) -> bytes:
    """Encode a standard data frame in the variable-length protocol."""
    type_byte = 0xC0 | (len(data) & 0x0F)
    return (bytes([0xAA, type_byte])
            + struct.pack("<H", can_id & 0x7FF)
            + data
            + bytes([0x55]))


def parse_fixed_frame(frame_bytes: bytes):
    """Parse a fixed 20-byte frame (port of waveshare_can_sniffer.py)."""
    if len(frame_bytes) != FIXED_FRAME_SIZE:
        return None
    if frame_bytes[0] != 0xAA or frame_bytes[1] != 0x55:
        return None
    frame_type = frame_bytes[3]
    can_id = struct.unpack_from("<I", frame_bytes, 5)[0]
    can_id &= 0x7FF if frame_type == 0x01 else 0x1FFFFFFF
    dlc = min(frame_bytes[9], 8)
    checksum_ok = (sum(frame_bytes[2:19]) & 0xFF) == frame_bytes[19]
    return {"can_id": can_id, "dlc": dlc,
            "data": bytes(frame_bytes[10:10 + dlc]),
            "checksum_ok": checksum_ok}


def parse_variable_frame(buf):
    """Parse a variable frame at buf[0] (port of waveshare_can_sniffer.py).

    Returns (frame_dict, consumed) — consumed 0 means need more bytes,
    consumed 1 with frame None means resync by one byte.
    """
    if len(buf) < 2 or buf[0] != 0xAA:
        return None, 1
    type_byte = buf[1]
    if type_byte < 0xC0:
        return None, 1
    is_ext = bool(type_byte & 0x20)
    dlc = type_byte & 0x0F
    if dlc > 8:
        return None, 1
    id_len = 4 if is_ext else 2
    frame_len = 2 + id_len + dlc + 1
    if len(buf) < frame_len:
        return None, 0
    if is_ext:
        can_id = struct.unpack_from("<I", buf, 2)[0] & 0x1FFFFFFF
    else:
        can_id = struct.unpack_from("<H", buf, 2)[0] & 0x7FF
    data_start = 2 + id_len
    if buf[data_start + dlc] != 0x55:
        return None, 1
    return {"can_id": can_id, "dlc": dlc,
            "data": bytes(buf[data_start:data_start + dlc]),
            "checksum_ok": True}, frame_len


def decode_sc_status(data: bytes) -> dict:
    """Decode the 4-byte SC_Status payload (sc_monitoring.c mon_build_payload).

    Byte 0: [AliveCounter:4 | DataId:4]
    Byte 1: CRC-8 SAE-J1850 over (byte2, byte3, DataId)
    Byte 2: SC_Mode [3:0] | SC_FaultFlags [7:4]
    Byte 3: ECU_Health [2:0] | FaultReason [6:3] | RelayState [7]
    """
    if len(data) < 4:
        return {}
    alive = (data[0] >> 4) & 0x0F
    data_id = data[0] & 0x0F
    crc_ok = crc8_sae_j1850(bytes([data[2], data[3], data_id])) == data[1]
    return {
        "alive": alive,
        "crc_ok": int(crc_ok),
        "sc_mode": data[2] & 0x0F,
        "fault_flags": (data[2] >> 4) & 0x0F,
        "ecu_health": data[3] & 0x07,
        "fault_reason": (data[3] >> 3) & 0x0F,
        "relay_energized": (data[3] >> 7) & 0x01,
    }


# ----------------------------------------------------------------------
# Logging
# ----------------------------------------------------------------------

TX_COLUMNS = ["host_time_iso", "host_time_s", "row_type", "event", "can_id",
              "data_hex", "detail"]
RX_COLUMNS = ["host_time_iso", "host_time_s", "can_id", "dlc", "data_hex",
              "alive", "crc_ok", "sc_mode", "fault_flags", "ecu_health",
              "fault_reason", "relay_energized"]


def now_row_prefix():
    epoch = time.time()
    iso = dt.datetime.fromtimestamp(epoch, tz=dt.timezone.utc).isoformat(
        timespec="milliseconds")
    return iso, f"{epoch:.6f}"


class CsvLog:
    def __init__(self, path, columns):
        self.path = path
        self.columns = columns
        self.file = None
        self.writer = None
        if path:
            self.file = open(path, "w", newline="", encoding="utf-8")
            self.writer = csv.writer(self.file)
            self.writer.writerow(columns)

    def row(self, values):
        if self.writer:
            self.writer.writerow(values)
            self.file.flush()

    def close(self):
        if self.file:
            self.file.close()


# ----------------------------------------------------------------------
# Scenario engine
# ----------------------------------------------------------------------

class Scenario:
    def __init__(self, args):
        self.args = args
        self.encode = encode_fixed if args.proto == "fixed" else encode_variable
        self.tx_log = CsvLog(args.tx_log, TX_COLUMNS)
        self.rx_log = CsvLog(args.rx_log, RX_COLUMNS)
        self.alive = {cfg["id"]: 0 for cfg in ECU_HEARTBEATS.values()}
        self.tx_counts = {}
        self.rx_counts = {}
        self.rx_buf = bytearray()
        self.events = []
        self.last_status = None

        if args.phase == "nominal":
            self.total_s = args.duration
        else:
            self.total_s = args.inject_at + args.hold

    def log_event(self, event, detail=""):
        iso, epoch = now_row_prefix()
        self.tx_log.row([iso, epoch, "event", event, "", "", detail])
        self.events.append((iso, event, detail))
        print(f"[{iso}] EVENT {event} {detail}")

    def log_tx(self, can_id, data):
        self.tx_counts[can_id] = self.tx_counts.get(can_id, 0) + 1
        if self.args.log_frames:
            iso, epoch = now_row_prefix()
            self.tx_log.row([iso, epoch, "tx", "", f"0x{can_id:03X}",
                             data.hex().upper(), ""])

    def send(self, ser, can_id, data):
        ser.write(self.encode(can_id, data))
        self.log_tx(can_id, data)

    def heartbeat_ecus(self, t_s):
        """Which ECUs get a heartbeat at scenario time t_s, and the payload
        variant for the target ECU during injection."""
        injecting = (self.args.phase != "nominal"
                     and t_s >= self.args.inject_at)
        for key, cfg in ECU_HEARTBEATS.items():
            if injecting and key == self.args.target_ecu:
                if self.args.phase == "dropout":
                    continue                    # omit target heartbeat
                yield cfg, self.args.phase      # corrupt / content-fault
            else:
                yield cfg, "ok"

    def pump_rx(self, ser):
        chunk = ser.read(max(1, ser.in_waiting))
        if chunk:
            self.rx_buf.extend(chunk)
        while True:
            frame = None
            if self.args.proto == "fixed":
                idx = self.rx_buf.find(b"\xAA\x55")
                if idx < 0:
                    if len(self.rx_buf) > 1:
                        del self.rx_buf[:-1]
                    return
                if idx > 0:
                    del self.rx_buf[:idx]
                if len(self.rx_buf) < FIXED_FRAME_SIZE:
                    return
                frame = parse_fixed_frame(bytes(self.rx_buf[:FIXED_FRAME_SIZE]))
                del self.rx_buf[:FIXED_FRAME_SIZE]
            else:
                if len(self.rx_buf) < 4:
                    return
                if self.rx_buf[0] != 0xAA:
                    del self.rx_buf[0]
                    continue
                frame, consumed = parse_variable_frame(self.rx_buf)
                if consumed == 0:
                    return
                del self.rx_buf[:consumed]
            if frame:
                self.record_rx(frame)

    def record_rx(self, frame):
        can_id = frame["can_id"]
        self.rx_counts[can_id] = self.rx_counts.get(can_id, 0) + 1
        decoded = {}
        if can_id == SC_STATUS_CAN_ID:
            decoded = decode_sc_status(frame["data"])
            state_key = (decoded.get("sc_mode"), decoded.get("fault_flags"),
                         decoded.get("ecu_health"), decoded.get("fault_reason"),
                         decoded.get("relay_energized"))
            if state_key != self.last_status:
                self.last_status = state_key
                self.log_event(
                    "sc_status_change",
                    f"mode={SC_MODE_NAMES.get(decoded.get('sc_mode'), '?')}"
                    f" fault_flags=0x{decoded.get('fault_flags', 0):X}"
                    f" health=0x{decoded.get('ecu_health', 0):X}"
                    f" reason={KILL_REASON_NAMES.get(decoded.get('fault_reason'), '?')}"
                    f" relay={decoded.get('relay_energized')}")
        iso, epoch = now_row_prefix()
        self.rx_log.row([iso, epoch, f"0x{can_id:03X}", frame["dlc"],
                         frame["data"].hex().upper(),
                         decoded.get("alive", ""), decoded.get("crc_ok", ""),
                         decoded.get("sc_mode", ""),
                         decoded.get("fault_flags", ""),
                         decoded.get("ecu_health", ""),
                         decoded.get("fault_reason", ""),
                         decoded.get("relay_energized", "")])

    def run(self, ser):
        args = self.args
        self.log_event("scenario_start",
                       f"phase={args.phase} target={args.target_ecu}"
                       f" inject_at={args.inject_at} hold={args.hold}"
                       f" restbus={int(args.restbus)} proto={args.proto}")
        injected = False
        t0 = time.perf_counter()
        tick = 0
        try:
            while True:
                t_s = time.perf_counter() - t0
                if t_s >= self.total_s:
                    break

                if (args.phase != "nominal" and not injected
                        and t_s >= args.inject_at):
                    injected = True
                    self.log_event(f"inject_{args.phase}_start",
                                   f"ecu={args.target_ecu} t={t_s:.3f}")

                if tick % HB_PERIOD_TICKS == 0:
                    for cfg, variant in self.heartbeat_ecus(t_s):
                        data = build_heartbeat_data(
                            cfg, self.alive[cfg["id"]], variant)
                        self.send(ser, cfg["id"], data)
                        self.alive[cfg["id"]] = (self.alive[cfg["id"]] + 1) & 0x0F

                if args.restbus:
                    for can_id, data in RESTBUS_FRAMES.items():
                        self.send(ser, can_id, data)

                self.pump_rx(ser)

                tick += 1
                next_tick = t0 + tick * BASE_TICK_S
                # Resync after host stalls instead of bursting missed ticks:
                # back-to-back same-ID frames overwrite the SC's DCAN mailbox
                # between its 10 ms polls -> E2E alive-counter gap (observed
                # on the bench 2026-07-07). Skipping ticks keeps real spacing.
                behind = time.perf_counter() - next_tick
                if behind > BASE_TICK_S:
                    skipped = int(behind / BASE_TICK_S)
                    tick += skipped
                    next_tick = t0 + tick * BASE_TICK_S
                    self.log_event("tick_resync",
                                   f"skipped={skipped} t={t_s:.3f}")
                remaining = next_tick - time.perf_counter()
                if remaining > 0.002:
                    time.sleep(remaining - 0.002)
                while time.perf_counter() < next_tick:
                    pass
        except KeyboardInterrupt:
            self.log_event("aborted_by_user", f"t={time.perf_counter()-t0:.3f}")
        self.log_event("scenario_end", f"t={time.perf_counter()-t0:.3f}")

    def summary(self):
        print("\n--- TX counts ---")
        for can_id in sorted(self.tx_counts):
            print(f"  0x{can_id:03X}: {self.tx_counts[can_id]}")
        print("--- RX counts ---")
        for can_id in sorted(self.rx_counts):
            print(f"  0x{can_id:03X}: {self.rx_counts[can_id]}")
        print("--- events ---")
        for iso, event, detail in self.events:
            print(f"  [{iso}] {event} {detail}")

    def close(self):
        self.tx_log.close()
        self.rx_log.close()


def self_test() -> int:
    """Offline round-trip checks — no serial port needed."""
    failures = 0
    hb = build_heartbeat_data(ECU_HEARTBEATS["fzc"], 5)
    if crc8_sae_j1850(bytes([hb[2], hb[3], hb[0] & 0x0F])) != hb[1]:
        print("FAIL: heartbeat CRC self-check")
        failures += 1
    corrupted = build_heartbeat_data(ECU_HEARTBEATS["fzc"], 5, "corrupt")
    if corrupted[1] == hb[1]:
        print("FAIL: corrupt variant did not change CRC byte")
        failures += 1
    faulty = build_heartbeat_data(ECU_HEARTBEATS["cvc"], 5, "content-fault")
    if not (faulty[3] == 0x31 and faulty[1] == crc8_sae_j1850(
            bytes([faulty[2], 0x31, faulty[0] & 0x0F]))):
        print("FAIL: content-fault variant byte3/CRC")
        failures += 1
    settings = adapter_settings_frame("fixed")
    if not (settings[2] == 0x02 and settings[19] == sum(settings[2:19]) & 0xFF):
        print("FAIL: adapter settings frame")
        failures += 1

    fixed = encode_fixed(0x011, hb)
    parsed = parse_fixed_frame(fixed)
    if (parsed is None or parsed["can_id"] != 0x011 or parsed["data"] != hb
            or not parsed["checksum_ok"]):
        print("FAIL: fixed-frame round trip")
        failures += 1

    var = encode_variable(0x011, hb)
    parsed_var, consumed = parse_variable_frame(bytearray(var))
    if (parsed_var is None or consumed != len(var)
            or parsed_var["can_id"] != 0x011 or parsed_var["data"] != hb):
        print("FAIL: variable-frame round trip")
        failures += 1

    # SC_Status decode against a hand-packed MONITORING/all-healthy frame
    byte2 = 0x01          # mode=MONITORING, fault_flags=0
    byte3 = 0x07 | 0x80   # health=all, reason=NONE, relay=1
    data_id = 0x05
    crc = crc8_sae_j1850(bytes([byte2, byte3, data_id]))
    status = bytes([(0x0A << 4) | data_id, crc, byte2, byte3])
    dec = decode_sc_status(status)
    if not (dec["alive"] == 0x0A and dec["crc_ok"] == 1
            and dec["sc_mode"] == 1 and dec["fault_flags"] == 0
            and dec["ecu_health"] == 0x07 and dec["fault_reason"] == 0
            and dec["relay_energized"] == 1):
        print(f"FAIL: SC_Status decode: {dec}")
        failures += 1

    print("self-test:", "PASS" if failures == 0 else f"{failures} FAILURE(S)")
    print(f"  sample FZC heartbeat:      {hb.hex().upper()}")
    print(f"  fixed-proto TX frame:      {fixed.hex().upper()}")
    print(f"  variable-proto TX frame:   {var.hex().upper()}")
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(
        description="S-UDP-05 CAN scenario driver (Waveshare USB-CAN-A)")
    parser.add_argument("--port", required=True,
                        help="Waveshare adapter COM port (identify first — "
                             "see tools/bench/hardware-map.local.toml); "
                             "use NONE with --dry-run")
    parser.add_argument("--proto", choices=["fixed", "variable"],
                        default="fixed",
                        help="Waveshare serial protocol mode (default: fixed)")
    parser.add_argument("--baud", type=int, default=2000000,
                        help="adapter serial baud (default: 2000000)")
    parser.add_argument("--phase",
                        choices=["nominal", "dropout", "corrupt",
                                 "content-fault"],
                        required=True)
    parser.add_argument("--init-adapter", choices=["fixed", "variable", "none"],
                        default="fixed",
                        help="send a USB-CAN-A settings frame on connect "
                             "(500k, normal mode, chosen emission framing); "
                             "default fixed to match --proto default")
    parser.add_argument("--duration", type=float, default=600.0,
                        help="nominal phase length in seconds (default: 600)")
    parser.add_argument("--inject-at", type=float, default=60.0,
                        help="injection time for dropout/corrupt (default: 60)")
    parser.add_argument("--hold", type=float, default=30.0,
                        help="seconds to hold the fault (default: 30)")
    parser.add_argument("--target-ecu", choices=list(ECU_HEARTBEATS.keys()),
                        default="fzc",
                        help="ECU whose heartbeat is dropped/corrupted")
    parser.add_argument("--restbus", action="store_true",
                        help="also send 0x600/0x601 rest-bus load @ 10 ms")
    parser.add_argument("--tx-log", default=None,
                        help="CSV path for TX events (+frames with --log-frames)")
    parser.add_argument("--rx-log", default=None,
                        help="CSV path for received frames (0x013 decoded)")
    parser.add_argument("--log-frames", action="store_true",
                        help="log every TX frame, not only events")
    parser.add_argument("--dry-run", action="store_true",
                        help="offline self-test + plan print, no serial I/O")
    args = parser.parse_args()

    if args.dry_run:
        rc = self_test()
        total = (args.duration if args.phase == "nominal"
                 else args.inject_at + args.hold)
        print(f"plan: phase={args.phase} target={args.target_ecu} "
              f"total={total:.0f}s restbus={args.restbus} proto={args.proto}")
        sys.exit(rc)

    try:
        import serial
    except ImportError:
        print("ERROR: pyserial not installed. Run: pip install pyserial",
              file=sys.stderr)
        sys.exit(1)

    scenario = Scenario(args)
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0)
    except serial.SerialException as exc:
        print(f"ERROR: cannot open {args.port}: {exc}", file=sys.stderr)
        sys.exit(1)

    if args.init_adapter != "none":
        ser.write(adapter_settings_frame(args.init_adapter))
        time.sleep(0.3)
        scenario.log_event("adapter_init",
                           f"emission={args.init_adapter} 500k normal")

    try:
        scenario.run(ser)
    finally:
        ser.close()
        scenario.summary()
        scenario.close()


if __name__ == "__main__":
    main()
