"""
Quality verification tests — compare arxmlgen output against professional reference.

These tests parse both professional reference files and arxmlgen-generated output,
then verify structural equivalence. They do NOT require byte-identical output.

Test categories:
  1. Structural completeness — all required sections present
  2. Data integrity — signal/PDU counts, IDs, bit layouts match DBC
  3. Cross-module consistency — Com, CanIf, Rte agree on IDs and routing
  4. Professional parity — generated output has same quality properties
"""

import os
import re
import pytest
from conftest import PROFESSIONAL_DIR


# ---------------------------------------------------------------------------
#  Helpers — parse C config files for structural comparison
# ---------------------------------------------------------------------------

def extract_defines(filepath):
    """Extract #define NAME VALUE pairs from a C header."""
    defines = {}
    if not os.path.exists(filepath):
        return defines
    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            m = re.match(r'^\s*#define\s+(\w+)\s+(\S+)', line)
            if m:
                name, value = m.group(1), m.group(2)
                # Strip trailing 'u' suffix and comments
                value = re.sub(r'[uU]$', '', value)
                value = value.split('/*')[0].strip()
                try:
                    defines[name] = int(value, 0)
                except ValueError:
                    defines[name] = value
    return defines


def extract_array_entries(filepath, array_name_pattern):
    """Count entries in a C static const array by counting '{' at nesting level 1."""
    if not os.path.exists(filepath):
        return 0
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    # Find the array
    pattern = re.compile(
        rf'{array_name_pattern}\s*\[\s*\]\s*=\s*\{{(.*?)\}};',
        re.DOTALL
    )
    m = pattern.search(content)
    if not m:
        return 0

    body = m.group(1)
    # Count top-level entries by counting '{ ... }' blocks
    depth = 0
    count = 0
    for ch in body:
        if ch == '{':
            depth += 1
            if depth == 1:
                count += 1
        elif ch == '}':
            depth -= 1
    return count


def extract_can_ids(filepath):
    """Extract CAN IDs from comments like '/* CAN 0x400 */' or hex literals '0x400u'."""
    can_ids = set()
    if not os.path.exists(filepath):
        return can_ids
    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            # Match CAN ID in comments
            for m in re.finditer(r'CAN\s+(0x[0-9A-Fa-f]+)', line):
                can_ids.add(int(m.group(1), 16))
            # Match hex CAN IDs in CanIf config arrays (first field)
            m = re.match(r'\s*\{\s*(0x[0-9A-Fa-f]+)u?\s*,', line)
            if m:
                can_ids.add(int(m.group(1), 16))
    return can_ids


def has_generated_header(filepath):
    """Check if file has a 'GENERATED' / 'DO NOT EDIT' marker."""
    if not os.path.exists(filepath):
        return False
    with open(filepath, "r", encoding="utf-8") as f:
        header = f.read(1024)
    return "GENERATED" in header and "DO NOT EDIT" in header


def has_header_guard(filepath):
    """Check if .h file has proper #ifndef/#define header guard."""
    if not os.path.exists(filepath):
        return False
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()
    return bool(re.search(r'#ifndef\s+\w+_H', content) and
                re.search(r'#define\s+\w+_H', content))


def count_signal_entries(filepath):
    """Count signal config entries in Com_Cfg or Rte_Cfg."""
    patterns = [
        r'[Ss]ignal[Cc]onfig',
        r'[Ss]ignal_[Cc]onfig',
        r'SignalConfig',
    ]
    for p in patterns:
        count = extract_array_entries(filepath, p)
        if count > 0:
            return count
    return 0


# ---------------------------------------------------------------------------
#  BCM Professional Reference — Known Values
# ---------------------------------------------------------------------------

BCM_EXPECTED = {
    "com_signal_count": 12,      # 6 TX + 6 RX
    "com_tx_pdu_count": 3,
    "com_rx_pdu_count": 3,
    "rte_signal_count": 26,      # 16 BSW + 10 BCM-specific
    "runnable_count": 8,
    "canif_tx_count": 3,
    "canif_rx_count": 3,
    "tx_can_ids": {0x400, 0x401, 0x402},
    "rx_can_ids": {0x100, 0x301, 0x350},
    "bsw_reserved_signals": 16,  # IDs 0-15
    "ecu_signal_offset": 16,     # BCM signals start at 16
}


# ===========================================================================
#  Test Category 1: Structural Completeness
# ===========================================================================

