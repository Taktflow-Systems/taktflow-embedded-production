#!/usr/bin/env python3
"""
Add @verifies traceability tags to test files.

Maps test scenarios to requirements based on what they test.
Idempotent: skips files that already have the tag.

Usage: python tools/trace/add_verifies_tags.py
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# SIL scenario -> requirement mapping (what each scenario verifies)
SIL_MAP = {
    'sil_001_normal_startup.yaml':       'SG-001, SG-002, SG-003, SG-004, SG-006, SG-007, SG-008, TSR-001, TSR-005, TSR-006, TSR-008, TSR-010, TSR-012, TSR-015, TSR-022, TSR-023, TSR-024, TSR-030, TSR-031, TSR-032, TSR-033, TSR-035, TSR-038, TSR-039, TSR-040, TSR-046, TSR-047, TSR-050',
    'sil_002_pedal_ramp.yaml':           'SG-001, TSR-001, SSR-CVC-001, SSR-CVC-002',
    'sil_003_emergency_stop.yaml':       'SG-008, TSR-046, SSR-CVC-029',
    'sil_004_can_busoff_fzc.yaml':       'SG-003, SG-004, TSR-022, SSR-CVC-016, SSR-BSW-026',
    'sil_005_watchdog_timeout_cvc.yaml':  'SG-008, TSR-048, SSR-SC-001',
    'sil_006_battery_undervoltage.yaml': 'SG-006, TSR-038, SSR-RZC-008',
    'sil_007_overcurrent_motor.yaml':    'SG-001, TSR-023, SSR-RZC-001, SSR-RZC-003',
    'sil_008_sensor_disagreement.yaml':  'SG-003, TSR-024, SSR-FZC-001',
    'sil_009_e2e_corruption.yaml':       'SG-008, TSR-022, SWR-BSW-023, SWR-BSW-024',
    'sil_010_overtemp_motor.yaml':       'SG-002, TSR-023, SSR-RZC-005',
    'sil_011_steering_sensor_failure.yaml': 'SG-003, TSR-024, SSR-FZC-015',
    'sil_012_multiple_faults.yaml':      'SG-001, SG-003, SG-004, SG-008',
    'sil_013_recovery_from_safe.yaml':   'SG-008, TSR-047',
    'sil_014_long_duration.yaml':        'SG-001, SG-008, TSR-046',
    'sil_015_power_cycle.yaml':          'SG-008, TSR-048',
    'sil_017_gateway_ml_anomaly.yaml':   'SG-007, TSR-039',
}

# Unit test -> requirement mapping
UNIT_MAP = {
    'test_Com_asild.c':       'SWR-BSW-015, SWR-BSW-016, SWR-BSW-023, SWR-BSW-024, SWR-BSW-025, SWR-BSW-031, SWR-BSW-050',
    'test_CanSM_asild.c':     'SWR-BSW-026, SSR-CVC-016, SSR-FZC-019',
    'test_E2E_SM_asild.c':    'SWR-BSW-023, SWR-BSW-024, TSR-022, TSR-023',
    'test_Can_asild.c':       'SWR-BSW-009, SWR-BSW-010',
    'test_Dem_asild.c':       'SWR-BSW-017, SWR-BSW-018',
    'test_Det_asild.c':       'SWR-BSW-019',
    'test_E2E_asild.c':       'SWR-BSW-023, SWR-BSW-024, SWR-BSW-025',
    'test_WdgM_asild.c':      'SWR-BSW-020, SWR-BSW-021, SWR-BSW-022',
    'test_BswM_asild.c':      'SWR-BSW-028',
    'test_Adc_asila.c':       'SWR-BSW-001',
    'test_Pwm_asild.c':       'SWR-BSW-003',
    'test_Spi_asild.c':       'SWR-BSW-005',
    'test_CanIf_asild.c':     'SWR-BSW-011, SWR-BSW-012',
    'test_PduR_asild.c':      'SWR-BSW-013',
    'test_Rte_asild.c':       'SWR-BSW-014',
    'test_IoHwAb_asild.c':    'SWR-BSW-007, SWR-BSW-008',
    # ECU-specific tests
    'test_Swc_Pedal_asild.c':           'SSR-CVC-001, SSR-CVC-002, SSR-CVC-003, SSR-CVC-008, SSR-CVC-009, SWR-CVC-001, SWR-CVC-002, SWR-CVC-003, SWR-CVC-008, SWR-CVC-009',
    'test_Swc_VehicleState_asild.c':    'SSR-CVC-010, SSR-CVC-011, SSR-CVC-012, SSR-CVC-013, SSR-CVC-018, SSR-CVC-020, SSR-CVC-021, SSR-CVC-022, SSR-CVC-023, SSR-CVC-024, SSR-CVC-025, SSR-CVC-026, SSR-CVC-027, SSR-CVC-028, SWR-CVC-010, SWR-CVC-011, SWR-CVC-012',
    'test_Swc_EStop_asilb.c':           'SSR-CVC-029, SSR-CVC-030, SSR-CVC-031, SSR-CVC-032, SSR-CVC-033, SSR-CVC-034, SSR-CVC-035, SWR-CVC-029, SWR-CVC-030',
    'test_Swc_CvcCom_asild.c':          'SSR-CVC-014, SSR-CVC-015, SSR-CVC-016, SWR-CVC-014, SWR-CVC-015, SWR-CVC-016, SWR-CVC-017',
    'test_Swc_Steering_asild.c':        'SSR-FZC-001, SSR-FZC-002, SSR-FZC-003, SSR-FZC-008, SSR-FZC-009, SSR-FZC-012, SSR-FZC-013, SSR-FZC-017, SWR-FZC-001, SWR-FZC-002, SWR-FZC-003',
    'test_Swc_Brake_asild.c':           'SSR-FZC-005, SSR-FZC-006, SSR-FZC-007, SSR-FZC-021, SSR-FZC-022, SSR-FZC-023, SSR-FZC-024, SSR-FZC-025, SSR-FZC-026, SSR-FZC-027, SWR-FZC-005, SWR-FZC-006, SWR-FZC-007',
    'test_Swc_Lidar_asilc.c':           'SSR-FZC-010, SSR-FZC-011, SSR-FZC-029, SSR-FZC-030, SSR-FZC-031, SSR-FZC-032, SWR-FZC-010, SWR-FZC-011',
    'test_Swc_FzcSafety_asild.c':       'SSR-FZC-015, SSR-FZC-016, SSR-FZC-019, SWR-FZC-015, SWR-FZC-016, SWR-FZC-019',
    'test_Swc_FzcCom_asild.c':          'SSR-FZC-018, SSR-FZC-019, SSR-FZC-020, SWR-FZC-018, SWR-FZC-019, SWR-FZC-020',
    'test_Swc_Motor_asild.c':           'SSR-RZC-001, SSR-RZC-002, SSR-RZC-003, SSR-RZC-006, SSR-RZC-007, SSR-RZC-011, SSR-RZC-012, SSR-RZC-013, SSR-RZC-014, SSR-RZC-017, SSR-RZC-018, SSR-RZC-019, SSR-RZC-020, SSR-RZC-021, SSR-RZC-022, SSR-RZC-023, SSR-RZC-024, SWR-RZC-001, SWR-RZC-002, SWR-RZC-003',
    'test_Swc_Encoder_asilc.c':         'SSR-RZC-005, SSR-RZC-026, SSR-RZC-027, SSR-RZC-028, SSR-RZC-029, SSR-RZC-030, SWR-RZC-005',
    'test_Swc_RzcCom_asild.c':          'SSR-RZC-008, SSR-RZC-009, SWR-RZC-008, SWR-RZC-009',
    'test_Swc_Heartbeat_asilc.c':       'SSR-CVC-017, SSR-FZC-018, SSR-RZC-010, SWR-CVC-017, SWR-FZC-018, SWR-RZC-010',
    'test_int_e2e_faults_asild.c':      'TSR-022, TSR-023, SWR-BSW-023, SWR-BSW-024',
    'test_Dem_asild.c':                 'SWR-BSW-017, SWR-BSW-018, TSR-038, TSR-039',
    'test_CanIf_asild.c':              'SWR-BSW-011, SWR-BSW-012, TSR-022',
    'test_PduR_asild.c':               'SWR-BSW-013, TSR-022, TSR-023',
    # BCM tests (QM)
    'test_Swc_BcmCan_qm.c':           'SSR-BCM-001, SSR-BCM-002, SSR-BCM-010, SSR-BCM-011, SWR-BCM-001, SWR-BCM-002',
    'test_Swc_Lights_qm.c':           'SSR-BCM-003, SSR-BCM-004, SSR-BCM-005, SWR-BCM-003, SWR-BCM-004, SWR-BCM-005',
    'test_Swc_Indicators_qm.c':       'SSR-BCM-006, SSR-BCM-007, SSR-BCM-008, SWR-BCM-006, SWR-BCM-007, SWR-BCM-008',
    'test_Swc_DoorLock_qm.c':         'SSR-BCM-009, SWR-BCM-009',
    'test_Swc_BcmMain_qm.c':          'SSR-BCM-010, SSR-BCM-011, SSR-BCM-012, SWR-BCM-010, SWR-BCM-011, SWR-BCM-012',
    # SC tests (ASIL D)
    'test_sc_can_asild.c':             'SSR-SC-001, SSR-SC-002, SWR-SC-001, SWR-SC-002',
    'test_sc_e2e_asild.c':             'SSR-SC-003, SWR-SC-003',
    'test_sc_heartbeat_asilc.c':       'SSR-SC-004, SSR-SC-005, SSR-SC-006, SWR-SC-004, SWR-SC-005, SWR-SC-006',
    'test_sc_plausibility_asilc.c':    'SSR-SC-007, SSR-SC-008, SSR-SC-009, SWR-SC-007, SWR-SC-008, SWR-SC-009',
    'test_sc_relay_asild.c':           'SSR-SC-010, SSR-SC-011, SSR-SC-012, SWR-SC-010, SWR-SC-011, SWR-SC-012',
    'test_sc_led_qm.c':               'SSR-SC-013, SWR-SC-013',
    'test_sc_esm_asilc.c':            'SSR-SC-014, SSR-SC-015, SWR-SC-014, SWR-SC-015',
    'test_sc_selftest_asild.c':        'SSR-SC-016, SSR-SC-017, SWR-SC-016, SWR-SC-017',
    'test_sc_watchdog_asild.c':        'SSR-SC-017, SWR-SC-022',
    'test_sc_state_asild.c':           'SSR-SC-002, SSR-SC-003, SSR-SC-016, SSR-SC-017, SWR-SC-025',
    'test_sc_main_asild.c':            'SSR-SC-002, SSR-SC-003, SSR-SC-004, SSR-SC-005',
    # ICU tests (QM)
    'test_Swc_Dashboard_qm.c':        'SSR-ICU-001, SSR-ICU-002, SSR-ICU-007, SSR-ICU-008, SSR-ICU-009, SSR-ICU-010, SWR-ICU-001, SWR-ICU-002',
    'test_Swc_DtcDisplay_qm.c':       'SSR-ICU-001, SWR-ICU-008',
    'test_icu_main_qm.c':             'SSR-ICU-001, SSR-ICU-002',
    'test_icu_com_qm.c':              'SSR-ICU-001, SSR-ICU-002',
    # TCU tests (QM)
    'test_Swc_UdsServer_qm.c':        'SSR-TCU-001, SSR-TCU-002, SSR-TCU-014, SWR-TCU-002, SWR-TCU-003',
    'test_Swc_DtcStore_qm.c':         'SSR-TCU-002, SWR-TCU-008, SWR-TCU-009',
    'test_Swc_Obd2Pids_qm.c':         'SSR-TCU-002, SWR-TCU-010',
    'test_Swc_DataAggregator_qm.c':   'SSR-TCU-014, SWR-TCU-014',
    'test_tcu_main_qm.c':             'SSR-TCU-001, SSR-TCU-002',
    'test_tcu_com_qm.c':              'SSR-TCU-001',
}


def add_verifies_yaml(filepath, reqs):
    """Add or merge @verifies to YAML SIL scenario header."""
    with open(filepath, 'r') as f:
        content = f.read()

    existing = re.search(r'# @verifies\s+(.+)', content)
    if existing:
        existing_reqs = set(r.strip() for r in existing.group(1).split(','))
        new_reqs = set(r.strip() for r in reqs.split(','))
        combined = existing_reqs | new_reqs
        if combined == existing_reqs:
            return False
        combined_str = ', '.join(sorted(combined))
        content = re.sub(r'# @verifies\s+.+', f'# @verifies  {combined_str}', content, count=1)
    else:
        tag = f'# @verifies  {reqs}'
        content = re.sub(r'(# @iso\s+.+)', r'\1\n' + tag, content, count=1)

    with open(filepath, 'w') as f:
        f.write(content)
    return True


def add_verifies_c(filepath, reqs):
    """Add @verifies to C unit test file header."""
    with open(filepath, 'r') as f:
        content = f.read()

    # Check if already has verifies with these specific reqs
    if '@verifies' in content:
        # Update existing @verifies line to include new reqs
        existing = re.search(r'@verifies\s+(.+)', content)
        if existing:
            existing_reqs = set(r.strip() for r in existing.group(1).split(','))
            new_reqs = set(r.strip() for r in reqs.split(','))
            combined = existing_reqs | new_reqs
            if combined == existing_reqs:
                return False  # No new reqs to add
            combined_str = ', '.join(sorted(combined))
            content = re.sub(
                r'@verifies\s+.+',
                f'@verifies {combined_str}',
                content,
                count=1
            )
            with open(filepath, 'w') as f:
                f.write(content)
            return True

    return False


def main():
    added = 0

    # SIL scenarios
    for filename, reqs in SIL_MAP.items():
        filepath = os.path.join(ROOT, 'test', 'sil', 'scenarios', filename)
        if os.path.exists(filepath):
            if add_verifies_yaml(filepath, reqs):
                print(f'  YAML: {filename} -> {reqs}')
                added += 1

    # Unit tests
    for filename, reqs in UNIT_MAP.items():
        # Search in multiple directories
        for search_dir in ['test/unit/bsw', 'test/unit/mcal',
                           'firmware/ecu/cvc/test', 'firmware/ecu/fzc/test',
                           'firmware/ecu/rzc/test', 'firmware/ecu/bcm/test',
                           'firmware/ecu/sc/test', 'firmware/ecu/icu/test',
                           'firmware/ecu/tcu/test', 'firmware/bsw/test']:
            filepath = os.path.join(ROOT, search_dir, filename)
            if os.path.exists(filepath):
                if add_verifies_c(filepath, reqs):
                    print(f'  C:    {filename} -> {reqs}')
                    added += 1
                break

    print(f'\nAdded/updated @verifies in {added} files')
    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main())
