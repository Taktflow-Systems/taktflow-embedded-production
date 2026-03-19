#!/usr/bin/env python3
"""
generate_all.py — Single command: DBC + code → complete ARXML + configs.

Input:
  - gateway/taktflow_vehicle.dbc (source of truth)
  - firmware/ecu/*/src/Swc_*.c (code scan for ports + connectors)
  - model/ecu_sidecar.yaml (thresholds, DTCs, scheduling)

Output:
  - arxml/TaktflowSystem.arxml (complete: comm + SWCs + connectors + mappings)
  - firmware/ecu/*/cfg/* (generated configs via arxmlgen)

Usage:
    python3 tools/generate_all.py

No intermediate files. No manual steps. No pipeline ordering.
"""

import os
import sys
import re
import json
import subprocess

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(PROJECT_ROOT)

DBC_PATH = "gateway/taktflow_vehicle.dbc"  # unified: production + diag + virtual sensors
SWC_MODEL_PATH = "arxml_v2/swc_model.json"
ARXML_OUTPUT = "arxml/TaktflowSystem.arxml"
ARXML_V2_DIR = "arxml_v2"
FIRMWARE_ROOT = "firmware/ecu"
ECUS = ["cvc", "fzc", "rzc", "bcm", "icu", "tcu"]


def step(name):
    print("\n=== %s ===" % name)


def run(cmd):
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print("  FAILED: %s" % result.stderr[-200:])
        return False
    return True


def scan_rte_calls(filepath):
    """Extract Rte_Read/Rte_Write signal names from a .c file."""
    writes = set()
    reads = set()
    with open(filepath, "r", errors="replace") as f:
        for line in f:
            for m in re.finditer(r"Rte_Write\(\s*([A-Z_][A-Z0-9_]*)", line):
                writes.add(m.group(1))
            for m in re.finditer(r"Rte_Read\(\s*([A-Z_][A-Z0-9_]*)", line):
                reads.add(m.group(1))
    return writes, reads


def get_swc_name(filename):
    base = filename[:-2]
    if base.startswith("Swc_"):
        return base
    return None


