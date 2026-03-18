#!/usr/bin/env python3
"""
ARXML Wiring Tool — adds compositions, connectors, and mappings.

Reads:
  - arxml_v2/TaktflowSystem.arxml (SWCs, ports, S/R interfaces)
  - arxml_v2/connector_map.json (92 intra-ECU connections)
  - arxml_v2/swc_model.json (ECU → SWC assignment)

Adds:
  1. COMPOSITION-SW-COMPONENT-TYPE per ECU (7)
  2. SW-COMPONENT-PROTOTYPE per SWC in composition
  3. ASSEMBLY-SW-CONNECTOR per intra-ECU connection (92)
  4. SYSTEM-MAPPING with SWC-TO-ECU-MAPPING (48)
  5. ROOT-SW-COMPOSITION-PROTOTYPE

Reference: patrikja/autosar EcuExtract.arxml

Usage:
    python3 tools/arxml_wiring.py
"""

import autosar_data as ad
import json
import sys

ARXML_PATH = "arxml_v2/TaktflowSystem.arxml"
CONNECTOR_MAP_PATH = "arxml_v2/connector_map.json"
SWC_MODEL_PATH = "arxml_v2/swc_model.json"
OUTPUT_PATH = "arxml_v2/TaktflowSystem.arxml"

ECU_NAMES = ["CVC", "FZC", "RZC", "SC", "BCM", "ICU", "TCU"]


def find_or_create_package(model, file, parent_path, name):
    """Find or create an AR-PACKAGE at the given path."""
    full_path = "%s/%s" % (parent_path, name)
    for depth, elem in model.elements_dfs:
        if elem.element_name == "AR-PACKAGE":
            try:
                if elem.item_name == name:
                    # Check if parent matches
                    path = elem.path
                    if path and path.endswith("/%s" % name):
                        return elem
            except Exception:
                pass
    return None


