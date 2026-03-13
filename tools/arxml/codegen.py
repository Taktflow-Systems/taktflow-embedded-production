#!/usr/bin/env python3
"""
ARXML + ECU Model -> C Code Generator

Reads the ECU model JSON (from swc_extractor.py) and generates:
  - *_Cfg.h       -- RTE signal IDs, Com PDU IDs, DTC IDs, enums, thresholds
  - Com_Cfg_*.c   -- Com signal config, TX/RX PDU config tables
  - Rte_Cfg_*.c   -- RTE signal config, runnable config tables

Usage:
    python tools/arxml/codegen.py <ecu-model.json> <output-dir>

Example:
    python tools/arxml/codegen.py model/ecu_model.json generated/
"""

import sys
import os
import json


# ---------------------------------------------------------------------------
# Code templates
# ---------------------------------------------------------------------------

FILE_HEADER = """\
/**
 * @file    {filename}
 * @brief   {brief}
 * @date    2026-03-10
 *
 * GENERATED -- DO NOT EDIT
 * Source: ARXML architecture migration (tools/arxml/codegen.py)
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
"""


def gen_cfg_header(ecu_name, prefix, cfg):
    """Generate *_Cfg.h content."""
    guard = "%s_CFG_H" % prefix
    lines = []
    lines.append(FILE_HEADER.format(
        filename="%s_Cfg.h" % prefix.capitalize(),
        brief="%s configuration -- all %s-specific ID definitions" % (prefix, prefix),
    ))
    lines.append("#ifndef %s" % guard)
    lines.append("#define %s" % guard)
    lines.append("")

    # RTE Signal IDs
    lines.append("/* " + "=" * 68 + " */")
    lines.append("/* RTE Signal IDs (extends BSW well-known IDs at offset 16)")
    lines.append(" * " + "=" * 66 + " */")
    lines.append("")
    for name, val in sorted(cfg["rte_signals"].items(), key=lambda x: x[1]):
        lines.append("#define %-35s %du" % (name, val))
    sig_count_name = "%s_SIG_COUNT" % prefix
    if cfg["rte_signals"]:
        max_sig = max(cfg["rte_signals"].values()) + 1
        lines.append("#define %-35s %du" % (sig_count_name, max_sig))
    lines.append("")

    # Com TX PDU IDs
    lines.append("/* " + "=" * 68 + " */")
    lines.append("/* Com TX PDU IDs")
    lines.append(" * " + "=" * 66 + " */")
    lines.append("")
    for name, info in sorted(cfg["com_tx_pdus"].items(), key=lambda x: x[1]["pdu_id"]):
        can_comment = ""
        if info.get("can_id") is not None:
            can_comment = "   /* CAN 0x%03X */" % info["can_id"]
        lines.append("#define %-35s %du%s" % (name, info["pdu_id"], can_comment))
    lines.append("")

    # Com RX PDU IDs
    lines.append("/* " + "=" * 68 + " */")
    lines.append("/* Com RX PDU IDs")
    lines.append(" * " + "=" * 66 + " */")
    lines.append("")
    for name, info in sorted(cfg["com_rx_pdus"].items(), key=lambda x: x[1]["pdu_id"]):
        can_comment = ""
        if info.get("can_id") is not None:
            can_comment = "   /* CAN 0x%03X */" % info["can_id"]
        lines.append("#define %-35s %du%s" % (name, info["pdu_id"], can_comment))
    lines.append("")

    # DTC Event IDs
    if cfg["dtc_events"]:
        lines.append("/* " + "=" * 68 + " */")
        lines.append("/* DTC Event IDs (Dem_EventIdType)")
        lines.append(" * " + "=" * 66 + " */")
        lines.append("")
        for name, val in sorted(cfg["dtc_events"].items(), key=lambda x: x[1]):
            lines.append("#define %-35s %du" % (name, val))
        lines.append("")

    # E2E Data IDs
    if cfg["e2e_data_ids"]:
        lines.append("/* " + "=" * 68 + " */")
        lines.append("/* E2E Data IDs")
        lines.append(" * " + "=" * 66 + " */")
        lines.append("")
        for name, val in sorted(cfg["e2e_data_ids"].items(), key=lambda x: x[1]):
            lines.append("#define %-35s 0x%02Xu" % (name, val))
        lines.append("")

    # Enums
    if cfg["enums"]:
        lines.append("/* " + "=" * 68 + " */")
        lines.append("/* State and Fault Enums")
        lines.append(" * " + "=" * 66 + " */")
        lines.append("")
        for name, val in sorted(cfg["enums"].items()):
            if isinstance(val, int):
                lines.append("#define %-35s %du" % (name, val))
            else:
                lines.append("#define %-35s %s" % (name, val))
        lines.append("")

    # Thresholds
    if cfg["thresholds"]:
        lines.append("/* " + "=" * 68 + " */")
        lines.append("/* Thresholds and Constants")
        lines.append(" * " + "=" * 66 + " */")
        lines.append("")
        for name, val in sorted(cfg["thresholds"].items()):
            if isinstance(val, int):
                lines.append("#define %-35s %du" % (name, val))
            else:
                lines.append("#define %-35s %s" % (name, val))
        lines.append("")

    lines.append("#endif /* %s */" % guard)
    lines.append("")
    return "\n".join(lines)


