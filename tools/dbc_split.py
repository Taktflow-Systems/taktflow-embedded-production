#!/usr/bin/env python3
"""
Split taktflow.dbc into production + SIL + diagnostics.

Reads the monolithic DBC file and produces:
  1. taktflow_vehicle.dbc   — 26 core messages (production CAN matrix)
  2. taktflow_sil.dbc       — virtual sensor messages (SIL-only)
  3. taktflow_diag.dbc      — UDS diagnostic messages
  4. taktflow.dbc           — merged (backward-compat, generated)

Also qualifies signal names: E2E_DataID → EStBc_E2E_DataID (message-prefixed).

Usage:
    python3 tools/dbc_split.py
"""

import cantools
import sys

# Message short-name prefixes (for signal qualification)
MSG_PREFIX = {
    "EStop_Broadcast":      "EStBc",
    "CVC_Heartbeat":        "CvcHb",
    "FZC_Heartbeat":        "FzcHb",
    "RZC_Heartbeat":        "RzcHb",
    "SC_Status":            "ScSt",
    "ICU_Heartbeat":        "IcuHb",
    "TCU_Heartbeat":        "TcuHb",
    "BCM_Heartbeat":        "BcmHb",
    "Vehicle_State":        "VehSt",
    "Torque_Request":       "TrqRq",
    "Steer_Command":        "StrCmd",
    "Brake_Command":        "BrkCmd",
    "Steering_Status":      "StrSt",
    "Brake_Status":         "BrkSt",
    "Brake_Fault":          "BrkFlt",
    "Motor_Cutoff_Req":     "MtrCut",
    "Lidar_Distance":       "LidarD",
    "Motor_Status":         "MtrSt",
    "Motor_Current":        "MtrCur",
    "Motor_Temperature":    "MtrTmp",
    "Battery_Status":       "BatSt",
    "Body_Control_Cmd":     "BdyCmd",
    "Light_Status":         "LtSt",
    "Indicator_State":      "IndSt",
    "Door_Lock_Status":     "DlkSt",
    "DTC_Broadcast":        "DtcBc",
    # SIL
    "FZC_Virtual_Sensors":  "FzcVS",
    "RZC_Virtual_Sensors":  "RzcVS",
    # Diag
    "UDS_Func_Request":     "UdsFnRq",
    "UDS_Phys_Req_CVC":     "UdsRqCvc",
    "UDS_Phys_Req_FZC":     "UdsRqFzc",
    "UDS_Phys_Req_RZC":     "UdsRqRzc",
    "UDS_Phys_Req_TCU":     "UdsRqTcu",
    "UDS_Resp_CVC":         "UdsRsCvc",
    "UDS_Resp_FZC":         "UdsRsFzc",
    "UDS_Resp_RZC":         "UdsRsRzc",
    "UDS_Resp_TCU":         "UdsRsTcu",
}

# Category assignment
SIL_MSG_IDS = {0x600, 0x601}
DIAG_MSG_IDS = {0x7DF, 0x7E0, 0x7E1, 0x7E2, 0x7E3, 0x7E8, 0x7E9, 0x7EA, 0x644}

PROD_NODES = ["CVC", "FZC", "RZC", "SC", "BCM", "ICU", "TCU"]
SIL_NODES = ["Plant_Sim"]
DIAG_NODES = ["Tester"]


def qualify_signal_name(msg_name, sig_name):
    """Qualify a signal name with its message prefix."""
    prefix = MSG_PREFIX.get(msg_name, msg_name[:6])
    return "%s_%s" % (prefix, sig_name)


def main():
    db = cantools.database.load_file("gateway/taktflow.dbc")

    print("=== Current DBC ===")
    print("  Messages: %d" % len(db.messages))
    print("  Signals:  %d" % sum(len(m.signals) for m in db.messages))
    print("  Nodes:    %d" % len(db.nodes))

    # Categorize messages
    prod_msgs = []
    sil_msgs = []
    diag_msgs = []

    for msg in db.messages:
        if msg.frame_id in SIL_MSG_IDS:
            sil_msgs.append(msg)
        elif msg.frame_id in DIAG_MSG_IDS:
            diag_msgs.append(msg)
        else:
            prod_msgs.append(msg)

    print("\n=== Split ===")
    print("  Production: %d messages, %d signals" % (
        len(prod_msgs), sum(len(m.signals) for m in prod_msgs)))
    print("  SIL:        %d messages, %d signals" % (
        len(sil_msgs), sum(len(m.signals) for m in sil_msgs)))
    print("  Diagnostics:%d messages, %d signals" % (
        len(diag_msgs), sum(len(m.signals) for m in diag_msgs)))

    # Check signal name collisions in production
    print("\n=== Signal Name Audit (production only) ===")
    sig_to_msgs = {}
    for msg in prod_msgs:
        for sig in msg.signals:
            sig_to_msgs.setdefault(sig.name, []).append(msg.name)

    colliding = {name: msgs for name, msgs in sig_to_msgs.items() if len(msgs) > 1}
    unique_sigs = {name for name, msgs in sig_to_msgs.items() if len(msgs) == 1}

    if colliding:
        print("  %d signal names appear in multiple messages:" % len(colliding))
        for name in sorted(colliding):
            msgs = colliding[name]
            qualified = [qualify_signal_name(m, name) for m in msgs]
            print("    %-20s  %d msgs → %s" % (name, len(msgs), ", ".join(qualified[:4])))
    else:
        print("  No collisions (all signal names unique)")
    print("  %d signal names already unique (no rename needed)" % len(unique_sigs))

    # Show qualification preview
    print("\n=== Qualification Preview (first 5 messages) ===")
    for msg in prod_msgs[:5]:
        print("  %s (0x%03X):" % (msg.name, msg.frame_id))
        for sig in msg.signals:
            old = sig.name
            new = qualify_signal_name(msg.name, sig.name)
            changed = " ← RENAMED" if old != new else ""
            print("    %-30s → %-40s%s" % (old, new, changed))

    print("\n=== Ready to generate ===")
    print("  Run with --write to generate the 3 DBC files")

    if "--write" in sys.argv:
        print("\n  TODO: DBC write not implemented yet")
        print("  The cantools library can write DBC files.")
        print("  Need to create new Database objects with qualified names.")


if __name__ == "__main__":
    main()