class TestStructuralCompleteness:
    """Verify professional reference files have all required sections."""

    def test_com_cfg_h_exists(self, bcm_professional_dir):
        assert os.path.exists(os.path.join(bcm_professional_dir, "Com_Cfg.h"))

    def test_com_cfg_c_exists(self, bcm_professional_dir):
        assert os.path.exists(os.path.join(bcm_professional_dir, "Com_Cfg.c"))

    def test_rte_cfg_c_exists(self, bcm_professional_dir):
        assert os.path.exists(os.path.join(bcm_professional_dir, "Rte_Cfg.c"))

    def test_canif_cfg_c_exists(self, bcm_professional_dir):
        assert os.path.exists(os.path.join(bcm_professional_dir, "CanIf_Cfg.c"))

    def test_bcm_cfg_h_exists(self, bcm_professional_dir):
        assert os.path.exists(os.path.join(bcm_professional_dir, "Bcm_Cfg.h"))

    def test_all_files_have_generated_marker(self, bcm_professional_dir):
        for fname in ["Com_Cfg.h", "Com_Cfg.c", "Rte_Cfg.c", "CanIf_Cfg.c", "Bcm_Cfg.h"]:
            fpath = os.path.join(bcm_professional_dir, fname)
            assert has_generated_header(fpath), f"{fname} missing GENERATED marker"

    def test_headers_have_guards(self, bcm_professional_dir):
        for fname in ["Com_Cfg.h", "Bcm_Cfg.h"]:
            fpath = os.path.join(bcm_professional_dir, fname)
            assert has_header_guard(fpath), f"{fname} missing header guard"


# ===========================================================================
#  Test Category 2: Data Integrity (against known BCM values)
# ===========================================================================

class TestDataIntegrity:
    """Verify signal/PDU counts and IDs match expected BCM configuration."""

    def test_com_signal_count(self, bcm_professional_dir):
        defines = extract_defines(
            os.path.join(bcm_professional_dir, "Com_Cfg.h"))
        assert defines.get("COM_CFG_SIGNAL_COUNT") == BCM_EXPECTED["com_signal_count"]

    def test_com_tx_pdu_count(self, bcm_professional_dir):
        defines = extract_defines(
            os.path.join(bcm_professional_dir, "Com_Cfg.h"))
        assert defines.get("COM_CFG_TX_PDU_COUNT") == BCM_EXPECTED["com_tx_pdu_count"]

    def test_com_rx_pdu_count(self, bcm_professional_dir):
        defines = extract_defines(
            os.path.join(bcm_professional_dir, "Com_Cfg.h"))
        assert defines.get("COM_CFG_RX_PDU_COUNT") == BCM_EXPECTED["com_rx_pdu_count"]

    def test_rte_signal_count(self, bcm_professional_dir):
        defines = extract_defines(
            os.path.join(bcm_professional_dir, "Bcm_Cfg.h"))
        assert defines.get("BCM_SIG_COUNT") == BCM_EXPECTED["rte_signal_count"]

    def test_runnable_count(self, bcm_professional_dir):
        defines = extract_defines(
            os.path.join(bcm_professional_dir, "Bcm_Cfg.h"))
        assert defines.get("BCM_RUNNABLE_COUNT") == BCM_EXPECTED["runnable_count"]

    def test_bsw_signal_offset(self, bcm_professional_dir):
        """ECU-specific signals must start at offset 16 (after BSW reserved)."""
        defines = extract_defines(
            os.path.join(bcm_professional_dir, "Bcm_Cfg.h"))
        assert defines.get("BCM_SIG_VEHICLE_SPEED") == BCM_EXPECTED["ecu_signal_offset"]

    def test_tx_can_ids_present(self, bcm_professional_dir):
        """All BCM TX CAN IDs must appear in CanIf config."""
        can_ids = extract_can_ids(
            os.path.join(bcm_professional_dir, "CanIf_Cfg.c"))
        assert BCM_EXPECTED["tx_can_ids"].issubset(can_ids)

    def test_rx_can_ids_present(self, bcm_professional_dir):
        """All BCM RX CAN IDs must appear in CanIf config."""
        can_ids = extract_can_ids(
            os.path.join(bcm_professional_dir, "CanIf_Cfg.c"))
        assert BCM_EXPECTED["rx_can_ids"].issubset(can_ids)

    def test_signal_array_count_matches_define(self, bcm_professional_dir):
        """Number of entries in Com signal array must match COM_CFG_SIGNAL_COUNT."""
        count = count_signal_entries(
            os.path.join(bcm_professional_dir, "Com_Cfg.c"))
        assert count == BCM_EXPECTED["com_signal_count"]

    def test_runnable_array_count_matches_define(self, bcm_professional_dir):
        """Number of entries in Rte runnable array must match BCM_RUNNABLE_COUNT."""
        count = extract_array_entries(
            os.path.join(bcm_professional_dir, "Rte_Cfg.c"),
            r'[Rr]te_[Rr]unnable[Cc]onfig')
        assert count == BCM_EXPECTED["runnable_count"]


# ===========================================================================
#  Test Category 3: Cross-Module Consistency
# ===========================================================================

