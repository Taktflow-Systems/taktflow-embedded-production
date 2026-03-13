#!/usr/bin/env python3
"""Full CAN network integration test - validates all ECUs on the bus."""

import can
import time
import sys
from collections import defaultdict

CAN_IDS = {
    0x010: ("CVC_Heartbeat", "CVC", 50),
    0x011: ("FZC_Heartbeat", "FZC", 50),
    0x012: ("RZC_Heartbeat", "RZC", 50),
    0x013: ("SC_Heartbeat",  "SC",  500),
    0x100: ("Vehicle_State", "CVC", 10),
    0x101: ("Torque_Request","CVC", 10),
    0x102: ("Steer_Command", "CVC", 10),
    0x103: ("Brake_Command", "CVC", 10),
    0x200: ("Steer_Status",  "FZC", 10),
    0x201: ("Brake_Sts_FZC", "FZC", None),
    0x210: ("Brake_Fault",   "FZC", None),
    0x211: ("Motor_Cutoff",  "FZC", None),
    0x220: ("Lidar_Distance","FZC", None),
    0x300: ("Motor_Status",  "RZC", 10),
    0x301: ("Motor_Current", "RZC", 10),
    0x302: ("Brake_Sts_RZC", "RZC", None),
    0x303: ("Battery_Status","RZC", None),
    0x014: ("ICU_Heartbeat", "ICU", 500),
    0x015: ("TCU_Heartbeat", "TCU", 500),
    0x350: ("Body_Cmd",      "CVC", None),
    0x500: ("DTC_Broadcast", "CVC", None),
}

CRC8_TABLE = []
for _i in range(256):
    _c = _i
    for _ in range(8):
        _c = ((_c << 1) ^ 0x07) & 0xFF if _c & 0x80 else (_c << 1) & 0xFF
    CRC8_TABLE.append(_c)

def crc8(data):
    crc = 0x00
    for b in data:
        crc = CRC8_TABLE[crc ^ b]
    return crc


