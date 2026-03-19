#!/usr/bin/env python3
"""ARXML structural audit using autosar-data library."""

import autosar_data as ad
import sys

model = ad.AutosarModel()
result = model.load_file("arxml/TaktflowSystem.arxml")
print("Loaded OK")

# 1. Reference integrity
ref_errors = model.check_references()
print("Reference errors: %d" % len(ref_errors))
for e in ref_errors[:10]:
    print("  %s" % str(e))

# 2. Count elements by type
counts = {}
for (depth, elem) in model.elements_dfs:
    n = elem.element_name
    counts[n] = counts.get(n, 0) + 1

# Key AUTOSAR elements
key_types = [
    "APPLICATION-SW-COMPONENT-TYPE",
    "SENDER-RECEIVER-INTERFACE",
    "CLIENT-SERVER-INTERFACE",
    "R-PORT-PROTOTYPE",
    "P-PORT-PROTOTYPE",
    "RUNNABLE-ENTITY",
    "I-SIGNAL",
    "I-SIGNAL-I-PDU",
    "SYSTEM",
    "SYSTEM-MAPPING",
    "ECU-INSTANCE",
    "COMPOSITION-SW-COMPONENT-TYPE",
    "SW-COMPONENT-PROTOTYPE",
    "CONNECTOR",
    "DELEGATION-SW-CONNECTOR",
    "ASSEMBLY-SW-CONNECTOR",
]

print("\n=== ARXML Element Counts ===")
for t in key_types:
    c = counts.get(t, 0)
    status = "OK" if c > 0 else "MISSING"
    print("  %-40s %4d  %s" % (t, c, status))

# 3. List SWC names
print("\n=== SWC Names ===")
for (depth, elem) in model.elements_dfs:
    if elem.element_name == "APPLICATION-SW-COMPONENT-TYPE":
        try:
            print("  %s" % elem.item_name)
        except Exception:
            pass

# 4. List S/R interfaces
print("\n=== Sender-Receiver Interfaces ===")
sr_count = 0
for (depth, elem) in model.elements_dfs:
    if elem.element_name == "SENDER-RECEIVER-INTERFACE":
        try:
            print("  %s" % elem.item_name)
            sr_count += 1
        except Exception:
            pass
        if sr_count >= 20:
            print("  ... (truncated)")
            break

# 5. Check for port connections (assembly connectors)
assembly_count = counts.get("ASSEMBLY-SW-CONNECTOR", 0)
delegation_count = counts.get("DELEGATION-SW-CONNECTOR", 0)
print("\n=== Port Connections ===")
print("  Assembly connectors:   %d" % assembly_count)
print("  Delegation connectors: %d" % delegation_count)
if assembly_count == 0:
    print("  WARNING: No assembly connectors — SWC ports are not connected!")

# Summary
print("\n=== Summary ===")
total = sum(counts.values())
print("  Total elements: %d" % total)
if ref_errors:
    print("  FAIL: %d broken references" % len(ref_errors))
    sys.exit(1)
else:
    print("  PASS: All references valid")
    if assembly_count == 0:
        print("  WARNING: No port connections defined")