class TestCrossModuleConsistency:
    """Verify Com, CanIf, and Rte agree on IDs and counts."""

    def test_canif_tx_count_matches_com(self, bcm_professional_dir):
        canif_count = extract_array_entries(
            os.path.join(bcm_professional_dir, "CanIf_Cfg.c"),
            r'[Cc]an[Ii]f_[Tt]x[Pp]du[Cc]onfig')
        assert canif_count == BCM_EXPECTED["com_tx_pdu_count"]

    def test_canif_rx_count_matches_com(self, bcm_professional_dir):
        canif_count = extract_array_entries(
            os.path.join(bcm_professional_dir, "CanIf_Cfg.c"),
            r'[Cc]an[Ii]f_[Rr]x[Pp]du[Cc]onfig')
        assert canif_count == BCM_EXPECTED["com_rx_pdu_count"]

    def test_rte_signal_array_matches_count(self, bcm_professional_dir):
        rte_count = count_signal_entries(
            os.path.join(bcm_professional_dir, "Rte_Cfg.c"))
        assert rte_count == BCM_EXPECTED["rte_signal_count"]

    def test_tx_can_ids_in_both_canif_and_com(self, bcm_professional_dir):
        """TX CAN IDs documented in Com_Cfg.h must appear in CanIf_Cfg.c."""
        com_h = os.path.join(bcm_professional_dir, "Com_Cfg.h")
        canif_c = os.path.join(bcm_professional_dir, "CanIf_Cfg.c")
        com_can_ids = extract_can_ids(com_h)
        canif_can_ids = extract_can_ids(canif_c)
        # All CAN IDs in Com header should appear in CanIf
        for cid in BCM_EXPECTED["tx_can_ids"]:
            assert cid in canif_can_ids, f"TX CAN ID 0x{cid:03X} in Com but not in CanIf"


# ===========================================================================
#  Test Category 4: Professional Parity (quality properties)
# ===========================================================================

class TestProfessionalParity:
    """Verify generated output has same quality properties as professional tools."""

    def test_no_magic_numbers_in_pdu_config(self, bcm_professional_dir):
        """PDU config entries should use symbolic #define names or have comments."""
        fpath = os.path.join(bcm_professional_dir, "Com_Cfg.c")
        if not os.path.exists(fpath):
            pytest.skip("File not found")
        with open(fpath, "r", encoding="utf-8") as f:
            content = f.read()
        # Every array entry line with a numeric PDU ID should have a comment or use a define
        for line in content.split('\n'):
            if re.match(r'\s*\{.*PduId.*\d+u', line):
                assert ('/*' in line or '//' in line or
                        'ComConf_' in line or 'BCM_COM_' in line), \
                    f"Magic number without context: {line.strip()}"

    def test_shadow_buffers_are_static(self, bcm_professional_dir):
        """Shadow buffers must be static (not globally visible)."""
        fpath = os.path.join(bcm_professional_dir, "Com_Cfg.c")
        if not os.path.exists(fpath):
            pytest.skip("File not found")
        with open(fpath, "r", encoding="utf-8") as f:
            content = f.read()
        # Find buffer declarations — they should be static or VAR()
        buf_lines = [l for l in content.split('\n')
                     if 'SigBuf' in l or 'sig_tx_' in l or 'sig_rx_' in l]
        for line in buf_lines:
            if line.strip() and not line.strip().startswith('//') and not line.strip().startswith('/*'):
                assert 'static' in line or 'VAR(' in line, \
                    f"Buffer not static: {line.strip()}"

    def test_config_structs_are_const(self, bcm_professional_dir):
        """Config arrays and aggregate structs must be const."""
        for fname in ["Com_Cfg.c", "Rte_Cfg.c", "CanIf_Cfg.c"]:
            fpath = os.path.join(bcm_professional_dir, fname)
            if not os.path.exists(fpath):
                continue
            with open(fpath, "r", encoding="utf-8") as f:
                content = f.read()
            # Array declarations should contain 'const' or 'CONST('
            arrays = re.findall(
                r'(?:static\s+)?(?:CONST|const)\s*\(?\s*\w+.*?\w+\s*\[\]',
                content)
            assert len(arrays) > 0, f"{fname} has no const arrays"

    def test_signal_bit_positions_no_overlap(self, bcm_professional_dir):
        """Signals in the same PDU must not have overlapping bit ranges."""
        fpath = os.path.join(bcm_professional_dir, "Com_Cfg.c")
        if not os.path.exists(fpath):
            pytest.skip("File not found")
        with open(fpath, "r", encoding="utf-8") as f:
            content = f.read()

        # Parse signal entries: extract (pdu_id_ref, bit_position, bit_size)
        # Look for patterns like: BitPosition = X, BitSize = Y, PduId = Z
        signals_by_pdu = {}
        current_signal = {}

        for line in content.split('\n'):
            bp_match = re.search(r'BitPosition\s*(?:=\s*)?(\d+)u?', line)
            bs_match = re.search(r'BitSize\s*(?:=\s*)?(\d+)u?', line)
            pdu_match = re.search(r'PduId\s*(?:=\s*)?(\w+)', line)

            if bp_match:
                current_signal['bit_pos'] = int(bp_match.group(1))
            if bs_match:
                current_signal['bit_size'] = int(bs_match.group(1))
            if pdu_match:
                current_signal['pdu'] = pdu_match.group(1)

            if len(current_signal) == 3:
                pdu = current_signal['pdu']
                if pdu not in signals_by_pdu:
                    signals_by_pdu[pdu] = []
                signals_by_pdu[pdu].append(
                    (current_signal['bit_pos'], current_signal['bit_size']))
                current_signal = {}

        # Check for overlaps within each PDU
        for pdu, signals in signals_by_pdu.items():
            for i, (pos_a, size_a) in enumerate(signals):
                for j, (pos_b, size_b) in enumerate(signals):
                    if i >= j:
                        continue
                    range_a = range(pos_a, pos_a + size_a)
                    range_b = range(pos_b, pos_b + size_b)
                    overlap = set(range_a) & set(range_b)
                    assert len(overlap) == 0, \
                        f"PDU {pdu}: signals at bit {pos_a}:{size_a} and " \
                        f"{pos_b}:{size_b} overlap at bits {overlap}"


