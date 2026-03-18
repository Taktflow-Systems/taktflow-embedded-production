#!/usr/bin/env python3
"""
Build exact mapping: code signal names → DBC signal names.

Three categories of code signals:

1. CAN signals (via sidecar rte_aliases):
   Code: CVC_SIG_VEHICLE_STATE
   Alias: Vehicle_State_VehicleState  (old Message_Signal format)
   → Extract message name + signal part
   → Look up message prefix from MSG_PREFIX
   → New DBC name: VehSt_Mode

2. Already-qualified signals (BCM/ICU/TCU use generated names):
   Code: BCM_SIG_VEHICLE_STATE_VEHICLE_STATE
   → Already contains message + signal
   → Map via old→new signal name table

3. Internal signals (not on CAN):
   Code: FZC_SIG_BUZZER_PATTERN, RZC_SIG_ENCODER_SPEED
   → No DBC mapping — stays as internal RTE signal
   → Mark as INTERNAL

Output: code_to_dbc.json with every code signal mapped or marked.
"""

import cantools
import yaml
import json
import os
import re

# Message prefix map (from dbc_split.py)
MSG_PREFIX = {
    "EStop_Broadcast": "EStBc",
    "CVC_Heartbeat": "CvcHb",
    "FZC_Heartbeat": "FzcHb",
    "RZC_Heartbeat": "RzcHb",
    "SC_Status": "ScSt",
    "ICU_Heartbeat": "IcuHb",
    "TCU_Heartbeat": "TcuHb",
    "BCM_Heartbeat": "BcmHb",
    "Vehicle_State": "VehSt",
    "Torque_Request": "TrqRq",
    "Steer_Command": "StrCmd",
    "Brake_Command": "BrkCmd",
    "Steering_Status": "StrSt",
    "Brake_Status": "BrkSt",
    "Brake_Fault": "BrkFlt",
    "Motor_Cutoff_Req": "MtrCut",
    "Lidar_Distance": "LidarD",
    "Motor_Status": "MtrSt",
    "Motor_Current": "MtrCur",
    "Motor_Temperature": "MtrTmp",
    "Battery_Status": "BatSt",
    "Body_Control_Cmd": "BdyCmd",
    "Light_Status": "LtSt",
    "Indicator_State": "IndSt",
    "Door_Lock_Status": "DlkSt",
    "DTC_Broadcast": "DtcBc",
}

# Old signal name → new signal name (for the 10 renamed signals)
RENAMED = {
    "VehicleState": "Mode",
    "TorqueRequest": "Torque_pct",
    "BatteryStatus": "Status",
    "RelayState": "RelayEnergized",
    "CurrentDirection": "DirIsReverse",
    "TailLightCmd": "TailLightOn",
    "HazardCmd": "HazardActive",
    "LeftIndicator": "LeftOn",
    "RightIndicator": "RightOn",
    "BlinkState": "BlinkPhaseHigh",
}

def old_to_new_signal(msg_name, old_signal_name):
    """Convert old unqualified signal name to new DBC qualified name."""
    prefix = MSG_PREFIX.get(msg_name)
    if not prefix:
        return None
    new_sig = RENAMED.get(old_signal_name, old_signal_name)
    return "%s_%s" % (prefix, new_sig)


