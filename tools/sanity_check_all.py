#!/usr/bin/env python3
"""
Full sanity check across all levels of the data model.

L2: DBC
L3: ARXML
L2→L3: DBC↔ARXML consistency
Connector map integrity
Code→DBC mapping completeness
"""

import cantools
import autosar_data as ad
import json
import os
import re
import sys

DBC_PATH = "gateway/taktflow_vehicle.dbc"
ARXML_PATH = "arxml_v2/TaktflowSystem.arxml"
WIRING_PATH = "arxml_v2/TaktflowWiring.arxml"
CONNECTOR_MAP_PATH = "arxml_v2/connector_map.json"
CODE_TO_DBC_PATH = "arxml_v2/code_to_dbc.json"
PORTS_MODEL_PATH = "arxml_v2/ports_model.json"
FIRMWARE_ROOT = "firmware/ecu"

passed = 0
failed = 0
warnings = 0


def check(name, condition, detail=""):
    global passed, failed
    if condition:
        print("  [PASS] %s" % name)
        passed += 1
    else:
        print("  [FAIL] %s — %s" % (name, detail))
        failed += 1


def warn(name, detail):
    global warnings
    print("  [WARN] %s — %s" % (name, detail))
    warnings += 1


def main():
    global passed, failed, warnings

    # ============================================================
    print("=" * 60)
    print("L2: DBC Checks")
    print("=" * 60)

    db = cantools.database.load_file(DBC_PATH)
    msgs = db.messages
    all_sigs = [s.name for m in msgs for s in m.signals]

    check("DBC loads without error", True)
    check("37 messages", len(msgs) == 37, "got %d" % len(msgs))
    check("174 signals", len(all_sigs) == 174, "got %d" % len(all_sigs))
    check("8 nodes", len(db.nodes) == 8, "got %d" % len(db.nodes))

    dupes = set(s for s in all_sigs if all_sigs.count(s) > 1)
    check("0 duplicate signal names", len(dupes) == 0, "dupes: %s" % dupes)

    no_tester = all(n.name not in ("Tester", "Plant_Sim") for n in db.nodes)
    check("No test-only nodes", no_tester)

    # Signal prefix check
    prefix_errors = 0
    for m in msgs:
        for s in m.signals:
            if not s.name.startswith(m.name + "_"):
                prefix_errors += 1
    check("All signals have full message prefix", prefix_errors == 0,
          "%d signals missing prefix" % prefix_errors)

    # Owner tag check
    tagged = 0
    untagged = 0
    for m in msgs:
        for s in m.signals:
            try:
                if s.dbc.attributes.get("Owner"):
                    tagged += 1
                else:
                    untagged += 1
            except Exception:
                untagged += 1
    check("All 174 signals have Owner tag", tagged == 174,
          "%d tagged, %d untagged" % (tagged, untagged))

    # GenSigStartValue check
    has_init = 0
    for m in msgs:
        for s in m.signals:
            try:
                if s.dbc.attributes.get("GenSigStartValue") is not None:
                    has_init += 1
            except Exception:
                pass
    check("All signals have GenSigStartValue", has_init == 174,
          "%d have it" % has_init)

    # Standard attributes on messages
    for m in msgs:
        has_cycle = False
        has_asil = False
        has_owner = False
        try:
            has_cycle = m.dbc.attributes.get("GenMsgCycleTime") is not None
            has_asil = m.dbc.attributes.get("ASIL") is not None
            has_owner = m.dbc.attributes.get("Owner") is not None
        except Exception:
            pass
        if not (has_cycle and has_asil and has_owner):
            warn("Message 0x%03X %s missing attrs" % (m.frame_id, m.name),
                 "cycle=%s asil=%s owner=%s" % (has_cycle, has_asil, has_owner))

    # ============================================================
    print()
    print("=" * 60)
    print("L3: ARXML Checks")
    print("=" * 60)

    model = ad.AutosarModel()
    model.load_file(ARXML_PATH)

    ref_errors = model.check_references()
    check("ARXML loads with 0 reference errors", len(ref_errors) == 0,
          "%d errors" % len(ref_errors))

    counts = {}
    for d, e in model.elements_dfs:
        n = e.element_name
        counts[n] = counts.get(n, 0) + 1

    isig_count = counts.get("I-SIGNAL", 0)
    ipdu_count = counts.get("I-SIGNAL-I-PDU", 0)
    ecu_count = counts.get("ECU-INSTANCE", 0)
    swc_count = counts.get("APPLICATION-SW-COMPONENT-TYPE", 0)
    run_count = counts.get("RUNNABLE-ENTITY", 0)
    pport_count = counts.get("P-PORT-PROTOTYPE", 0)
    rport_count = counts.get("R-PORT-PROTOTYPE", 0)
    sr_count = counts.get("SENDER-RECEIVER-INTERFACE", 0)

    check("I-SIGNAL count matches DBC", isig_count == 174,
          "ARXML=%d DBC=174" % isig_count)
    check("I-PDU count matches DBC", ipdu_count == 37,
          "ARXML=%d DBC=37" % ipdu_count)
    check("ECU count matches DBC", ecu_count == 8,
          "ARXML=%d DBC=8" % ecu_count)
    check("SWC types >= 48", swc_count >= 48,
          "got %d" % swc_count)
    check("Runnables > 0", run_count > 0,
          "got %d" % run_count)
    check("P-ports > 0", pport_count > 0,
          "got %d" % pport_count)
    check("R-ports > 0", rport_count > 0,
          "got %d" % rport_count)
    check("S/R interfaces > 158", sr_count >= 158,
          "got %d" % sr_count)

    print("  Info: %d P-ports, %d R-ports, %d S/R interfaces" % (
        pport_count, rport_count, sr_count))

    # ============================================================
    print()
    print("=" * 60)
    print("L2→L3: DBC↔ARXML Consistency")
    print("=" * 60)

    arxml_isigs = set()
    for d, e in model.elements_dfs:
        if e.element_name == "I-SIGNAL":
            try:
                arxml_isigs.add(e.item_name)
            except Exception:
                pass

    dbc_sig_names = set(s.name for m in msgs for s in m.signals)
    missing_in_arxml = dbc_sig_names - arxml_isigs
    extra_in_arxml = arxml_isigs - dbc_sig_names

    check("All DBC signals in ARXML", len(missing_in_arxml) == 0,
          "%d missing: %s" % (len(missing_in_arxml), list(missing_in_arxml)[:3]))
    if extra_in_arxml:
        warn("Extra I-SIGNALs in ARXML", "%d (SIL overlay?)" % len(extra_in_arxml))

    # ============================================================
    print()
    print("=" * 60)
    print("Connector Map Checks")
    print("=" * 60)

    if not os.path.isfile(CONNECTOR_MAP_PATH):
        warn("Connector map not found", "run build_connectors.py first")
        cmap = {}
    else:
        with open(CONNECTOR_MAP_PATH) as f:
            cmap = json.load(f)

    total_conns = sum(len(v) for v in cmap.values())
    check("Connector map has 92 connections", total_conns == 92,
          "got %d" % total_conns)

    # Verify 10% sample against code
    import random
    random.seed(42)
    all_conns = []
    for ecu, conns in cmap.items():
        all_conns.extend(conns)
    if not all_conns:
        warn("No connectors to sample", "run build_connectors.py")
        sample = []
    else:
        sample = random.sample(all_conns, max(1, len(all_conns) // 10))

    sample_pass = 0
    sample_fail = 0
    for c in sample:
        sig = c["signal"]
        prov = c["provider"]
        reqr = c["requester"]
        ecu = c["ecu"].lower()

        w_path = os.path.join(FIRMWARE_ROOT, ecu, "src", "%s.c" % prov)
        r_path = os.path.join(FIRMWARE_ROOT, ecu, "src", "%s.c" % reqr)

        w_ok = False
        r_ok = False
        if os.path.isfile(w_path):
            with open(w_path, errors="replace") as f:
                w_ok = ("Rte_Write(%s" % sig) in f.read()
        if os.path.isfile(r_path):
            with open(r_path, errors="replace") as f:
                r_ok = ("Rte_Read(%s" % sig) in f.read()

        if w_ok and r_ok:
            sample_pass += 1
        else:
            sample_fail += 1

    check("Connector 10%% sample verified against code",
          sample_fail == 0,
          "%d/%d passed" % (sample_pass, len(sample)))

    # ============================================================
    print()
    print("=" * 60)
    print("Code→DBC Mapping Checks")
    print("=" * 60)

    if not os.path.isfile(CODE_TO_DBC_PATH):
        warn("Code→DBC mapping not found", "run map_code_to_dbc.py first")
        c2d = {}
    else:
        with open(CODE_TO_DBC_PATH) as f:
            c2d = json.load(f)

    total_mapped = sum(1 for v in c2d.values() if v not in ("INTERNAL", "UNMAPPED"))
    total_internal = sum(1 for v in c2d.values() if v == "INTERNAL")
    total_unmapped = sum(1 for v in c2d.values() if v == "UNMAPPED")

    check("0 UNMAPPED signals", total_unmapped == 0,
          "%d unmapped" % total_unmapped)

    print("  Info: %d mapped to DBC, %d INTERNAL, %d UNMAPPED" % (
        total_mapped, total_internal, total_unmapped))

    # Verify mapped signals exist in DBC
    bad_map = 0
    for code_sig, dbc_name in c2d.items():
        if dbc_name not in ("INTERNAL", "UNMAPPED"):
            if dbc_name not in dbc_sig_names:
                bad_map += 1
                if bad_map <= 3:
                    warn("Mapped signal not in DBC", "%s → %s" % (code_sig, dbc_name))
    check("All mapped signals exist in DBC", bad_map == 0,
          "%d bad mappings" % bad_map)

    # ============================================================
    print()
    print("=" * 60)
    print("Wiring ARXML Checks")
    print("=" * 60)

    if os.path.isfile(WIRING_PATH):
        wmodel = ad.AutosarModel()
        wmodel.load_file(WIRING_PATH)
        wcounts = {}
        for d, e in wmodel.elements_dfs:
            n = e.element_name
            wcounts[n] = wcounts.get(n, 0) + 1

        asm = wcounts.get("ASSEMBLY-SW-CONNECTOR", 0)
        comp = wcounts.get("COMPOSITION-SW-COMPONENT-TYPE", 0)
        proto = wcounts.get("SW-COMPONENT-PROTOTYPE", 0)
        ecumaps = wcounts.get("SWC-TO-ECU-MAPPING", 0)

        check("Assembly connectors >= 90", asm >= 90,
              "got %d" % asm)
        check("Compositions >= 6", comp >= 6,
              "got %d" % comp)
        check("SWC prototypes >= 48", proto >= 48,
              "got %d" % proto)
        check("ECU mappings >= 48", ecumaps >= 48,
              "got %d" % ecumaps)

        wref = wmodel.check_references()
        # Wiring file references main ARXML — cross-file refs expected
        if wref:
            warn("Wiring ARXML reference errors", "%d (expected: cross-file refs)" % len(wref))
    else:
        warn("Wiring ARXML not found", WIRING_PATH)

    # ============================================================
    print()
    print("=" * 60)
    print("SUMMARY")
    print("=" * 60)
    print("  Passed:   %d" % passed)
    print("  Failed:   %d" % failed)
    print("  Warnings: %d" % warnings)

    if failed > 0:
        print("\n  RESULT: FAIL — %d checks failed" % failed)
        return 1
    elif warnings > 0:
        print("\n  RESULT: PASS with %d warnings" % warnings)
        return 0
    else:
        print("\n  RESULT: PASS — all checks green")
        return 0


if __name__ == "__main__":
    sys.exit(main())