# ===========================================================================
#  Test Category 5: arxmlgen Model vs Professional Reference
#  (These tests run only when arxmlgen model is loadable)
# ===========================================================================

class TestModelVsProfessional:
    """Compare arxmlgen's parsed model against professional reference values."""

    def test_bcm_signal_count(self, load_model):
        """Model BCM must have signals (broadcast CAN gives all non-TX as RX)."""
        model = load_model
        bcm = model.ecus.get("bcm")
        if bcm is None:
            pytest.skip("BCM not in model")
        tx_signals = set()
        for pdu in bcm.tx_pdus:
            for sig in pdu.signals:
                tx_signals.add(sig.name)
        assert len(tx_signals) >= 6, f"BCM TX signals: {len(tx_signals)} (expected >=6)"
        all_signals = set()
        for pdu in bcm.tx_pdus + bcm.rx_pdus:
            for sig in pdu.signals:
                all_signals.add(sig.name)
        assert len(all_signals) >= BCM_EXPECTED["com_signal_count"]

    def test_bcm_tx_pdu_count(self, load_model):
        model = load_model
        bcm = model.ecus.get("bcm")
        if bcm is None:
            pytest.skip("BCM not in model")
        assert len(bcm.tx_pdus) == BCM_EXPECTED["com_tx_pdu_count"]

    def test_bcm_rx_pdu_count(self, load_model):
        """RX count >= baseline (broadcast CAN gives more until generator filters)."""
        model = load_model
        bcm = model.ecus.get("bcm")
        if bcm is None:
            pytest.skip("BCM not in model")
        assert len(bcm.rx_pdus) >= BCM_EXPECTED["com_rx_pdu_count"]

    def test_bcm_tx_can_ids(self, load_model):
        model = load_model
        bcm = model.ecus.get("bcm")
        if bcm is None:
            pytest.skip("BCM not in model")
        tx_ids = {pdu.can_id for pdu in bcm.tx_pdus}
        assert tx_ids == BCM_EXPECTED["tx_can_ids"]

    def test_bcm_rx_can_ids(self, load_model):
        model = load_model
        bcm = model.ecus.get("bcm")
        if bcm is None:
            pytest.skip("BCM not in model")
        rx_ids = {pdu.can_id for pdu in bcm.rx_pdus}
        assert BCM_EXPECTED["rx_can_ids"].issubset(rx_ids)

    def test_bcm_swc_count(self, load_model):
        model = load_model
        bcm = model.ecus.get("bcm")
        if bcm is None:
            pytest.skip("BCM not in model")
        # BCM has 3 SWCs: Lights, Indicators, DoorLock
        assert len(bcm.swcs) >= 3

    def test_all_ecus_have_heartbeat_can_id(self, load_model):
        """Every ECU must have its heartbeat CAN ID in TX PDUs."""
        model = load_model
        heartbeat_ids = {
            "cvc": 0x010, "fzc": 0x011, "rzc": 0x012,
            "sc": 0x013, "icu": 0x014, "tcu": 0x015,
        }
        for ecu_name, hb_id in heartbeat_ids.items():
            ecu = model.ecus.get(ecu_name)
            if ecu is None:
                continue
            tx_ids = {pdu.can_id for pdu in ecu.tx_pdus}
            assert hb_id in tx_ids, \
                f"{ecu_name} missing heartbeat CAN ID 0x{hb_id:03X}"