def run_test():
    print("=" * 70)
    print("  TAKTFLOW NETWORK INTEGRATION TEST")
    print("  CAN interface: can0 @ 500kbps")
    print("=" * 70)
    print()

    bus = can.Bus(channel="can0", interface="socketcan", bitrate=500000)

    print("[1/6] Collecting CAN traffic for 5 seconds...")
    frames = defaultdict(list)
    t0 = time.monotonic()
    deadline = t0 + 5.0
    while time.monotonic() < deadline:
        msg = bus.recv(timeout=0.05)
        if msg is not None:
            frames[msg.arbitration_id].append((time.monotonic() - t0, bytes(msg.data)))
    total = sum(len(v) for v in frames.values())
    print("      Captured %d frames across %d unique CAN IDs" % (total, len(frames)))
    print()

    pass_count = [0]
    fail_count = [0]

    def verdict(name, passed, detail=""):
        status = "PASS" if passed else "FAIL"
        if passed:
            pass_count[0] += 1
        else:
            fail_count[0] += 1
        mark = "+" if passed else "!"
        print("  [%s] %s: %s  %s" % (mark, name, status, detail))

    print("[2/6] Heartbeat presence (physical ECUs)...")
    for hb_id in [0x010, 0x011, 0x012, 0x013]:
        name = CAN_IDS[hb_id][0]
        count = len(frames.get(hb_id, []))
        verdict("%s (0x%03X) present" % (name, hb_id), count > 0, "count=%d" % count)
    print()

    print("[3/6] CVC TX message schedule...")
    for cid in [0x100, 0x101, 0x102, 0x103]:
        name, ecu, period = CAN_IDS[cid]
        count = len(frames.get(cid, []))
        verdict("%s (0x%03X) present" % (name, cid), count > 0, "count=%d" % count)
        if count >= 2 and period:
            ts_list = [f[0] for f in frames[cid]]
            deltas = [(ts_list[i+1] - ts_list[i]) * 1000 for i in range(len(ts_list)-1)]
            avg = sum(deltas) / len(deltas)
            verdict("%s period ~%dms" % (name, period),
                    abs(avg - period) < period * 0.5,
                    "avg=%.1fms" % avg)
    print()

    print("[4/6] FZC/RZC TX messages...")
    for cid in [0x200, 0x300, 0x301]:
        name = CAN_IDS[cid][0]
        count = len(frames.get(cid, []))
        verdict("%s (0x%03X) present" % (name, cid), count > 0, "count=%d" % count)
    print()

    print("[5/6] E2E CRC-8 validation on heartbeats...")
    for hb_id in [0x010, 0x011, 0x012]:
        name = CAN_IDS[hb_id][0]
        hb_frames = frames.get(hb_id, [])
        if not hb_frames:
            verdict("%s E2E CRC" % name, False, "no frames")
            continue
        crc_ok = 0
        crc_fail = 0
        alive_values = []
        for _, data in hb_frames:
            if len(data) < 2:
                crc_fail += 1
                continue
            rx_crc = data[0]
            payload = bytearray(data)
            payload[0] = 0x00
            expected = crc8(bytes(payload))
            if rx_crc == expected:
                crc_ok += 1
            else:
                crc_fail += 1
            alive_values.append(data[1] & 0x0F)
        verdict("%s E2E CRC" % name, crc_fail == 0,
                "ok=%d fail=%d" % (crc_ok, crc_fail))
        if len(alive_values) >= 3:
            mono_ok = 0
            mono_fail = 0
            for i in range(1, len(alive_values)):
                exp_next = (alive_values[i-1] + 1) & 0x0F
                if alive_values[i] == exp_next:
                    mono_ok += 1
                else:
                    mono_fail += 1
            verdict("%s alive counter" % name,
                    mono_fail <= max(1, len(alive_values) * 0.05),
                    "ok=%d fail=%d" % (mono_ok, mono_fail))
    print()

    print("[6/6] CVC Vehicle_State analysis...")
    vs_frames = frames.get(0x100, [])
    if vs_frames:
        last_data = vs_frames[-1][1]
        if len(last_data) >= 3:
            state_byte = last_data[2] & 0x0F
            state_names = {0: "INIT", 1: "INIT", 2: "SELF_TEST", 3: "SELF_TEST",
                          4: "RUN", 5: "DEGRADED", 6: "LIMP", 7: "SAFE_STOP",
                          8: "SHUTDOWN"}
            state_str = state_names.get(state_byte, "UNKNOWN(0x%02X)" % state_byte)
            rx_crc = last_data[0]
            payload = bytearray(last_data)
            payload[0] = 0x00
            exp_crc = crc8(bytes(payload))
            verdict("Vehicle_State E2E CRC", rx_crc == exp_crc,
                    "rx=0x%02X exp=0x%02X" % (rx_crc, exp_crc))
            verdict("CVC state", True, "state=%s (0x%02X)" % (state_str, state_byte))
    else:
        verdict("Vehicle_State present", False, "no frames")
    print()

    print("=" * 70)
    t = pass_count[0] + fail_count[0]
    print("  RESULTS: %d/%d PASS, %d FAIL" % (pass_count[0], t, fail_count[0]))
    print()
    print("  Bus Statistics (5s capture):")
    print("  %7s  %-22s  %6s  %8s" % ("CAN ID", "Name", "Count", "Rate"))
    print("  %s  %s  %s  %s" % ("-"*7, "-"*22, "-"*6, "-"*8))
    for cid in sorted(frames.keys()):
        name = CAN_IDS.get(cid, ("Unknown_0x%03X" % cid, "?", None))[0]
        count = len(frames[cid])
        rate = count / 5.0
        print("  0x%03X    %-22s  %6d  %7.1f/s" % (cid, name, count, rate))
    print()
    print("  Total: %d frames, %.0f frames/sec" % (total, total/5.0))
    print("=" * 70)
    bus.shutdown()
    return fail_count[0] == 0

if __name__ == "__main__":
    ok = run_test()
    sys.exit(0 if ok else 1)
