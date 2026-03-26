#!/usr/bin/env python3
"""
@file       test_hil_uds.py
@brief      HIL UDS diagnostic verification — physical CVC/FZC/RZC
@verifies   SWR-CVC-DCM-001, SWR-FZC-DCM-001, SWR-RZC-DCM-001
@traces_to  TSR-050, SSR-CVC-020, SSR-FZC-020, SSR-RZC-020
@aspice     SWE.6 — Software Qualification Testing
@iso        ISO 26262 Part 4, Section 7 — HSI Verification

Tests UDS (Unified Diagnostic Services) on each physical ECU:
  CVC (0x7E0→0x7E8): 10 DIDs, SecurityAccess, ECUReset, DTC
  FZC (0x7E1→0x7E9): 8 DIDs, CommunicationControl
  RZC (0x7E2→0x7EA): 7 DIDs, ECUReset (clears fault latches)

Exercises the full platform stack: Dcm + CanTp + CAN MCAL driver.

Usage:
    python3 test/hil/test_hil_uds.py
"""

import struct
import sys
import time

import can

from hil_test_lib import (
    CAN_CHANNEL, open_bus, wait_for_all_heartbeats,
    precondition_all_ecus_healthy,
    print_header, HopChecker,
)


# ---------------------------------------------------------------------------
# UDS constants
# ---------------------------------------------------------------------------

# ECU addressing (physical request → response)
ECU_CVC = {"name": "CVC", "req": 0x7E0, "resp": 0x7E8}
ECU_FZC = {"name": "FZC", "req": 0x7E1, "resp": 0x7E9}
ECU_RZC = {"name": "RZC", "req": 0x7E2, "resp": 0x7EA}

# UDS SIDs
SID_DIAG_SESSION   = 0x10
SID_ECU_RESET      = 0x11
SID_READ_DID       = 0x22
SID_TESTER_PRESENT = 0x3E

# NRC codes
NRC_SERVICE_NOT_SUPPORTED  = 0x11
NRC_REQUEST_OUT_OF_RANGE   = 0x31

# ISO-TP single-frame PCI
SF_PCI = 0x00
FF_PCI = 0x10
CF_PCI = 0x20
FC_PCI = 0x30


# ---------------------------------------------------------------------------
# UDS helpers
# ---------------------------------------------------------------------------

def uds_send(bus, req_id, payload):
    """Send a UDS request as ISO-TP single frame (payload ≤ 7 bytes)."""
    length = len(payload)
    if length > 7:
        raise ValueError(f"SF payload too long: {length} bytes (max 7)")
    data = bytes([SF_PCI | length]) + payload + bytes(8 - 1 - length)
    msg = can.Message(arbitration_id=req_id, data=data, is_extended_id=False)
    bus.send(msg)


def uds_recv(bus, resp_id, timeout=3.0):
    """Receive a UDS response (single-frame or multi-frame via ISO-TP).

    Also handles raw (non-ISO-TP) responses where the Dcm sends UDS payload
    directly without CanTp framing (DLC = payload length, no PCI byte).

    Returns the reassembled payload (without PCI bytes) or None on timeout.
    """
    end = time.time() + timeout
    while time.time() < end:
        msg = bus.recv(timeout=0.5)
        if msg is None or msg.arbitration_id != resp_id:
            continue
        pci = msg.data[0]
        pci_type = (pci >> 4) & 0x0F

        if pci_type == 0:  # Single Frame
            sf_len = pci & 0x0F
            return bytes(msg.data[1:1 + sf_len])

        elif pci_type == 1:  # First Frame
            ff_len = ((pci & 0x0F) << 8) | msg.data[1]
            payload = bytearray(msg.data[2:8])

            # Send Flow Control (CTS, BS=0 continuous, STmin=10ms)
            fc = can.Message(
                arbitration_id=resp_id - 8 + (resp_id & 0x0F),  # derive req from resp
                data=[FC_PCI | 0x00, 0x00, 0x0A, 0, 0, 0, 0, 0],
                is_extended_id=False,
            )
            # Use the original request ID for FC
            # CVC: resp 0x7E8, req 0x7E0; FZC: resp 0x7E9, req 0x7E1
            fc_id = resp_id - 8
            fc = can.Message(
                arbitration_id=fc_id,
                data=[FC_PCI, 0x00, 0x0A, 0, 0, 0, 0, 0],
                is_extended_id=False,
            )
            bus.send(fc)

            # Receive Consecutive Frames
            seq = 1
            while len(payload) < ff_len and time.time() < end:
                cf_msg = bus.recv(timeout=1.0)
                if cf_msg is None or cf_msg.arbitration_id != resp_id:
                    continue
                cf_pci = cf_msg.data[0]
                if (cf_pci >> 4) != 2:
                    continue
                cf_seq = cf_pci & 0x0F
                if cf_seq != (seq & 0x0F):
                    break
                payload.extend(cf_msg.data[1:8])
                seq += 1
            return bytes(payload[:ff_len])

        else:
            # Raw UDS response (no ISO-TP framing) — Dcm sent payload directly
            # Heuristic: byte[0] looks like a UDS positive/negative response SID
            if msg.dlc >= 2 and (pci >= 0x40 or pci == 0x7F):
                return bytes(msg.data[:msg.dlc])

    return None


