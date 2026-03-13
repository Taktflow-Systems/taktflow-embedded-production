#!/usr/bin/env python3
"""
SWC Extractor -- parse firmware headers to build ECU configuration model.

Reads *_Cfg.h, Rte_Cfg_*.c, Com_Cfg_*.c from taktflow-embedded/firmware/
and produces a structured JSON model suitable for ARXML generation or C codegen.

Usage:
    python tools/arxml/swc_extractor.py <firmware-root> <output-json>

Example:
    python tools/arxml/swc_extractor.py ../taktflow-embedded/firmware ecu_model.json
"""

import sys
import os
import re
import json
import glob


# ---------------------------------------------------------------------------
# Parsers
# ---------------------------------------------------------------------------

def parse_defines(filepath):
    """Extract #define NAME VALUE pairs from a C header file."""
    defines = {}
    if not os.path.isfile(filepath):
        return defines
    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            # Match: #define NAME value  or  #define NAME (expr)
            m = re.match(
                r"#define\s+(\w+)\s+([-]?\w+(?:\.\w+)?u?)\s*(?:/\*.*)?$", line
            )
            if m:
                name = m.group(1)
                val = m.group(2).rstrip("u").rstrip("U")
                try:
                    if val.startswith("0x") or val.startswith("0X"):
                        defines[name] = int(val, 16)
                    elif val.lstrip("-").isdigit():
                        defines[name] = int(val)
                    else:
                        defines[name] = val
                except ValueError:
                    defines[name] = val
    return defines


def parse_cfg_header(filepath, ecu_prefix):
    """Parse an ECU _Cfg.h and extract structured config."""
    defines = parse_defines(filepath)
    cfg = {
        "rte_signals": {},
        "com_tx_pdus": {},
        "com_rx_pdus": {},
        "dtc_events": {},
        "e2e_data_ids": {},
        "enums": {},
        "thresholds": {},
    }

    # Read file for CAN ID comments
    can_id_map = {}
    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(
                r"#define\s+(\w+)\s+\d+u?\s*/\*\s*CAN\s+(0x[0-9A-Fa-f]+)",
                line.strip(),
            )
            if m:
                can_id_map[m.group(1)] = int(m.group(2), 16)

    for name, val in defines.items():
        prefix = ecu_prefix.upper()

        # RTE Signal IDs
        if name.startswith(prefix + "_SIG_") and name != prefix + "_SIG_COUNT":
            cfg["rte_signals"][name] = val

        # Com TX PDU IDs
        elif name.startswith(prefix + "_COM_TX_"):
            can_id = can_id_map.get(name)
            cfg["com_tx_pdus"][name] = {"pdu_id": val, "can_id": can_id}

        # Com RX PDU IDs
        elif name.startswith(prefix + "_COM_RX_"):
            can_id = can_id_map.get(name)
            cfg["com_rx_pdus"][name] = {"pdu_id": val, "can_id": can_id}

        # DTC Event IDs
        elif name.startswith(prefix + "_DTC_"):
            cfg["dtc_events"][name] = val

        # E2E Data IDs
        elif name.startswith(prefix + "_E2E_") and "DATA_ID" in name:
            cfg["e2e_data_ids"][name] = val

        # State/fault enums
        elif (
            name.startswith(prefix + "_STATE_")
            or name.startswith(prefix + "_PEDAL_")
            or name.startswith(prefix + "_STEER_")
            and "FAULT" in name.upper()
            or name.startswith(prefix + "_BRAKE_")
            and "FAULT" in name.upper()
            or name.startswith(prefix + "_COMM_")
        ):
            cfg["enums"][name] = val

        # Thresholds and constants
        elif any(
            kw in name
            for kw in [
                "THRESHOLD",
                "DEBOUNCE",
                "TIMEOUT",
                "PERIOD",
                "LIMIT",
                "MAX",
                "MIN",
                "CYCLES",
            ]
        ):
            cfg["thresholds"][name] = val

    return cfg


def parse_rte_cfg(filepath, ecu_prefix):
    """Parse Rte_Cfg_*.c to extract runnable configuration."""
    runnables = []
    if not os.path.isfile(filepath):
        return runnables

    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    # Extract runnable entries: { FuncName, periodMs, priority, seId }
    pattern = re.compile(
        r"\{\s*(\w+)\s*,\s*(\d+)u?\s*,\s*(\d+)u?\s*,\s*(0x[0-9A-Fa-f]+|\d+)u?\s*\}"
    )
    # Find the runnable config array
    in_runnable = False
    for line in content.split("\n"):
        if "runnable_config" in line and "[]" in line:
            in_runnable = True
            continue
        if in_runnable and line.strip().startswith("};"):
            break
        if in_runnable:
            m = pattern.search(line)
            if m:
                func = m.group(1)
                period = int(m.group(2))
                priority = int(m.group(3))
                se_id_str = m.group(4)
                if se_id_str.startswith("0x"):
                    se_id = int(se_id_str, 16)
                else:
                    se_id = int(se_id_str)
                runnables.append(
                    {
                        "function": func,
                        "period_ms": period,
                        "priority": priority,
                        "wdgm_se_id": se_id,
                    }
                )

    return runnables


