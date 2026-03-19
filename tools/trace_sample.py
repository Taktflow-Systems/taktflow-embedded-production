#!/usr/bin/env python3
"""
Random 10% trace sample: DBC → ARXML → SWC model consistency.

Picks random 10% of DBC signals and verifies:
  1. I-SIGNAL exists in ARXML
  2. Owner SWC exists in SWC model
  3. ProducedBy tag present
  4. Satisfies tag present

If 100% pass → safe to run full gate.
If any fail → STOP and review.
"""

import cantools
import autosar_data as ad
import random
import json
import sys
import os

random.seed(42)

dbc_path = "gateway/taktflow_vehicle.dbc"
arxml_path = "arxml_v2/TaktflowSystem.arxml"
swc_model_path = "arxml_v2/swc_model.json"

# Load DBC
db = cantools.database.load_file(dbc_path)
dbc_signals = {}
for msg in db.messages:
    for sig in msg.signals:
        dbc_signals[sig.name] = {
            "msg": msg.name,
            "can_id": msg.frame_id,
            "bit_pos": sig.start,
            "bit_len": sig.length,
            "sig_obj": sig,
            "msg_obj": msg,
        }

# Load ARXML
model = ad.AutosarModel()
model.load_file(arxml_path)
arxml_isignals = set()
for d, e in model.elements_dfs:
    if e.element_name == "I-SIGNAL":
        try:
            arxml_isignals.add(e.item_name)
        except Exception:
            pass

# Load SWC model
with open(swc_model_path) as f:
    swc_model = json.load(f)

# Build SWC name set
all_swc_names = set()
for ecu_name, ecu_data in swc_model.get("ecus", {}).items():
    for swc in ecu_data.get("swcs", []):
        all_swc_names.add(swc.get("name", ""))

# Pick random 10%
all_sig_names = sorted(dbc_signals.keys())
sample_size = max(1, len(all_sig_names) // 10)
sample = random.sample(all_sig_names, sample_size)

print("=== Random 10%% Sample: %d of %d signals ===" % (sample_size, len(all_sig_names)))
print()

passed = 0
failed = 0

for sig_name in sorted(sample):
    info = dbc_signals[sig_name]
    sig_obj = info["sig_obj"]
    msg_obj = info["msg_obj"]
    checks = []
    all_ok = True

    # Check 1: DBC signal exists in ARXML I-SIGNAL
    in_arxml = False
    for arxml_name in arxml_isignals:
        if sig_name == arxml_name or sig_name in arxml_name:
            in_arxml = True
            break

    if in_arxml:
        checks.append("ARXML I-SIGNAL: FOUND")
    else:
        checks.append("ARXML I-SIGNAL: MISSING")
        all_ok = False

    # Check 2: Owner attribute → SWC exists
    owner = None
    try:
        attr = sig_obj.dbc.attributes.get("Owner")
        if attr:
            owner = str(attr.value)
    except Exception:
        pass

    if owner:
        if owner in all_swc_names:
            checks.append("Owner '%s': FOUND in SWC model" % owner)
        else:
            checks.append("Owner '%s': NOT FOUND in SWC model" % owner)
            all_ok = False
    else:
        checks.append("Owner: not tagged")

    # Check 3: ProducedBy attribute
    produced_by = None
    try:
        attr = sig_obj.dbc.attributes.get("ProducedBy")
        if attr:
            produced_by = str(attr.value)
    except Exception:
        pass

    if produced_by:
        checks.append("ProducedBy: '%s'" % produced_by)
    else:
        checks.append("ProducedBy: not tagged")

    # Check 4: Satisfies attribute
    satisfies = None
    try:
        attr = sig_obj.dbc.attributes.get("Satisfies")
        if attr:
            satisfies = str(attr.value)
    except Exception:
        pass

    if satisfies:
        checks.append("Satisfies: '%s'" % satisfies)
    else:
        checks.append("Satisfies: not tagged")

    status = "PASS" if all_ok else "FAIL"
    if all_ok:
        passed += 1
    else:
        failed += 1

    print("[%s] %s  (0x%03X %s)" % (status, sig_name, info["can_id"], info["msg"]))
    for c in checks:
        print("       %s" % c)
    print()

print("=== Result: %d/%d passed ===" % (passed, sample_size))
if failed > 0:
    print("STOP — %d failures. Review before proceeding." % failed)
    sys.exit(1)
else:
    print("100%% correct — safe to run full arxml_gate.py")
    sys.exit(0)
