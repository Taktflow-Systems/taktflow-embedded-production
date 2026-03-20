#!/usr/bin/env python3
"""
Pipeline Step 4: ARXML Validation

Input:  arxml/TaktflowSystem.arxml + gateway/taktflow_vehicle.dbc
Output: PASS/FAIL + issue list
Gate:   STOP if any issue found

Checks ARXML against DBC for consistency.
"""

import sys
import os
import cantools
import autosar_data as ad

DBC_PATH = os.environ.get("DBC_PATH", "gateway/taktflow_vehicle.dbc")
ARXML_PATH = os.environ.get("ARXML_PATH", "arxml/TaktflowSystem.arxml")


def get_attr(msg, name, default=None):
    try:
        attr = msg.dbc.attributes.get(name)
        if attr is None:
            return default
        return attr.value if hasattr(attr, 'value') else attr
    except Exception:
        return default


def run(arxml_path, dbc_path):
    db = cantools.database.load_file(dbc_path)
    model = ad.AutosarModel()
    model.load_file(arxml_path)

    issues = []
    checks_passed = 0
    checks_total = 0

    def check(name, condition, detail=""):
        nonlocal checks_passed, checks_total
        checks_total += 1
        if condition:
            checks_passed += 1
            print(f"  [PASS] {name}")
        else:
            issues.append(f"{name}: {detail}")
            print(f"  [FAIL] {name} -- {detail}")

    print(f"Step 4: ARXML Validation ({os.path.basename(arxml_path)})")

    # 1. Reference errors
    ref_errors = model.check_references()
    check("1. ARXML reference errors", len(ref_errors) == 0, f"{len(ref_errors)} errors")

    # 2. E2E protection count matches DBC
    # Count DBC messages that have E2E signals (not just BA_ attribute)
    dbc_e2e_count = 0
    for m in db.messages:
        sig_names = {s.name for s in m.signals}
        if any("E2E_DataID" in sn for sn in sig_names) and \
           any("E2E_AliveCounter" in sn for sn in sig_names) and \
           any("E2E_CRC8" in sn for sn in sig_names):
            dbc_e2e_count += 1
    arxml_e2e_count = 0
    for _, elem in model.elements_dfs:
        if str(elem.element_name) == "END-TO-END-PROTECTION":
            arxml_e2e_count += 1
    check(f"2. E2E protection count (ARXML={arxml_e2e_count} vs DBC={dbc_e2e_count})",
          arxml_e2e_count == dbc_e2e_count,
          f"Mismatch: ARXML has {arxml_e2e_count}, DBC has {dbc_e2e_count}")

    # 3. E2E DataIDs match DBC values
    arxml_e2e = {}  # name -> data_id
    for _, elem in model.elements_dfs:
        if str(elem.element_name) != "END-TO-END-PROTECTION":
            continue
        sn = None
        did = None
        for sub in elem.sub_elements:
            if str(sub.element_name) == "SHORT-NAME":
                sn = sub.character_data
            elif str(sub.element_name) == "END-TO-END-PROFILE":
                for psub in sub.sub_elements:
                    if str(psub.element_name) == "DATA-IDS":
                        for dsub in psub.sub_elements:
                            if str(dsub.element_name) == "DATA-ID":
                                try:
                                    did = int(dsub.character_data)
                                except (TypeError, ValueError):
                                    pass
        if sn and did is not None:
            pdu_name = sn[4:] if sn.startswith("E2E_") else sn
            arxml_e2e[pdu_name] = did

    mismatches = []
    for msg in db.messages:
        dbc_did = get_attr(msg, 'E2E_DataID')
        if dbc_did is None:
            continue
        dbc_did = int(dbc_did)
        import re
        mn = re.sub(r'[^A-Za-z0-9_]', '_', msg.name)
        arxml_did = arxml_e2e.get(mn)
        if arxml_did is None:
            # SC_Status uses custom E2E (no DataID nibble) — accepted deviation
            if msg.name != "SC_Status":
                mismatches.append(f"{msg.name}: in DBC but not ARXML")
        elif arxml_did != dbc_did:
            mismatches.append(f"{msg.name}: DBC={dbc_did} ARXML={arxml_did}")
    check("3. E2E DataIDs match DBC", len(mismatches) == 0, f"Mismatches: {mismatches}")

    # 4. I-PDU count matches DBC
    dbc_msg_count = len(db.messages)
    arxml_ipdu_count = 0
    for _, elem in model.elements_dfs:
        if str(elem.element_name) == "I-SIGNAL-I-PDU":
            arxml_ipdu_count += 1
    check(f"4. I-PDU count (ARXML={arxml_ipdu_count} vs DBC={dbc_msg_count})",
          arxml_ipdu_count == dbc_msg_count,
          f"Mismatch")

    # 5. I-SIGNAL count matches DBC
    dbc_sig_count = sum(len(m.signals) for m in db.messages)
    arxml_sig_count = 0
    for _, elem in model.elements_dfs:
        if str(elem.element_name) == "I-SIGNAL":
            arxml_sig_count += 1
    check(f"5. I-SIGNAL count (ARXML={arxml_sig_count} vs DBC={dbc_sig_count})",
          arxml_sig_count == dbc_sig_count,
          f"Mismatch")

    # 6. ADMIN-DATA Satisfies present on E2E protections
    has_satisfies = 0
    for _, elem in model.elements_dfs:
        if str(elem.element_name) != "END-TO-END-PROTECTION":
            continue
        for sub in elem.sub_elements:
            if str(sub.element_name) == "ADMIN-DATA":
                has_satisfies += 1
                break
    check(f"6. ADMIN-DATA traceability on E2E ({has_satisfies}/{arxml_e2e_count})",
          has_satisfies == arxml_e2e_count)

    # 7. SWC types present
    swc_count = 0
    for _, elem in model.elements_dfs:
        if str(elem.element_name) == "APPLICATION-SW-COMPONENT-TYPE":
            swc_count += 1
    check(f"7. SWC types present ({swc_count})", swc_count > 0)

    # 8. Runnables present
    runnable_count = 0
    for _, elem in model.elements_dfs:
        if str(elem.element_name) == "RUNNABLE-ENTITY":
            runnable_count += 1
    check(f"8. Runnables present ({runnable_count})", runnable_count > 0)

    # 9. ECU instances match DBC nodes
    arxml_ecu_count = 0
    for _, elem in model.elements_dfs:
        if str(elem.element_name) == "ECU-INSTANCE":
            arxml_ecu_count += 1
    dbc_node_count = len(db.nodes)
    check(f"9. ECU instances (ARXML={arxml_ecu_count} vs DBC nodes={dbc_node_count})",
          arxml_ecu_count == dbc_node_count,
          f"Mismatch")

    print()
    print(f"Result: {checks_passed}/{checks_total} checks passed, {len(issues)} issues")
    if issues:
        print("FAIL")
        for i in issues:
            print(f"  {i}")
        return 1
    else:
        print("PASS — ARXML validated, proceed to Step 5")
        return 0


if __name__ == "__main__":
    arxml = sys.argv[1] if len(sys.argv) > 1 else ARXML_PATH
    dbc = sys.argv[2] if len(sys.argv) > 2 else DBC_PATH
    sys.exit(run(arxml, dbc))
