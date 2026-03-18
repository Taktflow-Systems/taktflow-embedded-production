#!/usr/bin/env python3
"""
Build mapping: code signal names (CVC_SIG_*) → DBC signal names (VehSt_*).

Reads:
  - Code: all Rte_Read/Rte_Write signal names from SWC files
  - DBC: all qualified signal names from taktflow_vehicle.dbc
  - Sidecar: rte_aliases that map code → old DBC names

Outputs: code_to_dbc.json mapping
"""

import cantools
import yaml
import json
import os
import re
import sys

# 1. Load DBC signals
db = cantools.database.load_file("gateway/taktflow_vehicle.dbc")
dbc_signals = {}
for msg in db.messages:
    for sig in msg.signals:
        dbc_signals[sig.name] = {"message": msg.name, "can_id": msg.frame_id}

# 2. Load sidecar rte_aliases
with open("model/ecu_sidecar.yaml") as f:
    sidecar = yaml.safe_load(f)

# Build alias → DBC name map
# Sidecar alias format: RZC_SIG_BATTERY_MV: Battery_Status_BatteryVoltage_mV
# New DBC name: BatSt_BatteryVoltage_mV
# Old DBC name: BatteryVoltage_mV (unqualified, now gone)
# We need: alias value → find in new DBC by matching the signal part

# Strategy: for each sidecar alias value like "Battery_Status_BatteryVoltage_mV",
# extract the signal part after the message name, find it in new DBC
# New DBC: signal name = <MsgPrefix>_<SignalPart>

# 3. Extract all code signal names from firmware
code_signals = set()
for ecu in ["cvc", "fzc", "rzc", "bcm", "icu", "tcu"]:
    src_dir = os.path.join("firmware", "ecu", ecu, "src")
    if not os.path.isdir(src_dir):
        continue
    for fname in os.listdir(src_dir):
        if not fname.endswith(".c"):
            continue
        fpath = os.path.join(src_dir, fname)
        with open(fpath, errors="replace") as f:
            for line in f:
                for m in re.finditer(r"Rte_(?:Read|Write)\(\s*([A-Z_][A-Z0-9_]*)", line):
                    code_signals.add(m.group(1))

print("Code signal names: %d unique" % len(code_signals))
print("DBC signal names: %d" % len(dbc_signals))

# 4. Build mapping
# The code signal name like "CVC_SIG_VEHICLE_STATE" needs to map to "VehSt_VehicleState"
# The generated Cfg.h has: #define CVC_SIG_VEHICLE_STATE_VEHICLE_STATE  189u
# The rte_alias has: CVC_SIG_VEHICLE_STATE: Vehicle_State_VehicleState
# The old DBC had: VehicleState in message Vehicle_State
# The new DBC has: VehSt_VehicleState in message Vehicle_State

# For now, output all code signals and mark which ones we can map
mapped = 0
unmapped = 0
mapping = {}

for code_sig in sorted(code_signals):
    # Try to find via sidecar aliases
    found_dbc = None
    for ecu_name in ["cvc", "fzc", "rzc", "bcm", "icu", "tcu"]:
        ecu_data = sidecar.get("ecus", {}).get(ecu_name, {})
        aliases = ecu_data.get("rte_aliases", {})
        if code_sig in aliases:
            old_qualified = aliases[code_sig]  # e.g., "Vehicle_State_VehicleState"
            # The new DBC signal is the qualified version
            # Split: Message_Part_Signal_Part → find in new DBC
            for dbc_name, dbc_info in dbc_signals.items():
                # Match by checking if the signal part matches
                msg_name = dbc_info["message"]
                # old_qualified = "Vehicle_State_VehicleState"
                # msg_name = "Vehicle_State"
                # signal_part = "VehicleState"
                if old_qualified.startswith(msg_name + "_"):
                    signal_part = old_qualified[len(msg_name) + 1:]
                    if signal_part.lower().replace("_", "") in dbc_name.lower().replace("_", ""):
                        found_dbc = dbc_name
                        break
            if found_dbc:
                break

    if found_dbc:
        mapping[code_sig] = found_dbc
        mapped += 1
    else:
        unmapped += 1

print("\nMapped: %d" % mapped)
print("Unmapped: %d" % unmapped)

if unmapped > 0:
    print("\nUnmapped signals:")
    for code_sig in sorted(code_signals):
        if code_sig not in mapping:
            print("  %s" % code_sig)

# Save mapping
with open("arxml_v2/code_to_dbc.json", "w") as f:
    json.dump(mapping, f, indent=2, sort_keys=True)
print("\nWritten: arxml_v2/code_to_dbc.json")
