#!/usr/bin/env python3
"""Minimal port injection — no error swallowing."""

import autosar_data as asr
import autosar_data.abstraction as abst
from autosar_data.abstraction.software_component import (
    ApplicationSwComponentType, SenderReceiverInterface,
)
import json

ARXML = "arxml_v2/TaktflowSystem.arxml"
PORTS = "arxml_v2/ports_model.json"

model = asr.AutosarModel()
model.load_file(ARXML)
am = abst.AutosarModelAbstraction(model)

with open(PORTS) as f:
    pm = json.load(f)

sr_cache = {}
for d, e in model.elements_dfs:
    if e.element_name == "SENDER-RECEIVER-INTERFACE":
        try:
            sr_cache[e.item_name] = SenderReceiverInterface(e)
        except Exception:
            pass

existing = {}
for d, e in model.elements_dfs:
    if e.element_name in ("P-PORT-PROTOTYPE", "R-PORT-PROTOTYPE"):
        try:
            parts = e.path.rsplit("/", 1)
            existing.setdefault(parts[0], set()).add(parts[1])
        except Exception:
            pass

added = 0
skipped = 0
no_sr = 0
errors = 0

for ecu, swcs in pm.items():
    for swc_name, data in swcs.items():
        arxml_name = "%s_%s" % (ecu, swc_name)
        swc_path = "/Taktflow/SWCs/%s/%s" % (ecu, arxml_name)

        raw = am.get_element_by_path(swc_path)
        if raw is None:
            continue

        swc = ApplicationSwComponentType(raw)
        ports_set = existing.get(swc_path, set())

        for direction in ("p", "r"):
            key = "p_ports" if direction == "p" else "r_ports"
            for port_info in data.get(key, []):
                pname = port_info["name"]
                iname = port_info["interface"]

                if pname in ports_set:
                    skipped += 1
                    continue

                sr = sr_cache.get(iname)
                if sr is None:
                    no_sr += 1
                    continue

                try:
                    if direction == "p":
                        swc.create_p_port(pname, sr)
                    else:
                        swc.create_r_port(pname, sr)
                    added += 1
                    ports_set.add(pname)
                except Exception as ex:
                    errors += 1
                    if errors <= 5:
                        print("ERROR: %s/%s: %s" % (arxml_name, pname, ex))

print("Added: %d" % added)
print("Skipped (existing): %d" % skipped)
print("No SR interface: %d" % no_sr)
print("Errors: %d" % errors)

am.write()
print("Saved: %s" % ARXML)
ref = model.check_references()
print("Reference errors: %d" % len(ref))
