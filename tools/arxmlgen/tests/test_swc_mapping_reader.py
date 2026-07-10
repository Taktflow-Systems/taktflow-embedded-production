"""SM4 exact mapping identity contracts for the arxmlgen reader."""

from types import SimpleNamespace

from tools.arxmlgen.model import Ecu, Pdu, Port, Signal, Swc
from tools.arxmlgen.reader import ArxmlReader


def test_mapping_index_carries_exact_swc_and_port_identity(tmp_path):
    sidecar = tmp_path / "sidecar.yaml"
    sidecar.write_text(
        """swc_signal_mapping:
  ecus:
    cvc:
      swcs:
        Swc_Logical:
          arxml_short_name: ComponentWithoutEcuPrefix
          ports:
            - name: ExactPort
              message: ExactMessage
              signal: ExactSignal
""",
        encoding="utf-8",
    )
    reader = ArxmlReader.__new__(ArxmlReader)
    reader.config = SimpleNamespace(sidecar_path=str(sidecar))
    reader._mapped_swcs = {}
    reader._mapped_ports = {}

    reader._load_explicit_mapping_index()

    assert reader._mapped_swcs == {
        "ComponentWithoutEcuPrefix": ("cvc", "Swc_Logical")
    }
    assert reader._mapped_ports == {
        ("ComponentWithoutEcuPrefix", "ExactPort"): (
            "ExactMessage",
            "ExactSignal",
        )
    }


def test_mapped_message_signal_type_precedes_suffix_recovery():
    reader = ArxmlReader.__new__(ArxmlReader)
    reader._dbc_signal_types = {("ExactMessage", "ExactSignal"): "uint16_t"}
    port = Port(
        name="ExactPort",
        signal_name="ExactSignal",
        message_name="ExactMessage",
    )
    ecu = Ecu(
        name="cvc",
        prefix="CVC",
        swcs=[Swc(name="Swc_Logical", ports=[port])],
        tx_pdus=[
            Pdu(
                name="Misleading",
                signals=[Signal("Misleading_ExactSignal", 0, 8, data_type="uint8_t")],
            )
        ],
    )

    reader._resolve_port_types({"cvc": ecu})

    assert port.data_type == "uint16_t"