def main():
    # Load model
    model = ad.AutosarModel()
    file_result = model.load_file(ARXML_PATH)

    # Load connector map
    with open(CONNECTOR_MAP_PATH) as f:
        connector_map = json.load(f)

    # Load SWC model for ECU→SWC assignment
    with open(SWC_MODEL_PATH) as f:
        swc_model = json.load(f)

    # Get the file reference
    arxml_file = model.files[0] if model.files else None
    if arxml_file is None:
        print("ERROR: no ARXML file loaded")
        return 1

    # Load code→DBC signal name mapping
    code_to_dbc_path = "arxml_v2/code_to_dbc.json"
    with open(code_to_dbc_path) as f:
        code_to_dbc = json.load(f)

    # Load ports model (all ports from code scan)
    ports_model_path = "arxml_v2/ports_model.json"
    with open(ports_model_path) as f:
        ports_model = json.load(f)

    # Collect existing elements
    swc_types = {}  # name → path
    sr_interfaces = {}  # name → path
    ecu_instances = {}  # name → path
    port_paths = {}  # (swc_arxml_name, port_short_name) → full_path

    for depth, elem in model.elements_dfs:
        en = elem.element_name
        try:
            name = elem.item_name
            path = elem.path if hasattr(elem, 'path') else ""
        except Exception:
            continue

        if en == "APPLICATION-SW-COMPONENT-TYPE":
            swc_types[name] = path
        elif en == "SENDER-RECEIVER-INTERFACE":
            sr_interfaces[name] = path
        elif en == "ECU-INSTANCE":
            ecu_instances[name] = path
        elif en in ("P-PORT-PROTOTYPE", "R-PORT-PROTOTYPE"):
            port_paths[path] = name

    print("Existing: %d SWC types, %d S/R interfaces, %d ECU instances" % (
        len(swc_types), len(sr_interfaces), len(ecu_instances)))

    # Build ECU → SWC list from swc_model
    ecu_swcs = {}
    for ecu_name in ECU_NAMES:
        ecu_lower = ecu_name.lower()
        ecu_data = swc_model.get("ecus", {}).get(ecu_lower, {})
        swc_list = [s["name"] for s in ecu_data.get("swcs", [])]
        ecu_swcs[ecu_name] = swc_list

    # SC has no SWCs in our model (hand-written, not extracted)
    if not ecu_swcs.get("SC"):
        ecu_swcs["SC"] = []

    # Stats
    total_assembly = 0
    total_swc_protos = 0

    # Find the Compositions package (create path)
    # We'll build the XML manually since autosar_data API is complex
    # Instead, output the wiring as a separate ARXML file that can be merged

    print()
    print("=== Generating Wiring ARXML ===")

    lines = []
    lines.append('<?xml version="1.0" encoding="utf-8"?>')
    lines.append('<AUTOSAR xmlns="http://autosar.org/schema/r4.0" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://autosar.org/schema/r4.0 AUTOSAR_00054.xsd">')
    lines.append('  <AR-PACKAGES>')
    lines.append('    <AR-PACKAGE>')
    lines.append('      <SHORT-NAME>Compositions</SHORT-NAME>')
    lines.append('      <ELEMENTS>')

    # Generate one COMPOSITION per ECU
    for ecu_name in ECU_NAMES:
        swc_list = ecu_swcs.get(ecu_name, [])
        connectors = connector_map.get(ecu_name, [])

        if not swc_list:
            print("  %s: 0 SWCs, skipping composition" % ecu_name)
            continue

        comp_name = "%s_Composition" % ecu_name
        print("  %s: %d SWCs, %d connectors" % (ecu_name, len(swc_list), len(connectors)))

        lines.append('        <COMPOSITION-SW-COMPONENT-TYPE>')
        lines.append('          <SHORT-NAME>%s</SHORT-NAME>' % comp_name)

        # COMPONENTS (SWC prototypes)
        # ARXML SWC names are prefixed: CVC_Swc_EStop, not Swc_EStop
        lines.append('          <COMPONENTS>')
        for swc_name in sorted(swc_list):
            arxml_name = "%s_%s" % (ecu_name, swc_name)
            swc_path = swc_types.get(arxml_name)
            if swc_path:
                lines.append('            <SW-COMPONENT-PROTOTYPE>')
                lines.append('              <SHORT-NAME>%s</SHORT-NAME>' % swc_name)
                lines.append('              <TYPE-TREF DEST="APPLICATION-SW-COMPONENT-TYPE">%s</TYPE-TREF>' % swc_path)
                lines.append('            </SW-COMPONENT-PROTOTYPE>')
                total_swc_protos += 1
            else:
                lines.append('            <!-- WARNING: SWC type not found: %s (tried %s) -->' % (swc_name, arxml_name))
        lines.append('          </COMPONENTS>')

        # CONNECTORS (assembly)
        if connectors:
            lines.append('          <CONNECTORS>')
            for conn in connectors:
                provider = conn["provider"]
                requester = conn["requester"]
                signal = conn["signal"]

                # Connector name
                conn_name = "%s_%s_to_%s" % (provider, signal, requester)
                # Truncate if too long
                if len(conn_name) > 80:
                    conn_name = conn_name[:80]

                provider_arxml = "%s_%s" % (ecu_name, provider)
                requester_arxml = "%s_%s" % (ecu_name, requester)
                provider_path = swc_types.get(provider_arxml, "")
                requester_path = swc_types.get(requester_arxml, "")

                if not provider_path or not requester_path:
                    lines.append('            <!-- WARNING: missing SWC type for connector %s → %s -->' % (provider, requester))
                    continue

                # Map code signal name to DBC name for port lookup
                dbc_signal = code_to_dbc.get(signal, "")
                if dbc_signal in ("INTERNAL", "UNMAPPED", ""):
                    # Internal signal — port name uses signal directly
                    p_port_name = "PP_%s" % signal
                    r_port_name = "RP_%s" % signal
                else:
                    # CAN signal — port name uses DBC qualified name
                    p_port_name = "PP_%s" % dbc_signal
                    r_port_name = "RP_%s" % dbc_signal

                # Verify ports exist in ARXML
                p_port_path = "%s/%s" % (provider_path, p_port_name)
                r_port_path = "%s/%s" % (requester_path, r_port_name)

                p_exists = p_port_path in port_paths
                r_exists = r_port_path in port_paths

                if not p_exists or not r_exists:
                    lines.append('            <!-- WARNING: port not found: P=%s(%s) R=%s(%s) -->' % (
                        p_port_name, "OK" if p_exists else "MISSING",
                        r_port_name, "OK" if r_exists else "MISSING"))
                    continue

                lines.append('            <ASSEMBLY-SW-CONNECTOR>')
                lines.append('              <SHORT-NAME>%s</SHORT-NAME>' % conn_name)
                lines.append('              <PROVIDER-IREF>')
                lines.append('                <CONTEXT-COMPONENT-REF DEST="SW-COMPONENT-PROTOTYPE">/Compositions/%s/%s</CONTEXT-COMPONENT-REF>' % (comp_name, provider))
                lines.append('                <TARGET-P-PORT-REF DEST="P-PORT-PROTOTYPE">%s</TARGET-P-PORT-REF>' % p_port_path)
                lines.append('              </PROVIDER-IREF>')
                lines.append('              <REQUESTER-IREF>')
                lines.append('                <CONTEXT-COMPONENT-REF DEST="SW-COMPONENT-PROTOTYPE">/Compositions/%s/%s</CONTEXT-COMPONENT-REF>' % (comp_name, requester))
                lines.append('                <TARGET-R-PORT-REF DEST="R-PORT-PROTOTYPE">%s</TARGET-R-PORT-REF>' % r_port_path)
                lines.append('              </REQUESTER-IREF>')
                lines.append('            </ASSEMBLY-SW-CONNECTOR>')
                total_assembly += 1

            lines.append('          </CONNECTORS>')

        lines.append('        </COMPOSITION-SW-COMPONENT-TYPE>')

    lines.append('      </ELEMENTS>')
    lines.append('    </AR-PACKAGE>')

    # SYSTEM-MAPPING package
    lines.append('    <AR-PACKAGE>')
    lines.append('      <SHORT-NAME>SystemMapping</SHORT-NAME>')
    lines.append('      <ELEMENTS>')
    lines.append('        <SYSTEM>')
    lines.append('          <SHORT-NAME>TaktflowSystem</SHORT-NAME>')
    lines.append('          <MAPPINGS>')
    lines.append('            <SYSTEM-MAPPING>')
    lines.append('              <SHORT-NAME>EcuMapping</SHORT-NAME>')
    lines.append('              <SW-MAPPINGS>')

    for ecu_name in ECU_NAMES:
        swc_list = ecu_swcs.get(ecu_name, [])
        ecu_path = ecu_instances.get(ecu_name, "")
        comp_name = "%s_Composition" % ecu_name

        for swc_name in sorted(swc_list):
            lines.append('                <SWC-TO-ECU-MAPPING>')
            lines.append('                  <SHORT-NAME>%s_to_%s</SHORT-NAME>' % (swc_name, ecu_name))
            lines.append('                  <COMPONENT-IREFS>')
            lines.append('                    <COMPONENT-IREF>')
            lines.append('                      <TARGET-COMPONENT-REF DEST="SW-COMPONENT-PROTOTYPE">/Compositions/%s/%s</TARGET-COMPONENT-REF>' % (comp_name, swc_name))
            lines.append('                    </COMPONENT-IREF>')
            lines.append('                  </COMPONENT-IREFS>')
            lines.append('                  <ECU-INSTANCE-REF DEST="ECU-INSTANCE">%s</ECU-INSTANCE-REF>' % ecu_path)
            lines.append('                </SWC-TO-ECU-MAPPING>')

    lines.append('              </SW-MAPPINGS>')
    lines.append('            </SYSTEM-MAPPING>')
    lines.append('          </MAPPINGS>')
    lines.append('        </SYSTEM>')
    lines.append('      </ELEMENTS>')
    lines.append('    </AR-PACKAGE>')

    lines.append('  </AR-PACKAGES>')
    lines.append('</AUTOSAR>')

    # Write wiring file
    wiring_path = "arxml_v2/TaktflowWiring.arxml"
    with open(wiring_path, "w") as f:
        f.write("\n".join(lines))

    print()
    print("=== Summary ===")
    print("  Compositions: %d" % sum(1 for e in ECU_NAMES if ecu_swcs.get(e)))
    print("  SWC prototypes: %d" % total_swc_protos)
    print("  Assembly connectors: %d" % total_assembly)
    print("  ECU mappings: %d" % sum(len(v) for v in ecu_swcs.values()))
    print()
    print("  Written: %s" % wiring_path)

    # Validate the wiring file
    print()
    print("=== Validating wiring ARXML ===")
    wiring_model = ad.AutosarModel()
    try:
        wiring_model.load_file(wiring_path)
        ref_errors = wiring_model.check_references()
        print("  Load: OK")
        print("  Reference errors: %d" % len(ref_errors))
        if ref_errors:
            for e in ref_errors[:5]:
                print("    %s" % str(e))
    except Exception as ex:
        print("  Load FAILED: %s" % str(ex))

    return 0


if __name__ == "__main__":
    sys.exit(main())