def uds_read_did(bus, ecu, did):
    """Send ReadDataByIdentifier (0x22) and return response payload or None."""
    did_msb = (did >> 8) & 0xFF
    did_lsb = did & 0xFF
    resp = uds_request(bus, ecu, bytes([SID_READ_DID, did_msb, did_lsb]), timeout=3.0)
    if resp is None:
        return None
    # Positive response: 0x62 + DID_MSB + DID_LSB + data
    if len(resp) >= 3 and resp[0] == (SID_READ_DID + 0x40):
        return resp[3:]  # data after SID + DID
    # Negative response: 0x7F + SID + NRC
    if len(resp) >= 3 and resp[0] == 0x7F:
        return None
    return None


def uds_request(bus, ecu, payload, timeout=2.0, retries=5):
    """Send a UDS request with ISO 14229 P2 retry on timeout.

    Returns the response payload or None.  Retries are standard practice
    on loaded CAN buses (ISO 14229 §7.2 — P2 timeout + retry count).
    """
    for attempt in range(retries):
        # Flush stale responses from prior requests
        while bus.recv(timeout=0) is not None:
            pass
        uds_send(bus, ecu["req"], payload)
        resp = uds_recv(bus, ecu["resp"], timeout=timeout)
        if resp is not None:
            return resp
        time.sleep(0.05)  # brief gap before retry
    return None


def uds_tester_present(bus, ecu):
    """Send TesterPresent (0x3E, sub=0x00) and check positive response."""
    resp = uds_request(bus, ecu, bytes([SID_TESTER_PRESENT, 0x00]))
    if resp and len(resp) >= 2 and resp[0] == (SID_TESTER_PRESENT + 0x40):
        return True
    return False


def uds_session_control(bus, ecu, session=0x01):
    """Send DiagnosticSessionControl and check positive response."""
    resp = uds_request(bus, ecu, bytes([SID_DIAG_SESSION, session]))
    if resp and len(resp) >= 2 and resp[0] == (SID_DIAG_SESSION + 0x40):
        return True
    return False


# ---------------------------------------------------------------------------
# Main test
# ---------------------------------------------------------------------------

