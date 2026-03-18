#!/usr/bin/env python3
"""
Inject code-scanned ports into ARXML SWC types.

Runs AFTER dbc2arxml.py. Reads the generated ARXML and adds
missing P-ports and R-ports from ports_model.json.

For internal signals (not in DBC), creates new S/R interfaces.
For DBC signals where the port already exists, skips.
"""

import autosar_data as ad
import json
import sys

ARXML_PATH = "arxml_v2/TaktflowSystem.arxml"
PORTS_MODEL_PATH = "arxml_v2/ports_model.json"


def main():
    model = ad.AutosarModel()
    model.load_file(ARXML_PATH)

    with open(PORTS_MODEL_PATH) as f:
        ports_model = json.load(f)

    # Collect existing SWC types
    swc_elements = {}
    for d, e in model.elements_dfs:
        if e.element_name == "APPLICATION-SW-COMPONENT-TYPE":
            try:
                swc_elements[e.item_name] = e
            except Exception:
                pass

    # Collect existing S/R interfaces
    sr_elements = {}
    for d, e in model.elements_dfs:
        if e.element_name == "SENDER-RECEIVER-INTERFACE":
            try:
                sr_elements[e.item_name] = e
            except Exception:
                pass

    # Collect existing port names per SWC
    swc_port_names = {}
    for d, e in model.elements_dfs:
        if e.element_name in ("P-PORT-PROTOTYPE", "R-PORT-PROTOTYPE"):
            try:
                path = e.path
                # Path: /Taktflow/SWCs/ECU/SWC_NAME/PORT_NAME
                parts = path.split("/")
                if len(parts) >= 5:
                    swc_name = parts[-2]
                    port_name = parts[-1]
                    swc_port_names.setdefault(swc_name, set()).add(port_name)
            except Exception:
                pass

    # Get or create Internal interfaces package
    iface_pkg = None
    for d, e in model.elements_dfs:
        if e.element_name == "AR-PACKAGE":
            try:
                if e.item_name == "Internal":
                    iface_pkg = e
                    break
            except Exception:
                pass

    if iface_pkg is None:
        # Create /Taktflow/Interfaces/Internal
        for d, e in model.elements_dfs:
            if e.element_name == "AR-PACKAGE":
                try:
                    if e.item_name == "Interfaces":
                        iface_pkg = e.create_sub_element("AR-PACKAGE")
                        iface_pkg.create_sub_element("SHORT-NAME").set_character_data("Internal")
                        break
                except Exception:
                    pass

    # Get uint32 data type for internal signals
    uint32_type = None
    for d, e in model.elements_dfs:
        if e.element_name == "IMPLEMENTATION-DATA-TYPE":
            try:
                if "uint32" in e.item_name:
                    uint32_type = e
                    break
            except Exception:
                pass

    added_ports = 0
    added_ifaces = 0
    skipped = 0

    for ecu_name, swcs in ports_model.items():
        for swc_name, port_data in swcs.items():
            arxml_swc_name = "%s_%s" % (ecu_name, swc_name)
            swc = swc_elements.get(arxml_swc_name)
            if swc is None:
                continue

            existing = swc_port_names.get(arxml_swc_name, set())

            all_ports = []
            for p in port_data.get("p_ports", []):
                all_ports.append(("P", p))
            for r in port_data.get("r_ports", []):
                all_ports.append(("R", r))

            for direction, port_info in all_ports:
                port_name = port_info["name"]
                iface_name = port_info["interface"]

                if port_name in existing:
                    skipped += 1
                    continue

                # Get or create S/R interface
                sr = sr_elements.get(iface_name)
                if sr is None and iface_pkg is not None:
                    try:
                        sr_elem = iface_pkg.create_sub_element("SENDER-RECEIVER-INTERFACE")
                        sr_elem.create_sub_element("SHORT-NAME").set_character_data(iface_name)
                        sr_elements[iface_name] = sr_elem
                        added_ifaces += 1
                    except Exception:
                        sr_elem = sr_elements.get(iface_name)
                    sr = sr_elements.get(iface_name)

                if sr is None:
                    continue

                # Create port
                try:
                    if direction == "P":
                        port = swc.create_p_port(port_name, sr)
                    else:
                        port = swc.create_r_port(port_name, sr)
                    added_ports += 1
                    existing.add(port_name)
                except Exception as ex:
                    pass  # Port might already exist under different name

    print("=== Port Injection ===")
    print("  Added ports: %d" % added_ports)
    print("  Added interfaces: %d" % added_ifaces)
    print("  Skipped (existing): %d" % skipped)

    # Write back
    model.write_file(ARXML_PATH)
    print("  Written: %s" % ARXML_PATH)

    # Validate
    ref_errors = model.check_references()
    print("  Reference errors: %d" % len(ref_errors))

    return 0 if ref_errors == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
