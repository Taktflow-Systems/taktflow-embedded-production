#!/usr/bin/env python3
"""
Build assembly connector map from firmware Rte_Read/Rte_Write calls.

Scans SWC source code for:
  - Rte_Write(SIGNAL, val) → P-port: this SWC produces SIGNAL
  - Rte_Read(SIGNAL, &val) → R-port: this SWC consumes SIGNAL

Then for each ECU, finds intra-ECU connections:
  SWC_A writes SIGNAL, SWC_B reads SIGNAL, both on same ECU → assembly connector

Cross-ECU flows go via CAN (DBC signals), not assembly connectors.

Output: connector map as JSON + summary.
"""

import os
import re
import json
import sys
from collections import defaultdict

FIRMWARE_ROOT = "firmware/ecu"
ECUS = ["cvc", "fzc", "rzc", "bcm", "icu", "tcu"]


def extract_rte_calls(filepath):
    """Extract Rte_Read and Rte_Write signal names from a .c file."""
    writes = set()
    reads = set()
    with open(filepath, "r", errors="replace") as f:
        for line in f:
            line = line.strip()
            if line.startswith("//") or line.startswith("/*"):
                continue
            # Rte_Write(SIGNAL_NAME, value)
            m = re.search(r"Rte_Write\(\s*([A-Z_][A-Z0-9_]*)", line)
            if m:
                writes.add(m.group(1))
            # Rte_Read(SIGNAL_NAME, &value)
            m = re.search(r"Rte_Read\(\s*([A-Z_][A-Z0-9_]*)", line)
            if m:
                reads.add(m.group(1))
    return writes, reads


def get_swc_name(filepath):
    """Extract SWC name from file path: Swc_Battery.c → Swc_Battery"""
    basename = os.path.basename(filepath)
    if basename.startswith("Swc_") and basename.endswith(".c"):
        return basename[:-2]  # remove .c
    return None


def main():
    all_connectors = {}
    total_intra = 0
    total_cross_ecu = 0

    for ecu in ECUS:
        ecu_upper = ecu.upper()
        src_dir = os.path.join(FIRMWARE_ROOT, ecu, "src")
        if not os.path.isdir(src_dir):
            continue

        # Collect writes and reads per SWC
        swc_writes = {}  # signal → swc_name
        swc_reads = defaultdict(list)  # signal → [swc_name, ...]

        for fname in sorted(os.listdir(src_dir)):
            if not fname.endswith(".c"):
                continue
            fpath = os.path.join(src_dir, fname)
            swc_name = get_swc_name(fpath)
            if swc_name is None:
                # Non-SWC files (main.c, etc.)
                swc_name = fname[:-2]

            writes, reads = extract_rte_calls(fpath)
            for sig in writes:
                swc_writes[sig] = swc_name
            for sig in reads:
                swc_reads[sig].append(swc_name)

        # Find intra-ECU connections
        connectors = []
        for sig, writer in swc_writes.items():
            for reader in swc_reads.get(sig, []):
                if reader != writer:
                    connectors.append({
                        "signal": sig,
                        "provider": writer,
                        "requester": reader,
                        "ecu": ecu_upper,
                    })

        all_connectors[ecu_upper] = connectors
        total_intra += len(connectors)

        # Count cross-ECU (signal written here, read by different ECU)
        # We can detect these by looking at signals that are written but not read locally
        for sig in swc_writes:
            if sig not in swc_reads or len(swc_reads[sig]) == 0:
                total_cross_ecu += 1

    # Print summary
    print("=== Assembly Connector Map ===")
    print()
    for ecu in ECUS:
        ecu_upper = ecu.upper()
        conns = all_connectors.get(ecu_upper, [])
        print("--- %s: %d intra-ECU connectors ---" % (ecu_upper, len(conns)))
        for c in sorted(conns, key=lambda x: x["signal"]):
            print("  %s.P_%s → %s.R_%s" % (
                c["provider"], c["signal"],
                c["requester"], c["signal"]))
        print()

    print("=== Summary ===")
    print("  Intra-ECU connectors: %d" % total_intra)
    print("  Cross-ECU signals (via CAN): %d" % total_cross_ecu)
    print("  Total data flows: %d" % (total_intra + total_cross_ecu))

    # Write JSON
    output_path = "arxml_v2/connector_map.json"
    with open(output_path, "w") as f:
        json.dump(all_connectors, f, indent=2)
    print("\n  Written: %s" % output_path)


if __name__ == "__main__":
    main()