def main():
    errors = 0

    # ----------------------------------------------------------------
    step("1. Validate DBC")
    # ----------------------------------------------------------------
    import cantools
    db = cantools.database.load_file(DBC_PATH)
    all_sigs = [s.name for m in db.messages for s in m.signals]
    dupes = set(s for s in all_sigs if all_sigs.count(s) > 1)
    print("  %d messages, %d signals, %d dupes" % (len(db.messages), len(all_sigs), len(dupes)))
    if dupes:
        print("  ERROR: duplicate signals: %s" % dupes)
        errors += 1

    # ----------------------------------------------------------------
    step("2. Extract SWC model from code")
    # ----------------------------------------------------------------
    os.makedirs(ARXML_V2_DIR, exist_ok=True)
    if not run("python3 tools/arxml/swc_extractor.py %s %s" % (FIRMWARE_ROOT, SWC_MODEL_PATH)):
        print("  ERROR: swc_extractor failed")
        errors += 1

    # ----------------------------------------------------------------
    step("3. Generate base ARXML (DBC + SWC model)")
    # ----------------------------------------------------------------
    if not run("python3 tools/arxml/dbc2arxml.py %s %s/ %s" % (DBC_PATH, ARXML_V2_DIR, SWC_MODEL_PATH)):
        print("  ERROR: dbc2arxml failed")
        errors += 1

    # ----------------------------------------------------------------
    step("4. Copy base ARXML + run arxmlgen for Cfg.h")
    # ----------------------------------------------------------------
    import shutil
    arxml_path = os.path.join(ARXML_V2_DIR, "TaktflowSystem.arxml")
    shutil.copy(arxml_path, ARXML_OUTPUT)
    print("  Copied: %s → %s" % (arxml_path, ARXML_OUTPUT))

    if not run("python3 -m tools.arxmlgen --config project.yaml"):
        print("  ERROR: arxmlgen failed")
        errors += 1
    else:
        for ecu in ECUS:
            for f in os.listdir(os.path.join(FIRMWARE_ROOT, ecu, "src")):
                if re.match(r"Swc_[A-Z]{3}\.c$", f):
                    os.remove(os.path.join(FIRMWARE_ROOT, ecu, "src", f))
        print("  Cfg.h generated with correct signal names")

    # ----------------------------------------------------------------
    step("5. Add self-test SWCs + scan code ports + inject")
    # ----------------------------------------------------------------
    import autosar_data as asr
    import autosar_data.abstraction as abst
    from autosar_data.abstraction.software_component import (
        ApplicationSwComponentType, SenderReceiverInterface,
    )

    arxml_path = os.path.join(ARXML_V2_DIR, "TaktflowSystem.arxml")
    model = asr.AutosarModel()
    model.load_file(arxml_path)
    am = abst.AutosarModelAbstraction(model)

    # 4a. Add self-test SWCs for FZC/RZC
    for ecu in ["FZC", "RZC"]:
        pkg = am.get_or_create_package("/Taktflow/SWCs/%s" % ecu)
        sn = "%s_Swc_%sSelfTest" % (ecu, ecu.capitalize())
        try:
            pkg.create_application_sw_component_type(sn)
        except Exception:
            pass
        iface_pkg = am.get_or_create_package("/Taktflow/Interfaces/Internal")
        sig = "%s_SIG_SELF_TEST_RESULT" % ecu
        try:
            sr = iface_pkg.create_sender_receiver_interface("SRI_%s" % sig)
            sr.create_data_element("Value")
        except Exception:
            pass
        elem = am.get_element_by_path("/Taktflow/SWCs/%s/%s" % (ecu, sn))
        if elem:
            swc = ApplicationSwComponentType(elem)
            for d, e in model.elements_dfs:
                if e.element_name == "SENDER-RECEIVER-INTERFACE" and e.item_name == "SRI_%s" % sig:
                    try:
                        swc.create_p_port("PP_%s" % sig, SenderReceiverInterface(e))
                    except Exception:
                        pass
                    break
    print("  Self-test SWCs: done")

    # 4b. Scan code for all Rte_Read/Rte_Write → build ports
    sr_cache = {}
    for d, e in model.elements_dfs:
        if e.element_name == "SENDER-RECEIVER-INTERFACE":
            try:
                sr_cache[e.item_name] = SenderReceiverInterface(e)
            except Exception:
                pass

    existing_ports = {}
    for d, e in model.elements_dfs:
        if e.element_name in ("P-PORT-PROTOTYPE", "R-PORT-PROTOTYPE"):
            try:
                parts = e.path.rsplit("/", 1)
                existing_ports.setdefault(parts[0], set()).add(parts[1])
            except Exception:
                pass

    # Build code→DBC mapping from alias chain in Cfg.h/App.h
    dbc_sigs = set(s.name for m in db.messages for s in m.signals)
    alias_chain = {}
    for ecu in ECUS:
        for f in ["include/%s_App.h" % ecu.capitalize(), "include/%s_Cfg.h" % ecu.capitalize()]:
            fpath = os.path.join(FIRMWARE_ROOT, ecu, f)
            if not os.path.isfile(fpath):
                continue
            with open(fpath) as fh:
                for line in fh:
                    m = re.match(r"#define\s+([A-Z_]+SIG_[A-Z0-9_]+)\s+([A-Z_]+SIG_[A-Z0-9_]+)", line)
                    if m:
                        alias_chain[m.group(1)] = m.group(2)

    def code_to_dbc(code_sig):
        """Map code signal name to DBC signal name."""
        target = alias_chain.get(code_sig)
        if target:
            for pfx in ["CVC_SIG_", "FZC_SIG_", "RZC_SIG_", "BCM_SIG_", "ICU_SIG_", "TCU_SIG_"]:
                if target.startswith(pfx):
                    rem = target[len(pfx):].lower().replace("_", "")
                    for dn in dbc_sigs:
                        if rem == dn.lower().replace("_", ""):
                            return dn
                    break
        # Direct match for already-qualified names
        for pfx in ["BCM_SIG_", "ICU_SIG_", "TCU_SIG_"]:
            if code_sig.startswith(pfx):
                rem = code_sig[len(pfx):].lower().replace("_", "")
                for dn in dbc_sigs:
                    if rem == dn.lower().replace("_", ""):
                        return dn
                break
        return None  # internal signal

    added_ports = 0

    for ecu in ECUS:
        ecu_upper = ecu.upper()
        src_dir = os.path.join(FIRMWARE_ROOT, ecu, "src")
        if not os.path.isdir(src_dir):
            continue

        for fname in sorted(os.listdir(src_dir)):
            if not fname.endswith(".c"):
                continue
            swc_name = get_swc_name(fname)
            if swc_name is None:
                continue

            arxml_name = "%s_%s" % (ecu_upper, swc_name)
            swc_path = "/Taktflow/SWCs/%s/%s" % (ecu_upper, arxml_name)

            raw = am.get_element_by_path(swc_path)
            if raw is None:
                continue

            swc = ApplicationSwComponentType(raw)
            ports_set = existing_ports.get(swc_path, set())

            writes, reads = scan_rte_calls(os.path.join(src_dir, fname))

            for sig in writes:
                dbc_name = code_to_dbc(sig)
                pname = "PP_%s" % (dbc_name if dbc_name else sig)
                iname = "SRI_%s" % (dbc_name if dbc_name else sig)
                if pname in ports_set:
                    continue
                sr = sr_cache.get(iname)
                if sr is None:
                    iface_pkg = am.get_or_create_package("/Taktflow/Interfaces/Internal")
                    try:
                        sr = iface_pkg.create_sender_receiver_interface(iname)
                        sr.create_data_element("Value")
                        sr_cache[iname] = sr
                    except Exception:
                        sr = sr_cache.get(iname)
                if sr:
                    try:
                        swc.create_p_port(pname, sr)
                        added_ports += 1
                        ports_set.add(pname)
                    except Exception:
                        pass

            for sig in reads:
                dbc_name = code_to_dbc(sig)
                rname = "RP_%s" % (dbc_name if dbc_name else sig)
                iname = "SRI_%s" % (dbc_name if dbc_name else sig)
                if rname in ports_set:
                    continue
                sr = sr_cache.get(iname)
                if sr is None:
                    iface_pkg = am.get_or_create_package("/Taktflow/Interfaces/Internal")
                    try:
                        sr = iface_pkg.create_sender_receiver_interface(iname)
                        sr.create_data_element("Value")
                        sr_cache[iname] = sr
                    except Exception:
                        sr = sr_cache.get(iname)
                if sr:
                    try:
                        swc.create_r_port(rname, sr)
                        added_ports += 1
                        ports_set.add(rname)
                    except Exception:
                        pass

    print("  Code ports injected: %d" % added_ports)

    # 4c. Save enriched ARXML
    am.write()
    print("  Saved: %s" % arxml_path)

    # 4d. Validate
    ref_errors = model.check_references()
    print("  Reference errors: %d" % len(ref_errors))
    if ref_errors:
        errors += 1

    # ----------------------------------------------------------------
    step("6. Re-copy enriched ARXML + re-run arxmlgen")
    # ----------------------------------------------------------------
    shutil.copy(arxml_path, ARXML_OUTPUT)
    print("  Copied enriched ARXML to production path")
    if not run("python3 -m tools.arxmlgen --config project.yaml"):
        print("  ERROR: arxmlgen re-run failed")
        errors += 1
    else:
        for ecu in ECUS:
            for f in os.listdir(os.path.join(FIRMWARE_ROOT, ecu, "src")):
                if re.match(r"Swc_[A-Z]{3}\.c$", f):
                    os.remove(os.path.join(FIRMWARE_ROOT, ecu, "src", f))
        print("  Final configs generated")

    # ----------------------------------------------------------------
    step("DONE")
    # ----------------------------------------------------------------
    if errors > 0:
        print("  %d errors — review above" % errors)
        return 1
    else:
        print("  Clean generation. DBC → ARXML → configs.")
        return 0


if __name__ == "__main__":
    sys.exit(main())
