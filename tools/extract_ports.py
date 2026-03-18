#!/usr/bin/env python3
"""
Extract P-ports and R-ports from SWC code by scanning Rte_Read/Rte_Write.

For each SWC, produces:
  - P-port for every signal written via Rte_Write
  - R-port for every signal read via Rte_Read

Port names follow AUTOSAR convention:
  - PP_<signal_name> for provider ports
  - RP_<signal_name> for receiver ports

Maps code signal names to DBC names where possible.
Internal signals get their own S/R interface.

Output: ports_model.json consumed by dbc2arxml and arxml_wiring.
"""

import os
import re
import json
import cantools

FIRMWARE_ROOT = "firmware/ecu"
DBC_PATH = "gateway/taktflow_vehicle.dbc"
CODE_TO_DBC_PATH = "arxml_v2/code_to_dbc.json"
ECUS = ["cvc", "fzc", "rzc", "bcm", "icu", "tcu"]


def scan_rte_calls(filepath):
    """Extract Rte_Read and Rte_Write signal names from a .c file."""
    writes = set()
    reads = set()
    with open(filepath, "r", errors="replace") as f:
        for line in f:
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("/*"):
                continue
            for m in re.finditer(r"Rte_Write\(\s*([A-Z_][A-Z0-9_]*)", line):
                writes.add(m.group(1))
            for m in re.finditer(r"Rte_Read\(\s*([A-Z_][A-Z0-9_]*)", line):
                reads.add(m.group(1))
    return writes, reads


def get_swc_name(filename):
    """Swc_Battery.c → Swc_Battery, bcm_main.c → Swc_BcmMain"""
    base = filename[:-2]  # strip .c
    if base.startswith("Swc_"):
        return base
    if base == "bcm_main":
        return "Swc_BcmCan"
    if base == "main":
        return None  # skip main.c
    return None


def main():
    # Load DBC for signal name validation
    db = cantools.database.load_file(DBC_PATH)
    dbc_signals = set()
    for msg in db.messages:
        for sig in msg.signals:
            dbc_signals.add(sig.name)

    # Load code→DBC mapping
    with open(CODE_TO_DBC_PATH) as f:
        code_to_dbc = json.load(f)

    # Scan all SWCs
    model = {}  # ecu → {swc → {p_ports: [...], r_ports: [...]}}

    for ecu in ECUS:
        ecu_upper = ecu.upper()
        src_dir = os.path.join(FIRMWARE_ROOT, ecu, "src")
        if not os.path.isdir(src_dir):
            continue

        model[ecu_upper] = {}

        for fname in sorted(os.listdir(src_dir)):
            if not fname.endswith(".c"):
                continue

            swc_name = get_swc_name(fname)
            if swc_name is None:
                continue

            filepath = os.path.join(src_dir, fname)
            writes, reads = scan_rte_calls(filepath)

            p_ports = []
            for sig in sorted(writes):
                dbc_name = code_to_dbc.get(sig, "")
                if dbc_name in ("INTERNAL", "UNMAPPED", ""):
                    # Internal signal — use code name as port name
                    port_name = "PP_%s" % sig
                    interface = "SRI_%s" % sig
                    is_internal = True
                else:
                    port_name = "PP_%s" % dbc_name
                    interface = "SRI_%s" % dbc_name
                    is_internal = False
                p_ports.append({
                    "name": port_name,
                    "code_signal": sig,
                    "dbc_signal": dbc_name if not is_internal else None,
                    "interface": interface,
                    "internal": is_internal,
                })

            r_ports = []
            for sig in sorted(reads):
                dbc_name = code_to_dbc.get(sig, "")
                if dbc_name in ("INTERNAL", "UNMAPPED", ""):
                    port_name = "RP_%s" % sig
                    interface = "SRI_%s" % sig
                    is_internal = True
                else:
                    port_name = "RP_%s" % dbc_name
                    interface = "SRI_%s" % dbc_name
                    is_internal = False
                r_ports.append({
                    "name": port_name,
                    "code_signal": sig,
                    "dbc_signal": dbc_name if not is_internal else None,
                    "interface": interface,
                    "internal": is_internal,
                })

            if p_ports or r_ports:
                model[ecu_upper][swc_name] = {
                    "p_ports": p_ports,
                    "r_ports": r_ports,
                }

    # Summary
    total_p = 0
    total_r = 0
    total_internal = 0
    total_dbc = 0

    for ecu, swcs in sorted(model.items()):
        ecu_p = sum(len(s["p_ports"]) for s in swcs.values())
        ecu_r = sum(len(s["r_ports"]) for s in swcs.values())
        ecu_int = sum(
            sum(1 for p in s["p_ports"] if p["internal"]) +
            sum(1 for p in s["r_ports"] if p["internal"])
            for s in swcs.values()
        )
        print("%s: %d SWCs, %d P-ports, %d R-ports (%d internal)" % (
            ecu, len(swcs), ecu_p, ecu_r, ecu_int))
        total_p += ecu_p
        total_r += ecu_r
        total_internal += ecu_int
        total_dbc += (ecu_p + ecu_r - ecu_int)

    print()
    print("Total: %d P-ports, %d R-ports" % (total_p, total_r))
    print("  DBC-mapped: %d" % total_dbc)
    print("  Internal: %d" % total_internal)

    output_path = "arxml_v2/ports_model.json"
    with open(output_path, "w") as f:
        json.dump(model, f, indent=2)
    print("\nWritten: %s" % output_path)


if __name__ == "__main__":
    main()
