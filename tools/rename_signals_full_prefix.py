#!/usr/bin/env python3
"""
Rename all DBC signals from abbreviated prefix to full message name prefix.

  EStBc_EStop_Active → EStop_Broadcast_EStop_Active
  VehSt_Mode → Vehicle_State_Mode

Reads taktflow_vehicle.dbc, replaces all signal names, writes back.
"""

import re
import sys

DBC_PATH = "gateway/taktflow_vehicle.dbc"

# Build prefix map: old abbreviated → message name
# Must match exactly what dbc_split.py defined
ABBREV_TO_MSG = {
    "EStBc": "EStop_Broadcast",
    "CvcHb": "CVC_Heartbeat",
    "FzcHb": "FZC_Heartbeat",
    "RzcHb": "RZC_Heartbeat",
    "ScSt": "SC_Status",
    "IcuHb": "ICU_Heartbeat",
    "TcuHb": "TCU_Heartbeat",
    "BcmHb": "BCM_Heartbeat",
    "VehSt": "Vehicle_State",
    "TrqRq": "Torque_Request",
    "StrCmd": "Steer_Command",
    "BrkCmd": "Brake_Command",
    "StrSt": "Steering_Status",
    "BrkSt": "Brake_Status",
    "BrkFlt": "Brake_Fault",
    "MtrCut": "Motor_Cutoff_Req",
    "LidarD": "Lidar_Distance",
    "MtrSt": "Motor_Status",
    "MtrCur": "Motor_Current",
    "MtrTmp": "Motor_Temperature",
    "BatSt": "Battery_Status",
    "BdyCmd": "Body_Control_Cmd",
    "LtSt": "Light_Status",
    "IndSt": "Indicator_State",
    "DlkSt": "Door_Lock_Status",
    "DtcBc": "DTC_Broadcast",
}


def main():
    with open(DBC_PATH, "r") as f:
        content = f.read()

    rename_count = 0
    for abbrev, msg_name in sorted(ABBREV_TO_MSG.items(), key=lambda x: -len(x[0])):
        # Replace abbrev_ with msg_name_ in signal contexts
        # Match: word boundary + abbrev + underscore
        old_pattern = r'\b' + re.escape(abbrev) + r'_'
        new_replacement = msg_name + '_'

        new_content = re.sub(old_pattern, new_replacement, content)
        if new_content != content:
            count = len(re.findall(old_pattern, content))
            rename_count += count
            content = new_content

    with open(DBC_PATH, "w") as f:
        f.write(content)

    print("Renamed %d occurrences" % rename_count)
    print("Written: %s" % DBC_PATH)

    # Validate
    import cantools
    db = cantools.database.load_file(DBC_PATH)
    all_sigs = [s.name for m in db.messages for s in m.signals]
    dupes = set([s for s in all_sigs if all_sigs.count(s) > 1])
    print("Signals: %d, Duplicates: %s" % (len(all_sigs), dupes or "NONE"))

    # Show longest
    longest = max(all_sigs, key=len)
    print("Longest: %d chars (%s)" % (len(longest), longest))


if __name__ == "__main__":
    main()
