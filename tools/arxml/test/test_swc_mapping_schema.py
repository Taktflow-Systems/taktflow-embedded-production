"""Disconnected schema contract for explicit signal-to-SWC mappings."""

from __future__ import annotations

from dataclasses import FrozenInstanceError
from pathlib import Path

import pytest

from tools.arxml.swc_mapping import SwcMappingError, load_swc_mapping


FIXTURES = Path(__file__).with_name("fixtures") / "swc_mapping"


def _load(name: str):
    return load_swc_mapping(FIXTURES / name)


def _replace(tmp_path: Path, old: str, new: str) -> Path:
    text = (FIXTURES / "valid.yaml").read_text(encoding="utf-8")
    assert old in text
    path = tmp_path / "mapping.yaml"
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    return path


def test_schema_version_one_parses_to_typed_immutable_objects():
    mapping = _load("valid.yaml")

    assert mapping.schema_version == 1
    assert mapping.policy.unmapped_signal == "error"
    assert mapping.ecus[0].name == "cvc"
    assert mapping.ecus[0].swcs[0].ports[0].identity.message == "Torque_Request"
    assert mapping.ecus[0].swcs[0].ports[0].write_owner is True
    assert mapping.ecus[0].swcs[0].runnables[1].period_ms == 10
    with pytest.raises(FrozenInstanceError):
        mapping.schema_version = 2


def test_yaml_order_has_no_semantics_and_signal_sets_expand_deterministically():
    first = _load("valid.yaml")
    second = _load("valid_reordered.yaml")

    assert first == second
    assert [item.message for item in first.ecus[0].unmapped_signal_sets] == [
        "Brake_Command",
        "Steer_Command",
    ]


@pytest.mark.parametrize(
    ("fixture", "code", "object_address"),
    [
        ("duplicate_key.yaml", "SWCMAP001", "swc_signal_mapping.schema_version"),
        ("missing_required.yaml", "SWCMAP003", "swc_signal_mapping.ecus"),
        ("wrong_type.yaml", "SWCMAP004", "swc_signal_mapping.schema_version"),
        ("invalid_short_name.yaml", "SWCMAP006", "ecus.cvc.swcs.Swc_Pedal.arxml_short_name"),
        ("pattern_identity.yaml", "SWCMAP007", "ecus.cvc.swcs.Swc_Pedal.ports[0].message"),
    ],
)
def test_one_fault_fixtures_report_stable_category_and_object(
    fixture: str, code: str, object_address: str
):
    with pytest.raises(SwcMappingError) as caught:
        _load(fixture)

    assert caught.value.code == code
    assert caught.value.object_address == object_address
    assert str(FIXTURES) not in str(caught.value)


@pytest.mark.parametrize(
    ("old", "new", "object_address"),
    [
        ("schema_version: 1", "schema_version: 1\n  unexpected: true", "swc_signal_mapping.unexpected"),
        ("unmapped_signal: error", "unmapped_signal: error\n    unexpected: true", "policy.unexpected"),
        ("cvc:\n      swcs:", "cvc:\n      unexpected: true\n      swcs:", "ecus.cvc.unexpected"),
        ("Swc_Pedal:\n          arxml_short_name:", "Swc_Pedal:\n          unexpected: true\n          arxml_short_name:", "ecus.cvc.swcs.Swc_Pedal.unexpected"),
        ("function: Swc_Pedal_Init", "function: Swc_Pedal_Init\n              unexpected: true", "ecus.cvc.swcs.Swc_Pedal.runnables[0].unexpected"),
        ("name: PP_Torque_Request_PedalPosition1", "name: PP_Torque_Request_PedalPosition1\n              unexpected: true", "ecus.cvc.swcs.Swc_Pedal.ports[0].unexpected"),
        ("signal: Vehicle_State_Mode", "signal: Vehicle_State_Mode\n          unexpected: true", "ecus.cvc.unmapped_signals[0].unexpected"),
        ("messages: [Steer_Command, Brake_Command]", "messages: [Steer_Command, Brake_Command]\n          unexpected: true", "ecus.cvc.unmapped_signal_sets[0].unexpected"),
        ("function: Swc_Pedal_Background", "function: Swc_Pedal_Background\n          unexpected: true", "ecus.cvc.unmapped_runnables[0].unexpected"),
    ],
)
def test_unknown_keys_are_rejected_at_every_mapping_level(
    tmp_path: Path, old: str, new: str, object_address: str
):
    with pytest.raises(SwcMappingError) as caught:
        load_swc_mapping(_replace(tmp_path, old, new))

    assert caught.value.code == "SWCMAP002"
    assert caught.value.object_address == object_address


@pytest.mark.parametrize(
    ("old", "new", "object_address"),
    [
        ("message: Torque_Request", "message_regex: Torque_.*", "ecus.cvc.swcs.Swc_Pedal.ports[0].message_regex"),
        ("signal: Torque_Request_PedalPosition1", "signal_prefix: Torque_Request", "ecus.cvc.swcs.Swc_Pedal.ports[0].signal_prefix"),
        ("message: Torque_Request", "message_suffix: Request", "ecus.cvc.swcs.Swc_Pedal.ports[0].message_suffix"),
        ("signal: Torque_Request_PedalPosition1", "signal_substring: Pedal", "ecus.cvc.swcs.Swc_Pedal.ports[0].signal_substring"),
    ],
)
def test_pattern_mapping_fields_are_rejected(
    tmp_path: Path, old: str, new: str, object_address: str
):
    with pytest.raises(SwcMappingError) as caught:
        load_swc_mapping(_replace(tmp_path, old, new))

    assert caught.value.code == "SWCMAP002"
    assert caught.value.object_address == object_address


@pytest.mark.parametrize(
    ("old", "new", "code", "object_address"),
    [
        ("schema_version: 1", "schema_version: 2", "SWCMAP005", "swc_signal_mapping.schema_version"),
        ("write_owner: true", 'write_owner: "true"', "SWCMAP004", "ecus.cvc.swcs.Swc_Pedal.ports[0].write_owner"),
        ("period_ms: 10", "period_ms: 0", "SWCMAP005", "ecus.cvc.swcs.Swc_Pedal.runnables[1].period_ms"),
        ("trigger: init", "trigger: periodic", "SWCMAP003", "ecus.cvc.swcs.Swc_Pedal.runnables[0].period_ms"),
        ("direction: provided", "direction: sideways", "SWCMAP005", "ecus.cvc.swcs.Swc_Pedal.ports[0].direction"),
        ("message: Torque_Request", "message: Torque_Request?", "SWCMAP007", "ecus.cvc.swcs.Swc_Pedal.ports[0].message"),
    ],
)
def test_invalid_versions_types_values_and_pattern_identities_fail_closed(
    tmp_path: Path, old: str, new: str, code: str, object_address: str
):
    with pytest.raises(SwcMappingError) as caught:
        load_swc_mapping(_replace(tmp_path, old, new))

    assert caught.value.code == code
    assert caught.value.object_address == object_address


def test_parser_reads_only_the_explicitly_supplied_path(tmp_path: Path):
    explicit = tmp_path / "explicit.yaml"
    explicit.write_text((FIXTURES / "valid.yaml").read_text(encoding="utf-8"), encoding="utf-8")

    assert load_swc_mapping(explicit).schema_version == 1
    with pytest.raises(FileNotFoundError):
        load_swc_mapping(tmp_path / "missing.yaml")
