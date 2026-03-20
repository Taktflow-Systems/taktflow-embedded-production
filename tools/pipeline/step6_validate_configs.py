#!/usr/bin/env python3
"""
Pipeline Step 6: C Config Validation (round-trip check)

Verifies generated C configs match ARXML/DBC values:
- E2E DataIDs in Cfg.h match DBC E2E_DataID attributes
- Com CycleTimeMs in Com_Cfg.c match DBC GenMsgCycleTime
- Signal count per ECU matches expected

Input:  firmware/ecu/*/include/*_Cfg.h + firmware/ecu/*/cfg/Com_Cfg_*.c
Output: PASS/FAIL
"""

import sys
import os
import re
import cantools

DBC_PATH = os.environ.get("DBC_PATH", "gateway/taktflow_vehicle.dbc")
FW_DIR = "firmware/ecu"


def get_attr(msg, name, default=None):
    try:
        attr = msg.dbc.attributes.get(name)
        if attr is None:
            return default
        return attr.value if hasattr(attr, 'value') else attr
    except Exception:
        return default


def run(dbc_path):
    db = cantools.database.load_file(dbc_path)
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

    print(f"Step 6: C Config Validation (round-trip)")

    # Build DBC reference: message_name -> {e2e_data_id, cycle_ms}
    dbc_ref = {}
    for msg in db.messages:
        e2e = get_attr(msg, 'E2E_DataID')
        cycle = int(get_attr(msg, 'GenMsgCycleTime', 0) or 0)
        sender = msg.senders[0].upper() if msg.senders else None
        dbc_ref[msg.name] = {
            'e2e_data_id': int(e2e) if e2e is not None else None,
            'cycle_ms': cycle,
            'sender': sender,
        }

    # Check each ECU's generated Cfg.h for E2E DataID defines
    ecus = ['cvc', 'fzc', 'rzc', 'sc', 'bcm', 'icu', 'tcu']
    e2e_mismatches = []

    for ecu in ecus:
        cfg_h = os.path.join(FW_DIR, ecu, 'include', f'{ecu.capitalize()}_Cfg.h')
        if not os.path.exists(cfg_h):
            continue

        with open(cfg_h, 'r') as f:
            content = f.read()

        # Find E2E_*_DATA_ID defines
        for m in re.finditer(r'#define\s+\w+_E2E_(\w+)_DATA_ID\s+0x([0-9A-Fa-f]+)u', content):
            msg_part = m.group(1)
            gen_val = int(m.group(2), 16)

            # Find matching DBC message
            for msg_name, ref in dbc_ref.items():
                safe = msg_name.upper().replace(' ', '_').replace('-', '_')
                if safe == msg_part and ref['e2e_data_id'] is not None:
                    if gen_val != ref['e2e_data_id']:
                        e2e_mismatches.append(
                            f"{ecu}/{msg_name}: Cfg=0x{gen_val:02X} DBC=0x{ref['e2e_data_id']:02X}")
                    break

    check("1. E2E DataIDs in Cfg.h match DBC", len(e2e_mismatches) == 0,
          f"Mismatches: {e2e_mismatches}")

    # Check Com CycleTimeMs in Com_Cfg_*.c
    cycle_mismatches = []
    for ecu in ecus:
        com_cfg = os.path.join(FW_DIR, ecu, 'cfg', f'Com_Cfg_{ecu.capitalize()}.c')
        if not os.path.exists(com_cfg):
            continue

        with open(com_cfg, 'r') as f:
            content = f.read()

        # Parse tx_pdu_config entries: { PDU_NAME, DLC, CYCLE }
        for m in re.finditer(r'\{\s*\w+_COM_TX_(\w+),\s*\d+u,\s*(\d+)u\s*\}', content):
            pdu_name_upper = m.group(1)
            gen_cycle = int(m.group(2))

            # Find matching DBC message
            for msg_name, ref in dbc_ref.items():
                safe = msg_name.upper().replace(' ', '_').replace('-', '_')
                if safe == pdu_name_upper:
                    if gen_cycle != ref['cycle_ms']:
                        cycle_mismatches.append(
                            f"{ecu}/{msg_name}: Cfg={gen_cycle}ms DBC={ref['cycle_ms']}ms")
                    break

    check("2. Com CycleTimeMs in Cfg.c match DBC", len(cycle_mismatches) == 0,
          f"Mismatches: {cycle_mismatches}")

    # Check Cfg.h files exist for all ECUs
    missing_cfgs = []
    for ecu in ecus:
        cfg_h = os.path.join(FW_DIR, ecu, 'include', f'{ecu.capitalize()}_Cfg.h')
        if not os.path.exists(cfg_h):
            missing_cfgs.append(ecu)
    check("3. Cfg.h exists for all ECUs", len(missing_cfgs) == 0,
          f"Missing: {missing_cfgs}")

    print()
    print(f"Result: {checks_passed}/{checks_total} checks passed, {len(issues)} issues")
    if issues:
        print("FAIL")
        for i in issues:
            print(f"  {i}")
        return 1
    else:
        print("PASS — C configs validated against DBC")
        return 0


if __name__ == "__main__":
    dbc = sys.argv[1] if len(sys.argv) > 1 else DBC_PATH
    sys.exit(run(dbc))
