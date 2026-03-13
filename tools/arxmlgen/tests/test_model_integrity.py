"""
Model integrity tests — verify arxmlgen's internal data model is correct.

These tests load the full Taktflow ARXML + sidecar and validate that the
parsed model satisfies invariants that ANY correct AUTOSAR codegen must enforce.
These are transport-agnostic — they apply to CAN, LIN, Ethernet, or any future bus.
"""

import pytest


# ===========================================================================
#  Signal Invariants
# ===========================================================================

class TestSignalInvariants:
    """Every signal must have valid, non-conflicting properties."""

    def test_signal_bit_size_nonzero(self, load_model):
        """No signal may have 0-bit size."""
        model = load_model
        for ecu in model.ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                for sig in pdu.signals:
                    assert sig.bit_size > 0, \
                        f"{ecu.name}/{pdu.name}/{sig.name}: bit_size is 0"

    def test_signal_fits_in_pdu(self, load_model):
        """Signal bit_position + bit_size must not exceed PDU DLC × 8."""
        model = load_model
        for ecu in model.ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                max_bits = pdu.dlc * 8
                for sig in pdu.signals:
                    end_bit = sig.bit_position + sig.bit_size
                    assert end_bit <= max_bits, \
                        f"{ecu.name}/{pdu.name}/{sig.name}: " \
                        f"ends at bit {end_bit} but PDU is {max_bits} bits"

    def test_no_duplicate_signal_names_per_pdu(self, load_model):
        """Signal names must be unique within a PDU."""
        model = load_model
        for ecu in model.ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                names = [sig.name for sig in pdu.signals]
                assert len(names) == len(set(names)), \
                    f"{ecu.name}/{pdu.name}: duplicate signal names"

    def test_signal_type_is_valid(self, load_model):
        """Every signal must have a recognized data type."""
        valid_types = {
            "uint8", "uint16", "uint32", "sint8", "sint16", "sint32",
            "boolean", "uint8_t", "uint16_t", "uint32_t",
            "int8_t", "int16_t", "int32_t", "bool",
            "UINT8", "UINT16", "UINT32", "SINT8", "SINT16", "SINT32",
            "BOOLEAN", None, "",  # Allow unresolved (reader warns)
        }
        model = load_model
        for ecu in model.ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                for sig in pdu.signals:
                    if sig.data_type:
                        assert sig.data_type.lower() in {
                            v.lower() if v else "" for v in valid_types if v
                        }, f"{sig.name}: unknown type '{sig.data_type}'"


# ===========================================================================
#  PDU Invariants
# ===========================================================================

class TestPduInvariants:
    """Every PDU must have valid routing and structure."""

    def test_pdu_dlc_range(self, load_model):
        """DLC must be 1–64 (CAN FD max) or 1–8 (classic CAN)."""
        model = load_model
        for ecu in model.ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                assert 1 <= pdu.dlc <= 64, \
                    f"{ecu.name}/{pdu.name}: DLC {pdu.dlc} out of range"

    def test_pdu_has_can_id(self, load_model):
        """Every PDU must have a CAN ID assigned."""
        model = load_model
        for ecu in model.ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                assert pdu.can_id is not None and pdu.can_id >= 0, \
                    f"{ecu.name}/{pdu.name}: missing CAN ID"

    def test_can_id_range(self, load_model):
        """CAN IDs must be in valid 11-bit (0–0x7FF) or 29-bit range."""
        model = load_model
        for ecu in model.ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                assert pdu.can_id <= 0x1FFFFFFF, \
                    f"{ecu.name}/{pdu.name}: CAN ID 0x{pdu.can_id:X} exceeds 29-bit"

    def test_no_duplicate_tx_can_ids_per_ecu(self, load_model):
        """An ECU must not transmit two PDUs with the same CAN ID."""
        model = load_model
        for ecu in model.ecus.values():
            tx_ids = [pdu.can_id for pdu in ecu.tx_pdus]
            assert len(tx_ids) == len(set(tx_ids)), \
                f"{ecu.name}: duplicate TX CAN IDs: {tx_ids}"

    def test_pdu_has_at_least_one_signal(self, load_model):
        """Every PDU should contain at least one signal."""
        model = load_model
        for ecu in model.ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                assert len(pdu.signals) > 0, \
                    f"{ecu.name}/{pdu.name}: PDU has no signals"

    def test_pdu_id_uniqueness(self, load_model):
        """PDU IDs must be unique within TX and within RX for each ECU."""
        model = load_model
        for ecu in model.ecus.values():
            tx_ids = [pdu.pdu_id for pdu in ecu.tx_pdus]
            rx_ids = [pdu.pdu_id for pdu in ecu.rx_pdus]
            assert len(tx_ids) == len(set(tx_ids)), \
                f"{ecu.name}: duplicate TX PDU IDs"
            assert len(rx_ids) == len(set(rx_ids)), \
                f"{ecu.name}: duplicate RX PDU IDs"


# ===========================================================================
#  ECU Invariants
# ===========================================================================