def gen_rte_cfg(ecu_name, prefix, runnables, cfg):
    """Generate Rte_Cfg_*.c content."""
    lines = []
    lines.append(FILE_HEADER.format(
        filename="Rte_Cfg_%s.c" % prefix.capitalize(),
        brief="RTE configuration for %s -- signal table and runnable table" % prefix,
    ))
    lines.append('#include "Rte.h"')
    lines.append('#include "%s_Cfg.h"' % prefix.capitalize())
    lines.append("")

    # Forward declarations
    lines.append("/* " + "=" * 68 + " */")
    lines.append("/* Forward declarations for SWC runnables")
    lines.append(" * " + "=" * 66 + " */")
    lines.append("")
    declared = set()
    for r in runnables:
        func = r["function"]
        if func not in declared:
            lines.append("extern void %s(void);" % func)
            declared.add(func)
    lines.append("")

    # Signal config table
    sig_count_name = "%s_SIG_COUNT" % prefix
    lines.append("/* " + "=" * 68 + " */")
    lines.append("/* Signal Configuration Table")
    lines.append(" * " + "=" * 66 + " */")
    lines.append("")
    lines.append(
        "static const Rte_SignalConfigType %s_signal_config[%s] = {"
        % (ecu_name, sig_count_name)
    )

    # BSW well-known (0-15) + ECU-specific
    rte_signals = cfg.get("rte_signals", {})
    max_id = max(rte_signals.values()) if rte_signals else 15
    sig_count = max_id + 1

    for i in range(sig_count):
        # Find name for this ID
        sig_name = None
        for name, val in rte_signals.items():
            if val == i:
                sig_name = name
                break

        if i < 16:
            # BSW well-known
            comment = "/* %d: BSW reserved */" % i
            lines.append("    { %2du, 0u },   %s" % (i, comment))
        elif sig_name:
            lines.append("    { %s, 0u },   /* %d */" % (sig_name, i))
        else:
            lines.append("    { %2du, 0u },   /* %d: unused */" % (i, i))

    lines.append("};")
    lines.append("")

    # Runnable config table
    lines.append("/* " + "=" * 68 + " */")
    lines.append("/* Runnable Configuration Table")
    lines.append(" * " + "=" * 66 + " */")
    lines.append("")
    lines.append(
        "static const Rte_RunnableConfigType %s_runnable_config[] = {" % ecu_name
    )
    lines.append("    /* func, periodMs, priority, seId */")
    for r in runnables:
        se_id = r["wdgm_se_id"]
        if se_id == 255:
            se_str = "0xFFu"
        else:
            se_str = "%du" % se_id
        lines.append(
            "    { %-40s %3du, %2du, %6s },"
            % (r["function"] + ",", r["period_ms"], r["priority"], se_str)
        )
    lines.append("};")
    lines.append("")

    lines.append(
        "#define %s_RUNNABLE_COUNT  "
        "(sizeof(%s_runnable_config) / sizeof(%s_runnable_config[0]))"
        % (prefix, ecu_name, ecu_name)
    )
    lines.append("")

    # Aggregate config
    lines.append("/* " + "=" * 68 + " */")
    lines.append("/* Aggregate RTE Configuration")
    lines.append(" * " + "=" * 66 + " */")
    lines.append("")
    lines.append("const Rte_ConfigType %s_rte_config = {" % ecu_name)
    lines.append("    .signalConfig   = %s_signal_config," % ecu_name)
    lines.append("    .signalCount    = %s," % sig_count_name)
    lines.append("    .runnableConfig = %s_runnable_config," % ecu_name)
    lines.append(
        "    .runnableCount  = (uint8)%s_RUNNABLE_COUNT," % prefix
    )
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def gen_com_cfg(ecu_name, prefix, com_signals, cfg):
    """Generate Com_Cfg_*.c content."""
    lines = []
    lines.append(FILE_HEADER.format(
        filename="Com_Cfg_%s.c" % prefix.capitalize(),
        brief="Com module configuration for %s -- TX/RX PDUs and signal mappings" % prefix,
    ))
    lines.append('#include "Com.h"')
    lines.append('#include "%s_Cfg.h"' % prefix.capitalize())
    lines.append("")

    if not com_signals:
        lines.append("/* No Com signal data extracted -- manual review needed */")
        lines.append("")
        return "\n".join(lines)

    # Classify TX vs RX by signal ID ordering convention
    # (TX signals typically have lower IDs, but use buffer name as hint)
    tx_signals = [s for s in com_signals if "tx" in s["buffer_name"].lower()]
    rx_signals = [s for s in com_signals if "rx" in s["buffer_name"].lower()]

    # Shadow buffer declarations
    lines.append("/* " + "=" * 68 + " */")
    lines.append("/* Shadow Buffers")
    lines.append(" * " + "=" * 66 + " */")
    lines.append("")

    type_map = {
        "COM_UINT8": "uint8",
        "COM_UINT16": "uint16",
        "COM_SINT16": "sint16",
        "COM_UINT32": "uint32",
    }

    for s in com_signals:
        c_type = type_map.get(s["type"], "uint8")
        lines.append("static %-8s %s;" % (c_type, s["buffer_name"]))
    lines.append("")

    # Signal config table
    lines.append("/* " + "=" * 68 + " */")
    lines.append("/* Signal Configuration Table")
    lines.append(" * " + "=" * 66 + " */")
    lines.append("")
    lines.append("static const Com_SignalConfigType %s_signal_config[] = {" % ecu_name)
    lines.append("    /* id, bitPos, bitSize, type, pduId, shadowBuf */")
    for s in com_signals:
        lines.append(
            "    { %2du, %3du, %3du, %-12s %-35s &%-30s },"
            % (
                s["signal_id"],
                s["bit_pos"],
                s["bit_size"],
                s["type"] + ",",
                s["pdu_id_name"] + ",",
                s["buffer_name"],
            )
        )
    lines.append("};")
    lines.append("")
    lines.append(
        "#define %s_COM_SIGNAL_COUNT  "
        "(sizeof(%s_signal_config) / sizeof(%s_signal_config[0]))"
        % (prefix, ecu_name, ecu_name)
    )
    lines.append("")

    # TX PDU config (from cfg header)
    tx_pdus = cfg.get("com_tx_pdus", {})
    if tx_pdus:
        lines.append("/* " + "=" * 68 + " */")
        lines.append("/* TX PDU Configuration Table")
        lines.append(" * " + "=" * 66 + " */")
        lines.append("")
        lines.append(
            "static const Com_TxPduConfigType %s_tx_pdu_config[] = {" % ecu_name
        )
        lines.append("    /* pduId, dlc, cycleMs */")
        for name, info in sorted(tx_pdus.items(), key=lambda x: x[1]["pdu_id"]):
            lines.append("    { %-35s 8u, 0u }," % (name + ","))
        lines.append("};")
        lines.append("")
        lines.append(
            "#define %s_COM_TX_PDU_COUNT  "
            "(sizeof(%s_tx_pdu_config) / sizeof(%s_tx_pdu_config[0]))"
            % (prefix, ecu_name, ecu_name)
        )
        lines.append("")

    # RX PDU config
    rx_pdus = cfg.get("com_rx_pdus", {})
    if rx_pdus:
        lines.append("/* " + "=" * 68 + " */")
        lines.append("/* RX PDU Configuration Table")
        lines.append(" * " + "=" * 66 + " */")
        lines.append("")
        lines.append(
            "static const Com_RxPduConfigType %s_rx_pdu_config[] = {" % ecu_name
        )
        lines.append("    /* pduId, dlc, timeoutMs */")
        for name, info in sorted(rx_pdus.items(), key=lambda x: x[1]["pdu_id"]):
            lines.append("    { %-35s 8u, 0u }," % (name + ","))
        lines.append("};")
        lines.append("")
        lines.append(
            "#define %s_COM_RX_PDU_COUNT  "
            "(sizeof(%s_rx_pdu_config) / sizeof(%s_rx_pdu_config[0]))"
            % (prefix, ecu_name, ecu_name)
        )
        lines.append("")

    # Aggregate config
    lines.append("/* " + "=" * 68 + " */")
    lines.append("/* Aggregate Com Configuration")
    lines.append(" * " + "=" * 66 + " */")
    lines.append("")
    lines.append("const Com_ConfigType %s_com_config = {" % ecu_name)
    lines.append("    .signalConfig = %s_signal_config," % ecu_name)
    lines.append("    .signalCount  = (uint8)%s_COM_SIGNAL_COUNT," % prefix)
    if tx_pdus:
        lines.append("    .txPduConfig  = %s_tx_pdu_config," % ecu_name)
        lines.append("    .txPduCount   = (uint8)%s_COM_TX_PDU_COUNT," % prefix)
    if rx_pdus:
        lines.append("    .rxPduConfig  = %s_rx_pdu_config," % ecu_name)
        lines.append("    .rxPduCount   = (uint8)%s_COM_RX_PDU_COUNT," % prefix)
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) < 3:
        print("Usage: %s <ecu-model.json> <output-dir>" % sys.argv[0])
        sys.exit(1)

    model_path = sys.argv[1]
    output_dir = sys.argv[2]

    with open(model_path, "r") as f:
        model = json.load(f)

    print("Generating C configs from ECU model...")

    for ecu_name, ecu_data in model["ecus"].items():
        prefix = ecu_data["prefix"]
        cfg = ecu_data["cfg"]
        runnables = ecu_data["runnables"]
        com_signals = ecu_data["com_signals"]

        ecu_dir = os.path.join(output_dir, ecu_name)
        cfg_dir = os.path.join(ecu_dir, "cfg")
        inc_dir = os.path.join(ecu_dir, "include")
        os.makedirs(cfg_dir, exist_ok=True)
        os.makedirs(inc_dir, exist_ok=True)

        # Generate *_Cfg.h
        cfg_h = gen_cfg_header(ecu_name, prefix, cfg)
        cfg_h_path = os.path.join(inc_dir, "%s_Cfg.h" % prefix.capitalize())
        with open(cfg_h_path, "w", encoding="utf-8") as f:
            f.write(cfg_h)
        print("  %s" % cfg_h_path)

        # Generate Rte_Cfg_*.c
        if runnables:
            rte_cfg = gen_rte_cfg(ecu_name, prefix, runnables, cfg)
            rte_path = os.path.join(cfg_dir, "Rte_Cfg_%s.c" % prefix.capitalize())
            with open(rte_path, "w", encoding="utf-8") as f:
                f.write(rte_cfg)
            print("  %s" % rte_path)

        # Generate Com_Cfg_*.c
        if com_signals:
            com_cfg = gen_com_cfg(ecu_name, prefix, com_signals, cfg)
            com_path = os.path.join(cfg_dir, "Com_Cfg_%s.c" % prefix.capitalize())
            with open(com_path, "w", encoding="utf-8") as f:
                f.write(com_cfg)
            print("  %s" % com_path)

    print("\nDone. Generated configs in %s/" % output_dir)


if __name__ == "__main__":
    main()
