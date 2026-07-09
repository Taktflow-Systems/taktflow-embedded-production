"""
Tests for Com TX-mode derivation (S-AG-02 / S-AG-03,
docs/plans/memo-arxmlgen-idempotency.md).

Committed TX-mode decisions must be reproducible from the pipeline inputs:
- DBC GenMsgSendType=event      → COM_TX_MODE_DIRECT (FZC Motor_Cutoff_Req, 0x211)
- sidecar message_tx_mode: none → COM_TX_MODE_NONE   (CVC virtual sensors, 0x600/0x601)
"""

import re

import pytest


# ---------------------------------------------------------------------------
# Model-level: Pdu.tx_mode resolution
# ---------------------------------------------------------------------------

def _tx_pdu(model, ecu_name, pdu_name):
    ecu = model.ecus[ecu_name]
    matches = [p for p in ecu.tx_pdus if p.name == pdu_name]
    assert matches, f"{pdu_name} not in {ecu_name} TX PDUs"
    return matches[0]


def test_event_send_type_gives_direct(load_model):
    """DBC GenMsgSendType=event forces DIRECT despite GenMsgCycleTime=50."""
    pdu = _tx_pdu(load_model, "fzc", "Motor_Cutoff_Req")
    assert pdu.tx_mode == "DIRECT"


def test_sidecar_none_override(load_model):
    """Sidecar message_tx_mode: none applies to the DBC sender ECU (CVC)."""
    for name in ("FZC_Virtual_Sensors", "RZC_Virtual_Sensors"):
        pdu = _tx_pdu(load_model, "cvc", name)
        assert pdu.tx_mode == "NONE", f"{name}: expected NONE, got '{pdu.tx_mode}'"


def test_estop_broadcast_pinned_periodic(load_model):
    """EStop_Broadcast is GenMsgSendType=event in the DBC but its repetition
    strategy (event + 10ms repeat) is modelled as periodic TX — the sidecar
    pin must beat the event→DIRECT rule."""
    pdu = _tx_pdu(load_model, "cvc", "EStop_Broadcast")
    assert pdu.tx_mode == "PERIODIC"
    assert pdu.cycle_ms == 10


def test_generated_estop_row_stays_periodic(com_cfg_c):
    row = _tx_row(com_cfg_c("cvc"), "CVC_COM_TX_ESTOP_BROADCAST")
    assert re.search(r",\s*10u,\s*COM_TX_MODE_PERIODIC", row), row


def test_cyclic_messages_keep_legacy_heuristic(load_model):
    """Plain cyclic messages carry no explicit tx_mode (template heuristic)."""
    pdu = _tx_pdu(load_model, "cvc", "CVC_Heartbeat")
    assert pdu.tx_mode == ""
    assert pdu.cycle_ms > 0


# ---------------------------------------------------------------------------
# Generated-file level: Com_Cfg TX table rows
# ---------------------------------------------------------------------------

@pytest.fixture
def com_cfg_c(load_model):
    """Generate Com_Cfg.c for a given ECU, return {ecu: content}."""
    from tools.arxmlgen.generators.com_cfg import ComCfgGenerator
    from tools.arxmlgen.engine import TemplateEngine
    from tools.arxmlgen.config import load_config
    import os

    config_path = os.path.join(
        os.path.dirname(__file__), "..", "..", "..", "project.yaml"
    )
    config = load_config(config_path)
    engine = TemplateEngine(config)
    generator = ComCfgGenerator()

    def render(ecu_name):
        ecu = load_model.ecus[ecu_name]
        rendered = generator.render(ecu, config.generators["com"], engine)
        return rendered["Com_Cfg.c"]

    return render


def _tx_row(content, pdu_define):
    for line in content.splitlines():
        if pdu_define in line and "COM_TX_MODE" in line:
            return line
    raise AssertionError(f"TX row for {pdu_define} not found")


def test_generated_motor_cutoff_row_is_direct(com_cfg_c):
    row = _tx_row(com_cfg_c("fzc"), "FZC_COM_TX_MOTOR_CUTOFF_REQ")
    assert "COM_TX_MODE_DIRECT" in row
    # cycleMs must be 0 for DIRECT (no periodic phantom cutoff — commit 46629dc)
    assert re.search(r",\s*0u,\s*COM_TX_MODE_DIRECT", row)


def test_generated_virtual_sensor_rows_are_none(com_cfg_c):
    content = com_cfg_c("cvc")
    for define in ("CVC_COM_TX_FZC_VIRTUAL_SENSORS", "CVC_COM_TX_RZC_VIRTUAL_SENSORS"):
        row = _tx_row(content, define)
        assert "COM_TX_MODE_NONE" in row, row
        assert re.search(r",\s*0u,\s*COM_TX_MODE_NONE", row), row


def test_no_other_tx_mode_changes(com_cfg_c):
    """Only the three decision rows deviate from the cycle heuristic."""
    for ecu_name in ("cvc", "fzc", "rzc", "bcm", "icu", "tcu"):
        content = com_cfg_c(ecu_name)
        for line in content.splitlines():
            m = re.search(
                r",\s*(\d+)u,\s*COM_TX_MODE_(\w+)\s*,", line
            )
            if not m:
                continue
            cycle, mode = int(m.group(1)), m.group(2)
            if "VIRTUAL_SENSORS" in line or "MOTOR_CUTOFF" in line:
                continue
            expected = "DIRECT" if cycle == 0 else "PERIODIC"
            assert mode == expected, f"{ecu_name}: unexpected mode drift: {line}"