def parse_com_cfg(filepath, ecu_prefix):
    """Parse Com_Cfg_*.c to extract signal configuration."""
    signals = []
    if not os.path.isfile(filepath):
        return signals

    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    # Match signal entries: { id, bitPos, bitSize, type, pduId, &buffer }
    pattern = re.compile(
        r"\{\s*(\d+)u?\s*,\s*(\d+)u?\s*,\s*(\d+)u?\s*,\s*(\w+)\s*,"
        r"\s*(\w+)\s*,\s*&(\w+)\s*\}"
    )

    for m in pattern.finditer(content):
        signals.append(
            {
                "signal_id": int(m.group(1)),
                "bit_pos": int(m.group(2)),
                "bit_size": int(m.group(3)),
                "type": m.group(4),
                "pdu_id_name": m.group(5),
                "buffer_name": m.group(6),
            }
        )

    return signals


def find_swc_headers(ecu_dir):
    """Find all Swc_*.h headers in an ECU directory."""
    swcs = []
    include_dir = os.path.join(ecu_dir, "include")
    if not os.path.isdir(include_dir):
        return swcs

    for path in sorted(glob.glob(os.path.join(include_dir, "Swc_*.h"))):
        name = os.path.basename(path).replace(".h", "")
        # Check which functions are declared
        functions = []
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.match(r"\w+\s+(Swc_\w+)\s*\(", line.strip())
                if m:
                    functions.append(m.group(1))
        swcs.append({"name": name, "header": path, "functions": functions})

    return swcs


# ---------------------------------------------------------------------------
# Main extractor
# ---------------------------------------------------------------------------

ECU_MAP = {
    "cvc": {"prefix": "CVC", "cfg_header": "Cvc_Cfg.h"},
    "fzc": {"prefix": "FZC", "cfg_header": "Fzc_Cfg.h"},
    "rzc": {"prefix": "RZC", "cfg_header": "Rzc_Cfg.h"},
    "bcm": {"prefix": "BCM", "cfg_header": "Bcm_Cfg.h"},
    "icu": {"prefix": "ICU", "cfg_header": "Icu_Cfg.h"},
    "tcu": {"prefix": "TCU", "cfg_header": "Tcu_Cfg.h"},
}


def extract_all(firmware_root):
    """Extract configuration from all ECUs."""
    model = {"ecus": {}}

    for ecu_name, ecu_info in ECU_MAP.items():
        ecu_dir = os.path.join(firmware_root, ecu_name)
        if not os.path.isdir(ecu_dir):
            print("  Skipping %s (not found)" % ecu_name)
            continue

        prefix = ecu_info["prefix"]
        cfg_path = os.path.join(ecu_dir, "include", ecu_info["cfg_header"])
        rte_cfg_path = os.path.join(ecu_dir, "cfg", "Rte_Cfg_%s.c" % prefix.capitalize())
        com_cfg_path = os.path.join(ecu_dir, "cfg", "Com_Cfg_%s.c" % prefix.capitalize())

        ecu_model = {
            "prefix": prefix,
            "cfg": parse_cfg_header(cfg_path, prefix),
            "runnables": parse_rte_cfg(rte_cfg_path, prefix),
            "com_signals": parse_com_cfg(com_cfg_path, prefix),
            "swcs": find_swc_headers(ecu_dir),
        }

        model["ecus"][ecu_name] = ecu_model
        print(
            "  %s: %d signals, %d runnables, %d SWCs, %d DTCs"
            % (
                prefix,
                len(ecu_model["cfg"]["rte_signals"]),
                len(ecu_model["runnables"]),
                len(ecu_model["swcs"]),
                len(ecu_model["cfg"]["dtc_events"]),
            )
        )

    return model


def main():
    if len(sys.argv) < 3:
        print("Usage: %s <firmware-root> <output-json>" % sys.argv[0])
        sys.exit(1)

    firmware_root = sys.argv[1]
    output_path = sys.argv[2]

    print("Extracting ECU configurations from %s..." % firmware_root)
    model = extract_all(firmware_root)

    # Summary
    total_signals = sum(
        len(e["cfg"]["rte_signals"]) for e in model["ecus"].values()
    )
    total_runnables = sum(len(e["runnables"]) for e in model["ecus"].values())
    total_swcs = sum(len(e["swcs"]) for e in model["ecus"].values())
    total_dtcs = sum(
        len(e["cfg"]["dtc_events"]) for e in model["ecus"].values()
    )

    print("")
    print("Totals: %d ECUs, %d RTE signals, %d runnables, %d SWCs, %d DTCs"
          % (len(model["ecus"]), total_signals, total_runnables, total_swcs, total_dtcs))

    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(model, f, indent=2)
    print("Written: %s" % output_path)


if __name__ == "__main__":
    main()
