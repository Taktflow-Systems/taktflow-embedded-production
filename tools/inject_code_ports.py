#!/usr/bin/env python3
"""
Inject code-scanned ports into existing ARXML SWC types.

Uses autosar_data.abstraction API (same as dbc2arxml.py).
Wraps existing SWC elements as ApplicationSwComponentType to get
create_p_port() and create_r_port() methods.

For internal signals, creates SenderReceiverInterface with uint32 data element.

Run AFTER dbc2arxml.py, BEFORE arxml_wiring.py.
"""

import autosar_data as asr
import autosar_data.abstraction as abst
from autosar_data.abstraction.software_component import (
    ApplicationSwComponentType,
    SenderReceiverInterface,
)
import json
import sys

ARXML_PATH = "arxml_v2/TaktflowSystem.arxml"
PORTS_MODEL_PATH = "arxml_v2/ports_model.json"


def main():
    model = asr.AutosarModel()
    model.load_file(ARXML_PATH)
    am = abst.AutosarModelAbstraction(model)

    with open(PORTS_MODEL_PATH) as f:
        ports_model = json.load(f)

    # Collect existing port names per SWC path
    existing_ports = {}
    for d, e in model.elements_dfs:
        if e.element_name in ("P-PORT-PROTOTYPE", "R-PORT-PROTOTYPE"):
            try:
                path = e.path
                parts = path.rsplit("/", 1)
                if len(parts) == 2:
                    swc_path = parts[0]
                    port_name = parts[1]
                    existing_ports.setdefault(swc_path, set()).add(port_name)
            except Exception:
                pass

    # Collect existing S/R interfaces by name
    sr_cache = {}
    for d, e in model.elements_dfs:
        if e.element_name == "SENDER-RECEIVER-INTERFACE":
            try:
                sr_cache[e.item_name] = SenderReceiverInterface(e)
            except Exception:
                pass

    # Get uint32 data type
    uint32_dt = None
    for d, e in model.elements_dfs:
        if e.element_name == "IMPLEMENTATION-DATA-TYPE":
            try:
                if "uint32" in e.item_name:
                    uint32_dt = e
                    break
            except Exception:
                pass

    added_ports = 0
    added_ifaces = 0
    skipped = 0

    for ecu_name, swcs in ports_model.items():
        for swc_name, port_data in swcs.items():
            arxml_name = "%s_%s" % (ecu_name, swc_name)
            swc_path = "/Taktflow/SWCs/%s/%s" % (ecu_name, arxml_name)

            # Get raw element and wrap as typed SWC
            raw_elem = am.get_element_by_path(swc_path)
            if raw_elem is None:
                continue

            try:
                swc = ApplicationSwComponentType(raw_elem)
            except Exception:
                continue

            existing = existing_ports.get(swc_path, set())

            for p in port_data.get("p_ports", []):
                if p["name"] in existing:
                    skipped += 1
                    continue

                sr = sr_cache.get(p["interface"])
                if sr is None:
                    # Create internal S/R interface
                    iface_pkg = am.get_or_create_package("/Taktflow/Interfaces/Internal")
                    try:
                        sr = iface_pkg.create_sender_receiver_interface(p["interface"])
                        de = sr.create_data_element("Value")
                        sr_cache[p["interface"]] = sr
                        added_ifaces += 1
                    except Exception:
                        sr = sr_cache.get(p["interface"])

                if sr is not None:
                    try:
                        swc.create_p_port(p["name"], sr)
                        added_ports += 1
                    except Exception:
                        pass

            for r in port_data.get("r_ports", []):
                if r["name"] in existing:
                    skipped += 1
                    continue

                sr = sr_cache.get(r["interface"])
                if sr is None:
                    iface_pkg = am.get_or_create_package("/Taktflow/Interfaces/Internal")
                    try:
                        sr = iface_pkg.create_sender_receiver_interface(r["interface"])
                        de = sr.create_data_element("Value")
                        sr_cache[r["interface"]] = sr
                        added_ifaces += 1
                    except Exception:
                        sr = sr_cache.get(r["interface"])

                if sr is not None:
                    try:
                        swc.create_r_port(r["name"], sr)
                        added_ports += 1
                    except Exception:
                        pass

    print("=== Port Injection ===")
    print("  Added ports: %d" % added_ports)
    print("  Added interfaces: %d" % added_ifaces)
    print("  Skipped (existing): %d" % skipped)

    # Save
    am.write()
    print("  Written: %s" % ARXML_PATH)

    # Validate
    ref_errors = model.check_references()
    print("  Reference errors: %d" % len(ref_errors))


if __name__ == "__main__":
    main()