def main():
    # Load new DBC for validation
    db = cantools.database.load_file("gateway/taktflow_vehicle.dbc")
    dbc_names = set()
    for msg in db.messages:
        for sig in msg.signals:
            dbc_names.add(sig.name)

    # Load sidecar
    with open("model/ecu_sidecar.yaml") as f:
        sidecar = yaml.safe_load(f)

    # Extract code signals
    code_signals = set()
    for ecu in ["cvc", "fzc", "rzc", "bcm", "icu", "tcu"]:
        src_dir = os.path.join("firmware", "ecu", ecu, "src")
        if not os.path.isdir(src_dir):
            continue
        for fname in os.listdir(src_dir):
            if not fname.endswith(".c"):
                continue
            with open(os.path.join(src_dir, fname), errors="replace") as f:
                for line in f:
                    for m in re.finditer(r"Rte_(?:Read|Write)\(\s*([A-Z_][A-Z0-9_]*)", line):
                        code_signals.add(m.group(1))

    # Build mapping
    mapping = {}
    internal = []
    failed = []

    for code_sig in sorted(code_signals):
        found = None

        # Strategy 1: sidecar rte_aliases
        for ecu_name in ["cvc", "fzc", "rzc", "bcm", "icu", "tcu"]:
            ecu_data = sidecar.get("ecus", {}).get(ecu_name, {})
            aliases = ecu_data.get("rte_aliases", {})
            if code_sig in aliases:
                # alias value: "Vehicle_State_VehicleState" or "Motor_Status_MotorSpeed_RPM"
                old_qualified = aliases[code_sig]
                # Split: find which message this belongs to
                for msg_name in MSG_PREFIX:
                    if old_qualified.startswith(msg_name + "_"):
                        old_signal = old_qualified[len(msg_name) + 1:]
                        new_name = old_to_new_signal(msg_name, old_signal)
                        if new_name and new_name in dbc_names:
                            found = new_name
                            break
                if found:
                    break

        # Strategy 2: already-qualified code name (BCM/ICU/TCU pattern)
        # BCM_SIG_VEHICLE_STATE_VEHICLE_STATE → extract message + signal
        if not found:
            for ecu_prefix in ["BCM_SIG_", "ICU_SIG_", "TCU_SIG_"]:
                if code_sig.startswith(ecu_prefix):
                    remainder = code_sig[len(ecu_prefix):]
                    # Try to match: VEHICLE_STATE_VEHICLE_STATE → Vehicle_State + VehicleState
                    for msg_name in MSG_PREFIX:
                        msg_upper = msg_name.upper().replace("_", "_")
                        if remainder.startswith(msg_upper + "_"):
                            old_signal = remainder[len(msg_upper) + 1:]
                            # Convert UPPER_SNAKE to PascalCase
                            parts = old_signal.split("_")
                            pascal = "".join(p.capitalize() for p in parts)
                            new_name = old_to_new_signal(msg_name, pascal)
                            if new_name and new_name in dbc_names:
                                found = new_name
                                break
                            # Try with original casing
                            new_name = old_to_new_signal(msg_name, old_signal)
                            if new_name and new_name in dbc_names:
                                found = new_name
                                break

        # Strategy 3: internal signal (not on CAN)
        if not found:
            # Check if this looks like an internal signal
            internal_prefixes = [
                "PEDAL_POSITION", "PEDAL_FAULT", "TORQUE_REQUEST",
                "FAULT_MASK", "SAFETY_STATUS", "HEARTBEAT_ALIVE",
                "SELF_TEST_RESULT", "COMM_STATUS", "CMD_TIMEOUT",
                "BUZZER_PATTERN", "PWM_DISABLE", "MOTOR_SPEED",
                "ENCODER", "TEMP_FAULT", "OVERCURRENT", "CURRENT_MA",
                "DERATING", "BATTERY", "MOTOR_DIR", "MOTOR_ENABLE",
                "MOTOR_FAULT", "MOTOR_CUTOFF", "ESTOP_ACTIVE",
                "VEHICLE_STATE", "BRAKE", "STEER", "LIDAR",
                "SC_RELAY", "TORQUE",
            ]
            is_internal = False
            for pfx in internal_prefixes:
                if pfx in code_sig:
                    is_internal = True
                    break

            if is_internal:
                internal.append(code_sig)
                mapping[code_sig] = "INTERNAL"
                continue

        if found:
            mapping[code_sig] = found
        else:
            failed.append(code_sig)
            mapping[code_sig] = "UNMAPPED"

    # Report
    mapped_count = sum(1 for v in mapping.values() if v not in ("INTERNAL", "UNMAPPED"))
    print("=== Code → DBC Signal Mapping ===")
    print("  Total code signals: %d" % len(code_signals))
    print("  Mapped to DBC:      %d" % mapped_count)
    print("  Internal (no CAN):  %d" % len(internal))
    print("  UNMAPPED (failed):  %d" % len(failed))
    print()

    if mapped_count > 0:
        print("Mapped signals:")
        for k, v in sorted(mapping.items()):
            if v not in ("INTERNAL", "UNMAPPED"):
                print("  %-45s → %s" % (k, v))
        print()

    if internal:
        print("Internal signals (RTE-only, no DBC):")
        for s in sorted(internal):
            print("  %s" % s)
        print()

    if failed:
        print("UNMAPPED (need manual resolution):")
        for s in sorted(failed):
            print("  %s" % s)
        print()

    with open("arxml_v2/code_to_dbc.json", "w") as f:
        json.dump(mapping, f, indent=2, sort_keys=True)
    print("Written: arxml_v2/code_to_dbc.json")

    if failed:
        return 1
    return 0


if __name__ == "__main__":
    exit(main())