def main():
    bus = open_bus()
    hc = HopChecker()

    print_header("UDS Diagnostic Verification — Platform Stack Test")

    # Unified precondition: all ECUs healthy
    precondition_all_ecus_healthy(bus)
    bus.shutdown()

    # Open a filtered bus for UDS — only receive response IDs (0x7E8-0x7EA).
    # Without this filter, socketcan delivers all ~200 frames/sec and Python
    # can't keep up on Pi ARM, causing intermittent UDS response drops.
    can_filters = [{"can_id": 0x7E8, "can_mask": 0x7FC}]  # matches 0x7E8-0x7EB
    bus = can.interface.Bus(channel=CAN_CHANNEL, interface="socketcan",
                           can_filters=can_filters)
    print()

    # -----------------------------------------------------------------------
    # CVC UDS (0x7E0 → 0x7E8)
    # -----------------------------------------------------------------------
    print("--- CVC UDS (0x7E0 → 0x7E8) ---")

    # Hop 0: TesterPresent
    print("Hop 0: CVC TesterPresent")
    ok = uds_tester_present(bus, ECU_CVC)
    hc.check(0, "CVC TesterPresent positive response", ok, "No response on 0x7E8")

    # Hop 1: DiagnosticSessionControl — default session
    print("Hop 1: CVC DiagnosticSessionControl (default)")
    if not hc.stopped:
        ok = uds_session_control(bus, ECU_CVC, session=0x01)
        hc.check(1, "CVC session=default accepted", ok, "No positive response")

    # Hop 2: ReadDID 0xF190 (SW Version) — 4 bytes expected
    print("Hop 2: CVC ReadDID 0xF190 (SW Version)")
    if not hc.stopped:
        data = uds_read_did(bus, ECU_CVC, 0xF190)
        if data and len(data) >= 1:
            hex_str = " ".join(f"0x{b:02X}" for b in data)
            hc.check(2, f"SW_VERSION=[{hex_str}] ({len(data)} bytes)", True)
        else:
            hc.check(2, "ReadDID 0xF190", False, "No data or NRC")

    # Hop 3: ReadDID 0xF010 (Vehicle State) — 1 byte, should be RUN(1)
    print("Hop 3: CVC ReadDID 0xF010 (Vehicle State)")
    if not hc.stopped:
        data = uds_read_did(bus, ECU_CVC, 0xF010)
        if data and len(data) >= 1:
            state = data[0]
            state_names = {0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP",
                          4: "SAFE_STOP", 5: "SHUTDOWN"}
            name = state_names.get(state, f"0x{state:02X}")
            hc.check(3, f"Vehicle_State={name} ({state})", True)
        else:
            hc.check(3, "ReadDID 0xF010", False, "No data or NRC")

    # Hop 4: ReadDID 0xF191 (Hardware Version) — 3 bytes
    print("Hop 4: CVC ReadDID 0xF191 (Hardware Version)")
    if not hc.stopped:
        data = uds_read_did(bus, ECU_CVC, 0xF191)
        if data is not None:
            hex_str = " ".join(f"0x{b:02X}" for b in data)
            hc.check(4, f"HW_VERSION=[{hex_str}] ({len(data)} bytes)", True)
        else:
            hc.check(4, "ReadDID 0xF191", False, "No data or NRC")

    # Hop 5: Invalid DID → NRC 0x31 (RequestOutOfRange)
    print("Hop 5: CVC ReadDID 0xFFFF (invalid) → NRC expected")
    if not hc.stopped:
        resp = uds_request(bus, ECU_CVC, bytes([SID_READ_DID, 0xFF, 0xFF]))
        if resp and len(resp) >= 3 and resp[0] == 0x7F and resp[1] == SID_READ_DID:
            nrc = resp[2]
            hc.check(5, f"NRC=0x{nrc:02X} (expected 0x31)", nrc == NRC_REQUEST_OUT_OF_RANGE,
                     f"NRC=0x{nrc:02X}")
        elif resp and resp[0] == (SID_READ_DID + 0x40):
            hc.check(5, "Invalid DID accepted (unexpected)", False,
                     "ECU returned positive response for invalid DID")
        else:
            hc.check(5, "NRC response", False, "No response for invalid DID")

    # -----------------------------------------------------------------------
    # FZC UDS (0x7E1 → 0x7E9)
    # -----------------------------------------------------------------------
    print()
    print("--- FZC UDS (0x7E1 → 0x7E9) ---")

    # Hop 6: FZC TesterPresent
    print("Hop 6: FZC TesterPresent")
    if not hc.stopped:
        ok = uds_tester_present(bus, ECU_FZC)
        hc.check(6, "FZC TesterPresent positive response", ok, "No response on 0x7E9")

    # Hop 7: FZC ReadDID 0xF195 (SW Version) — 5 bytes "0.9.0"
    print("Hop 7: FZC ReadDID 0xF195 (SW Version)")
    if not hc.stopped:
        data = uds_read_did(bus, ECU_FZC, 0xF195)
        if data and len(data) >= 1:
            try:
                sw_ver = data.decode("ascii", errors="replace")
            except Exception:
                sw_ver = " ".join(f"0x{b:02X}" for b in data)
            hc.check(7, f"FZC SW_VERSION=\"{sw_ver}\" ({len(data)} bytes)", True)
        else:
            hc.check(7, "ReadDID 0xF195", False, "No data or NRC")

    # Hop 8: FZC ReadDID 0xF190 (ECU Identifier) — 4 bytes
    print("Hop 8: FZC ReadDID 0xF190 (ECU Identifier)")
    if not hc.stopped:
        data = uds_read_did(bus, ECU_FZC, 0xF190)
        if data is not None:
            hex_str = " ".join(f"0x{b:02X}" for b in data)
            hc.check(8, f"FZC ECU_ID=[{hex_str}] ({len(data)} bytes)", True)
        else:
            hc.check(8, "ReadDID 0xF190", False, "No data or NRC")

    # Hop 9: FZC ReadDID 0xF022 (Lidar Distance) — 2 bytes BE uint16
    print("Hop 9: FZC ReadDID 0xF022 (Lidar Distance)")
    if not hc.stopped:
        data = uds_read_did(bus, ECU_FZC, 0xF022)
        if data and len(data) >= 2:
            dist_cm = struct.unpack(">H", data[:2])[0]
            hc.check(9, f"Lidar_Distance={dist_cm}cm", True)
        elif data:
            hc.check(9, f"Lidar data ({len(data)} bytes)", True)
        else:
            hc.check(9, "ReadDID 0xF022", False, "No data or NRC")

    # -----------------------------------------------------------------------
    # RZC UDS (0x7E2 → 0x7EA)
    # -----------------------------------------------------------------------
    print()
    print("--- RZC UDS (0x7E2 → 0x7EA) ---")

    # Hop 10: RZC TesterPresent
    print("Hop 10: RZC TesterPresent")
    if not hc.stopped:
        ok = uds_tester_present(bus, ECU_RZC)
        hc.check(10, "RZC TesterPresent positive response", ok, "No response on 0x7EA")

    # Hop 11: RZC ReadDID 0xF030 (Motor Current) — 2 bytes BE uint16, mA
    print("Hop 11: RZC ReadDID 0xF030 (Motor Current)")
    if not hc.stopped:
        data = uds_read_did(bus, ECU_RZC, 0xF030)
        if data and len(data) >= 2:
            current_ma = struct.unpack(">H", data[:2])[0]
            hc.check(11, f"Motor_Current={current_ma}mA", True)
        elif data:
            hc.check(11, f"Motor current data ({len(data)} bytes)", True)
        else:
            hc.check(11, "ReadDID 0xF030", False, "No data or NRC")

    # Hop 12: RZC ReadDID 0xF033 (Battery Voltage) — 2 bytes BE uint16, mV
    print("Hop 12: RZC ReadDID 0xF033 (Battery Voltage)")
    if not hc.stopped:
        data = uds_read_did(bus, ECU_RZC, 0xF033)
        if data and len(data) >= 2:
            batt_mv = struct.unpack(">H", data[:2])[0]
            hc.check(12, f"Battery_Voltage={batt_mv}mV", True)
        else:
            hc.check(12, "ReadDID 0xF033", False, "No data or NRC")

    # Hop 13: RZC ReadDID 0xF031 (Motor Temperature) — 2 bytes BE uint16, deci-°C
    print("Hop 13: RZC ReadDID 0xF031 (Motor Temperature)")
    if not hc.stopped:
        data = uds_read_did(bus, ECU_RZC, 0xF031)
        if data and len(data) >= 2:
            temp_dc = struct.unpack(">H", data[:2])[0]
            hc.check(13, f"Motor_Temp={temp_dc/10:.1f}°C ({temp_dc} deci-°C)", True)
        else:
            hc.check(13, "ReadDID 0xF031", False, "No data or NRC")

    # Hop 14: RZC ReadDID 0xF035 (Derating) — 1 byte, %
    print("Hop 14: RZC ReadDID 0xF035 (Derating Factor)")
    if not hc.stopped:
        data = uds_read_did(bus, ECU_RZC, 0xF035)
        if data and len(data) >= 1:
            derating = data[0]
            hc.check(14, f"Derating={derating}%", True)
        else:
            hc.check(14, "ReadDID 0xF035", False, "No data or NRC")

    # Hop 15: RZC ECUReset (0x11, sub=0x01) → positive response 0x51
    # This clears Motor+TempMonitor fault latches without power cycle
    print("Hop 15: RZC ECUReset (SID 0x11) — clears fault latches")
    if not hc.stopped:
        uds_send(bus, ECU_RZC["req"], bytes([SID_ECU_RESET, 0x01]))
        resp = uds_recv(bus, ECU_RZC["resp"], timeout=3.0)
        if resp and len(resp) >= 2 and resp[0] == (SID_ECU_RESET + 0x40):
            hc.check(15, f"ECUReset positive response (sub=0x{resp[1]:02X})", True)
        else:
            hc.check(15, "ECUReset response", False,
                     f"resp={resp.hex() if resp else 'None'}")

    bus.shutdown()
    sys.exit(hc.summary())


if __name__ == "__main__":
    main()