class TestEcuInvariants:
    """Every ECU must have valid identification and routing."""

    def test_all_ecus_present(self, load_model):
        """All 7 Taktflow ECUs must be in the model."""
        model = load_model
        expected = {"cvc", "fzc", "rzc", "sc", "bcm", "icu", "tcu"}
        # Model may have SC as separate — check at least the main 6
        actual = set(model.ecus.keys())
        missing = expected - actual
        assert len(missing) == 0, f"Missing ECUs: {missing}"

    def test_ecu_has_prefix(self, load_model):
        """Every ECU must have a non-empty prefix for code generation."""
        model = load_model
        for name, ecu in model.ecus.items():
            assert ecu.prefix and len(ecu.prefix) > 0, \
                f"{name}: missing prefix"

    def test_ecu_prefix_is_uppercase(self, load_model):
        """ECU prefix must be uppercase (naming convention)."""
        model = load_model
        for name, ecu in model.ecus.items():
            assert ecu.prefix == ecu.prefix.upper(), \
                f"{name}: prefix '{ecu.prefix}' is not uppercase"

    def test_ecu_has_tx_pdus(self, load_model):
        """Every ECU should transmit at least one PDU (heartbeat)."""
        model = load_model
        for name, ecu in model.ecus.items():
            assert len(ecu.tx_pdus) > 0, \
                f"{name}: no TX PDUs (not even heartbeat?)"


# ===========================================================================
#  SWC / Runnable Invariants
# ===========================================================================

class TestSwcInvariants:
    """SWC and runnable model must be consistent."""

    def test_swc_has_name(self, load_model):
        """Every SWC must have a non-empty name."""
        model = load_model
        for ecu in model.ecus.values():
            for swc in ecu.swcs:
                assert swc.name and len(swc.name) > 0, \
                    f"{ecu.name}: SWC with empty name"

    def test_runnable_has_positive_period(self, load_model):
        """Every runnable must have period > 0."""
        model = load_model
        for ecu in model.ecus.values():
            for swc in ecu.swcs:
                for r in swc.runnables:
                    if not r.is_init:
                        assert r.period_ms > 0, \
                            f"{ecu.name}/{swc.name}/{r.name}: period_ms is 0"

    def test_no_duplicate_runnable_names_per_ecu(self, load_model):
        """Runnable names must be unique within an ECU."""
        model = load_model
        for ecu in model.ecus.values():
            all_names = []
            for swc in ecu.swcs:
                for r in swc.runnables:
                    all_names.append(r.name)
            assert len(all_names) == len(set(all_names)), \
                f"{ecu.name}: duplicate runnable names"


# ===========================================================================
#  Cross-ECU Invariants
# ===========================================================================

class TestCrossEcuInvariants:
    """System-level invariants across all ECUs."""

    def test_no_two_ecus_tx_same_can_id(self, load_model):
        """On a shared CAN bus, no two ECUs may transmit the same CAN ID.
        Exception: UDS response IDs may overlap if physically addressed."""
        model = load_model
        uds_range = range(0x7E0, 0x800)  # UDS response range — allowed overlap
        tx_map = {}  # can_id → ecu_name
        for name, ecu in model.ecus.items():
            for pdu in ecu.tx_pdus:
                if pdu.can_id in uds_range:
                    continue
                if pdu.can_id in tx_map:
                    assert False, \
                        f"CAN ID 0x{pdu.can_id:03X} transmitted by both " \
                        f"{tx_map[pdu.can_id]} and {name}"
                tx_map[pdu.can_id] = name

    def test_total_signal_count(self, load_model):
        """Taktflow system has ~162 signals — sanity check."""
        model = load_model
        if hasattr(model, 'totals') and model.totals:
            total = model.totals.get('signals', 0)
            assert total >= 100, \
                f"Only {total} signals — expected ~162 for Taktflow"

    def test_e2e_pdus_have_data_id(self, load_model):
        """PDUs marked as E2E-protected must have a data_id assigned."""
        model = load_model
        for ecu in model.ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                if hasattr(pdu, 'e2e_protected') and pdu.e2e_protected:
                    assert hasattr(pdu, 'e2e_data_id') and pdu.e2e_data_id is not None, \
                        f"{ecu.name}/{pdu.name}: E2E-protected but no data_id"


# ===========================================================================
#  Sidecar Data Invariants
# ===========================================================================

class TestSidecarInvariants:
    """Sidecar-provided data must be merged correctly."""

    def test_cvc_has_dtc_events(self, load_model):
        """CVC (ASIL D) must have DTC events from sidecar."""
        model = load_model
        cvc = model.ecus.get("cvc")
        if cvc is None:
            pytest.skip("CVC not in model")
        assert hasattr(cvc, 'dtc_events') and len(cvc.dtc_events) > 0, \
            "CVC missing DTC events from sidecar"

    def test_cvc_dtc_count(self, load_model):
        """CVC should have 19 DTC events per sidecar."""
        model = load_model
        cvc = model.ecus.get("cvc")
        if cvc is None:
            pytest.skip("CVC not in model")
        if not hasattr(cvc, 'dtc_events'):
            pytest.skip("No dtc_events attribute")
        assert len(cvc.dtc_events) == 19

    def test_cvc_has_enums(self, load_model):
        """CVC must have state enums from sidecar."""
        model = load_model
        cvc = model.ecus.get("cvc")
        if cvc is None:
            pytest.skip("CVC not in model")
        assert hasattr(cvc, 'enums') and len(cvc.enums) > 0, \
            "CVC missing enums from sidecar"

    def test_cvc_has_thresholds(self, load_model):
        """CVC must have threshold constants from sidecar."""
        model = load_model
        cvc = model.ecus.get("cvc")
        if cvc is None:
            pytest.skip("CVC not in model")
        assert hasattr(cvc, 'thresholds') and len(cvc.thresholds) > 0, \
            "CVC missing thresholds from sidecar"
