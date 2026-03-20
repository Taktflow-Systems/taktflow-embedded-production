#!/usr/bin/env python3
"""
Pipeline Gate Test — Error Injection

Intentionally breaks the DBC in various ways and verifies
that Step 1 catches each error. Restores DBC after each test.

Usage: python tools/pipeline/test_gates.py
"""

import sys
import os
import shutil
import tempfile

# Import step1 directly
sys.path.insert(0, os.path.dirname(__file__))
from step1_validate_dbc import run as validate_dbc

DBC_PATH = "gateway/taktflow_vehicle.dbc"


def inject_and_test(name, transform_fn, expected_fail_check):
    """Copy DBC, apply transform, run validation, verify failure."""
    with tempfile.NamedTemporaryFile(suffix='.dbc', delete=False, mode='w') as tmp:
        with open(DBC_PATH, 'r') as orig:
            content = orig.read()
        modified = transform_fn(content)
        tmp.write(modified)
        tmp_path = tmp.name

    try:
        print(f"\n--- INJECT: {name} ---")
        result = run_quiet(tmp_path)
        if result != 0:
            print(f"  GATE CAUGHT IT -> PASS")
            return True
        else:
            print(f"  GATE MISSED IT -> FAIL (expected step1 to fail)")
            return False
    finally:
        os.unlink(tmp_path)


def run_quiet(dbc_path):
    """Run step1 but suppress output."""
    import io
    import contextlib
    f = io.StringIO()
    with contextlib.redirect_stdout(f):
        try:
            return validate_dbc(dbc_path)
        except SystemExit as e:
            return e.code or 0


def main():
    print("Pipeline Gate Test — Error Injection")
    print("=" * 50)

    tests = []

    # Test 1: Duplicate E2E DataID
    def dup_e2e(content):
        # Change SC_Status DataID from 0 to 1 (duplicates EStop_Broadcast)
        return content.replace(
            'BA_ "E2E_DataID" BO_ 19 0;',
            'BA_ "E2E_DataID" BO_ 19 1;'
        )
    tests.append(("Duplicate E2E DataID", dup_e2e, "check 1"))

    # Test 2: E2E DataID > 15
    def overflow_e2e(content):
        return content.replace(
            'BA_ "E2E_DataID" BO_ 19 0;',
            'BA_ "E2E_DataID" BO_ 19 20;'
        )
    tests.append(("E2E DataID > 15", overflow_e2e, "check 2"))

    # Test 3: FTTI exceeded (change Vehicle_State cycle to 100ms)
    def ftti_exceed(content):
        return content.replace(
            'BA_ "GenMsgCycleTime" BO_ 256 10;',
            'BA_ "GenMsgCycleTime" BO_ 256 100;'
        )
    tests.append(("FTTI exceeded (Vehicle_State 100ms)", ftti_exceed, "check 5"))

    # Test 4: Signal overlap (change DTC Number back to 24 bits)
    def sig_overlap(content):
        return content.replace(
            'SG_ DTC_Broadcast_Number : 7|16@0+',
            'SG_ DTC_Broadcast_Number : 7|24@0+'
        )
    tests.append(("Signal overlap (DTC 24-bit)", sig_overlap, "check 7"))

    # Test 5: Remove Satisfies (delete all Satisfies lines)
    def no_satisfies(content):
        lines = content.split('\n')
        return '\n'.join(l for l in lines if '"Satisfies"' not in l)
    tests.append(("No Satisfies traceability", no_satisfies, "check 12"))

    passed = 0
    failed = 0
    for name, transform, expected in tests:
        if inject_and_test(name, transform, expected):
            passed += 1
        else:
            failed += 1

    # Also test: clean DBC should PASS
    print(f"\n--- CLEAN DBC (should pass) ---")
    result = run_quiet(DBC_PATH)
    if result == 0:
        print(f"  Clean DBC passes -> PASS")
        passed += 1
    else:
        print(f"  Clean DBC fails -> FAIL (unexpected)")
        failed += 1

    print(f"\n{'=' * 50}")
    print(f"Gate tests: {passed} passed, {failed} failed out of {passed + failed}")
    if failed == 0:
        print("ALL GATES VERIFIED")
    else:
        print(f"{failed} GATES MISSED ERRORS")
    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
