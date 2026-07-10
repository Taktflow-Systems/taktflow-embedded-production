"""Fail-closed cross-input validation for explicit SWC mappings."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from tools.arxml.swc_mapping import (
    SwcMappingValidationError,
    load_swc_mapping,
    validate_before_publish,
    validate_swc_mapping,
)


FIXTURES = Path(__file__).with_name("fixtures") / "swc_mapping"
DBC = FIXTURES / "validation.dbc"
MODEL = FIXTURES / "validation_model.json"


def _mapping(tmp_path: Path, *replacements: tuple[str, str]):
    text = (FIXTURES / "validation_valid.yaml").read_text(encoding="utf-8")
    for old, new in replacements:
        assert old in text
        text = text.replace(old, new, 1)
    path = tmp_path / "mapping.yaml"
    path.write_text(text, encoding="utf-8")
    return load_swc_mapping(path)


def _invalid(tmp_path: Path, *replacements: tuple[str, str]):
    mapping = _mapping(tmp_path, *replacements)
    with pytest.raises(SwcMappingValidationError) as caught:
        validate_swc_mapping(mapping, DBC, MODEL)
    return caught.value


def _diagnostics(error: SwcMappingValidationError) -> list[tuple[str, str]]:
    return [(item.code, item.object_address) for item in error.diagnostics]


def test_valid_mapping_covers_all_nine_rules_and_is_order_independent(tmp_path: Path):
    first = validate_swc_mapping(load_swc_mapping(FIXTURES / "validation_valid.yaml"), DBC, MODEL)
    reordered = _mapping(
        tmp_path,
        ("Swc_Pedal_Init\n              trigger: init", "Swc_Pedal_MainFunction\n              trigger: periodic\n              period_ms: 10"),
        ("Swc_Pedal_MainFunction\n              trigger: periodic\n              period_ms: 10", "Swc_Pedal_Init\n              trigger: init"),
    )
    second = validate_swc_mapping(reordered, DBC, MODEL)

    assert first == second
    assert first.expanded_unmapped_signals == ()


def test_omitted_governed_routed_ecu_fails_closed(tmp_path: Path):
    text = (FIXTURES / "validation_valid.yaml").read_text(encoding="utf-8")
    assert "\n    fzc:\n" in text
    path = tmp_path / "mapping.yaml"
    path.write_text(text.split("\n    fzc:\n", 1)[0] + "\n", encoding="utf-8")

    with pytest.raises(SwcMappingValidationError) as caught:
        validate_swc_mapping(load_swc_mapping(path), DBC, MODEL)

    assert _diagnostics(caught.value) == [
        ("SWCMAP011", "ecus.fzc.coverage.mapping")
    ]


def test_external_diagnostic_node_is_required_only_when_explicitly_governed():
    mapping = load_swc_mapping(FIXTURES / "validation_valid.yaml")
    validate_swc_mapping(mapping, DBC, MODEL)

    with pytest.raises(SwcMappingValidationError) as caught:
        validate_swc_mapping(mapping, DBC, MODEL, governed_ecus=("diag",))

    assert _diagnostics(caught.value) == [
        ("SWCMAP011", "ecus.diag.coverage.mapping")
    ]


@pytest.mark.parametrize(
    ("old", "new", "code", "address"),
    [
        ("cvc:\n      swcs:", "unknown:\n      swcs:", "SWCMAP008", "ecus.unknown"),
        ("Swc_Pedal:\n          arxml_short_name:", "Swc_Unknown:\n          arxml_short_name:", "SWCMAP008", "ecus.cvc.swcs.Swc_Unknown"),
        ("function: Swc_Pedal_Init", "function: Swc_Pedal_Unknown", "SWCMAP008", "ecus.cvc.swcs.Swc_Pedal.runnables.Swc_Pedal_Unknown"),
        ("message: Torque_Request", "message: Unknown_Message", "SWCMAP008", "ecus.cvc.swcs.Swc_Pedal.ports.provided.Unknown_Message.PedalPosition"),
        ("signal: PedalPosition", "signal: UnknownSignal", "SWCMAP008", "ecus.cvc.swcs.Swc_Pedal.ports.provided.Torque_Request.UnknownSignal"),
    ],
)
def test_unknown_references_fail_closed(tmp_path: Path, old: str, new: str, code: str, address: str):
    error = _invalid(tmp_path, (old, new))
    assert (code, address) in _diagnostics(error)


def test_sanitized_identity_collision_is_rejected(tmp_path: Path):
    dbc = tmp_path / "collision.dbc"
    dbc.write_text(
        DBC.read_text(encoding="utf-8")
        + '\nBO_ 102 Torque__Request: 8 CVC\n SG_ PedalPosition : 0|8@1+ (1,0) [0|100] "" FZC\n',
        encoding="utf-8",
    )
    with pytest.raises(SwcMappingValidationError) as caught:
        validate_swc_mapping(load_swc_mapping(FIXTURES / "validation_valid.yaml"), dbc, MODEL)
    assert ("SWCMAP009", "dbc.Torque__Request.PedalPosition") in _diagnostics(caught.value)


@pytest.mark.parametrize(
    ("old", "new", "address"),
    [
        ("direction: provided", "direction: required", "ecus.cvc.swcs.Swc_Pedal.ports.required.Torque_Request.PedalPosition"),
        ("direction: required", "direction: provided", "ecus.cvc.swcs.Swc_Pedal.ports.provided.Brake_Command.BrakeForceCmd"),
    ],
)
def test_sender_and_receiver_mismatches_are_rejected(tmp_path: Path, old: str, new: str, address: str):
    assert ("SWCMAP010", address) in _diagnostics(_invalid(tmp_path, (old, new)))


def test_signal_and_runnable_coverage_is_fail_closed(tmp_path: Path):
    error = _invalid(
        tmp_path,
        ("            - name: RP_BrakeForceCmd\n              direction: required\n              message: Brake_Command\n              signal: BrakeForceCmd\n              interface: SRI_BrakeForceCmd\n", ""),
        ("      unmapped_runnables:\n        - function: Swc_Pedal_Background\n          disposition: legacy-not-emitted\n          rationale: The background function is not emitted during migration.\n", ""),
    )
    assert _diagnostics(error) == [
        ("SWCMAP011", "ecus.cvc.coverage.required.Brake_Command.BrakeForceCmd"),
        ("SWCMAP011", "ecus.cvc.coverage.runnable.Swc_Pedal_Background"),
    ]


@pytest.mark.parametrize(
    ("old", "new", "address"),
    [
        ("name: RP_BrakeForceCmd", "name: PP_PedalPosition", "ecus.cvc.swcs.Swc_Pedal.ports.PP_PedalPosition"),
        ("arxml_short_name: CVC_Swc_Pedal", "arxml_short_name: CVC_Swc_Pedal\n        Swc_Shadow:\n          arxml_short_name: CVC_Swc_Pedal", "ecus.cvc.swcs.Swc_Shadow.arxml_short_name"),
        ("function: Swc_Pedal_MainFunction", "function: Swc_Pedal_Init", "ecus.cvc.runnables.Swc_Pedal_Init"),
    ],
)
def test_duplicate_port_arxml_and_runnable_names_are_rejected(tmp_path: Path, old: str, new: str, address: str):
    assert ("SWCMAP012", address) in _diagnostics(_invalid(tmp_path, (old, new)))


def test_mapped_and_unmapped_identity_has_no_precedence(tmp_path: Path):
    error = _invalid(
        tmp_path,
        ("      unmapped_runnables:", "      unmapped_signals:\n        - direction: required\n          message: Brake_Command\n          signal: BrakeForceCmd\n          disposition: legacy-no-port\n          rationale: This is an intentional negative fixture.\n      unmapped_runnables:"),
    )
    assert ("SWCMAP015", "ecus.cvc.required.Brake_Command.BrakeForceCmd") in _diagnostics(error)


def test_inconsistent_sharing_group_and_interface_are_rejected(tmp_path: Path):
    error = _invalid(
        tmp_path,
        ("      unmapped_runnables:", "        Swc_Shadow:\n          arxml_short_name: CVC_Swc_Shadow\n          ports:\n            - name: PP_Shadow\n              direction: provided\n              message: Torque_Request\n              signal: PedalPosition\n              interface: SRI_Other\n              sharing_group: other\n      unmapped_runnables:"),
    )
    assert ("SWCMAP013", "ecus.cvc.provided.Torque_Request.PedalPosition") in _diagnostics(error)


@pytest.mark.parametrize("owners", [(), (True, True)])
def test_shared_provided_binding_has_exactly_one_write_owner(tmp_path: Path, owners: tuple[bool, ...]):
    owner_lines = ["              write_owner: true\n" if value else "" for value in owners]
    first = owner_lines[0] if owner_lines else ""
    second = owner_lines[1] if len(owner_lines) > 1 else ""
    error = _invalid(
        tmp_path,
        ("              write_owner: true\n", first + "              sharing_group: pedal-providers\n"),
        ("      unmapped_runnables:", "        Swc_Shadow:\n          arxml_short_name: CVC_Swc_Shadow\n          ports:\n            - name: PP_Shadow\n              direction: provided\n              message: Torque_Request\n              signal: PedalPosition\n              interface: SRI_PedalPosition\n              sharing_group: pedal-providers\n" + second + "      unmapped_runnables:"),
    )
    assert ("SWCMAP013", "ecus.cvc.provided.Torque_Request.PedalPosition.write_owner") in _diagnostics(error)


@pytest.mark.parametrize("kind", ["unmapped", "shared"])
def test_safety_sensitive_exceptions_require_reviewable_approval(tmp_path: Path, kind: str):
    if kind == "unmapped":
        replacements = (
            ("            - name: RP_BrakeForceCmd\n              direction: required\n              message: Brake_Command\n              signal: BrakeForceCmd\n              interface: SRI_BrakeForceCmd\n", ""),
            ("      unmapped_runnables:", "      unmapped_signals:\n        - direction: required\n          message: Brake_Command\n          signal: BrakeForceCmd\n          disposition: architecture-exception\n          rationale: The owner approved a temporary migration exception.\n      unmapped_runnables:"),
        )
    else:
        replacements = (
            ("              interface: SRI_BrakeForceCmd", "              interface: SRI_BrakeForceCmd\n              sharing_group: brake-readers"),
            ("      unmapped_runnables:", "        Swc_Shadow:\n          arxml_short_name: CVC_Swc_Shadow\n          ports:\n            - name: RP_Shadow\n              direction: required\n              message: Brake_Command\n              signal: BrakeForceCmd\n              interface: SRI_BrakeForceCmd\n              sharing_group: brake-readers\n      unmapped_runnables:"),
        )
    assert ("SWCMAP014", "ecus.cvc.required.Brake_Command.BrakeForceCmd.safety_approval_ref") in _diagnostics(_invalid(tmp_path, *replacements))


def test_reviewable_safety_approval_allows_an_exception(tmp_path: Path):
    mapping = _mapping(
        tmp_path,
        ("            - name: RP_BrakeForceCmd\n              direction: required\n              message: Brake_Command\n              signal: BrakeForceCmd\n              interface: SRI_BrakeForceCmd\n", ""),
        ("      unmapped_runnables:", "      unmapped_signals:\n        - direction: required\n          message: Brake_Command\n          signal: BrakeForceCmd\n          disposition: architecture-exception\n          rationale: The owner approved a temporary migration exception.\n          safety_approval_ref: SAFETY-DECISION-001\n      unmapped_runnables:"),
    )
    validate_swc_mapping(mapping, DBC, MODEL)


def test_safety_approval_must_be_a_reviewable_decision_reference(tmp_path: Path):
    mapping = _mapping(
        tmp_path,
        ("            - name: RP_BrakeForceCmd\n              direction: required\n              message: Brake_Command\n              signal: BrakeForceCmd\n              interface: SRI_BrakeForceCmd\n", ""),
        ("      unmapped_runnables:", "      unmapped_signals:\n        - direction: required\n          message: Brake_Command\n          signal: BrakeForceCmd\n          disposition: architecture-exception\n          rationale: The owner approved a temporary migration exception.\n          safety_approval_ref: unreviewed\n      unmapped_runnables:"),
    )
    with pytest.raises(SwcMappingValidationError) as caught:
        validate_swc_mapping(mapping, DBC, MODEL)
    assert (
        "SWCMAP014",
        "ecus.cvc.required.Brake_Command.BrakeForceCmd.safety_approval_ref",
    ) in _diagnostics(caught.value)


def test_safety_relevance_is_unchanged_by_message_and_signal_renaming(tmp_path: Path):
    dbc = tmp_path / "renamed.dbc"
    dbc.write_text(
        DBC.read_text(encoding="utf-8")
        .replace("Brake_Command", "Neutral_Message")
        .replace("BrakeForceCmd", "NeutralSignal"),
        encoding="utf-8",
    )
    mapping = _mapping(
        tmp_path,
        ("message: Brake_Command", "message: Neutral_Message"),
        ("signal: BrakeForceCmd", "signal: NeutralSignal"),
        ("message: Brake_Command", "message: Neutral_Message"),
        ("signal: BrakeForceCmd", "signal: NeutralSignal"),
        ("            - name: RP_BrakeForceCmd\n              direction: required\n              message: Neutral_Message\n              signal: NeutralSignal\n              interface: SRI_BrakeForceCmd\n", ""),
        ("      unmapped_runnables:", "      unmapped_signals:\n        - direction: required\n          message: Neutral_Message\n          signal: NeutralSignal\n          disposition: architecture-exception\n          rationale: This renamed safety route is intentionally unmapped.\n      unmapped_runnables:"),
    )

    with pytest.raises(SwcMappingValidationError) as caught:
        validate_swc_mapping(mapping, dbc, MODEL)

    assert (
        "SWCMAP014",
        "ecus.cvc.required.Neutral_Message.NeutralSignal.safety_approval_ref",
    ) in _diagnostics(caught.value)


def test_structured_dbc_safety_marker_controls_approval_requirement(tmp_path: Path):
    dbc = tmp_path / "qm.dbc"
    dbc.write_text(
        DBC.read_text(encoding="utf-8").replace(
            'BA_ "ASIL" BO_ 101 4;', 'BA_ "ASIL" BO_ 101 0;'
        ),
        encoding="utf-8",
    )
    mapping = _mapping(
        tmp_path,
        ("            - name: RP_BrakeForceCmd\n              direction: required\n              message: Brake_Command\n              signal: BrakeForceCmd\n              interface: SRI_BrakeForceCmd\n", ""),
        ("      unmapped_runnables:", "      unmapped_signals:\n        - direction: required\n          message: Brake_Command\n          signal: BrakeForceCmd\n          disposition: architecture-exception\n          rationale: The structured DBC marker classifies this fixture as QM.\n      unmapped_runnables:"),
    )

    validate_swc_mapping(mapping, dbc, MODEL)


def test_approved_shared_safety_binding_with_one_writer_is_valid(tmp_path: Path):
    mapping = _mapping(
        tmp_path,
        (
            "              write_owner: true\n",
            "              write_owner: true\n"
            "              sharing_group: pedal-providers\n"
            "              safety_approval_ref: SAFETY-DECISION-001\n",
        ),
        (
            "      unmapped_runnables:",
            "        Swc_Shadow:\n"
            "          arxml_short_name: CVC_Swc_Shadow\n"
            "          ports:\n"
            "            - name: PP_Shadow\n"
            "              direction: provided\n"
            "              message: Torque_Request\n"
            "              signal: PedalPosition\n"
            "              interface: SRI_PedalPosition\n"
            "              sharing_group: pedal-providers\n"
            "              safety_approval_ref: SAFETY-DECISION-001\n"
            "      unmapped_runnables:",
        ),
    )
    validate_swc_mapping(mapping, DBC, MODEL)


def test_signal_sets_expand_exactly_but_do_not_grant_coverage(tmp_path: Path):
    mapping = _mapping(
        tmp_path,
        ("            - name: RP_BrakeForceCmd\n              direction: required\n              message: Brake_Command\n              signal: BrakeForceCmd\n              interface: SRI_BrakeForceCmd\n", ""),
        ("      unmapped_runnables:", "      unmapped_signal_sets:\n        - direction: required\n          messages: [Brake_Command]\n          disposition: migration-review\n          rationale: Expansion is review output, not a coverage waiver.\n          safety_approval_ref: SAFETY-DECISION-001\n      unmapped_runnables:"),
    )
    with pytest.raises(SwcMappingValidationError) as caught:
        validate_swc_mapping(mapping, DBC, MODEL)
    assert caught.value.expanded_unmapped_signals[0].identity.signal == "BrakeForceCmd"
    assert ("SWCMAP011", "ecus.cvc.coverage.required.Brake_Command.BrakeForceCmd") in _diagnostics(caught.value)


@pytest.mark.parametrize("filename", ["TaktflowSystem.arxml", "TaktflowSystem.arxml.assumptions.md", "mapping.parity.json"])
@pytest.mark.parametrize("preexisting", [False, True])
def test_validation_failure_never_creates_or_replaces_any_output_class(tmp_path: Path, filename: str, preexisting: bool):
    target = tmp_path / filename
    original = b"existing-output\n"
    if preexisting:
        target.write_bytes(original)
    invalid = _mapping(tmp_path, ("message: Torque_Request", "message: Unknown_Message"))

    with pytest.raises(SwcMappingValidationError):
        validate_before_publish(invalid, DBC, MODEL, lambda: target.write_bytes(b"replacement\n"))

    assert target.read_bytes() == original if preexisting else not target.exists()


def test_diagnostics_are_deterministic_object_addressed_and_path_scrubbed(tmp_path: Path):
    mapping = _mapping(
        tmp_path,
        ("message: Torque_Request", "message: Unknown_Message"),
        ("signal: BrakeForceCmd", "signal: UnknownSignal"),
    )
    messages = []
    for _ in range(2):
        with pytest.raises(SwcMappingValidationError) as caught:
            validate_swc_mapping(mapping, DBC, MODEL)
        messages.append(str(caught.value))

    assert messages[0] == messages[1]
    assert str(tmp_path) not in messages[0]
    assert "SWCMAP008 ecus.cvc.swcs.Swc_Pedal.ports" in messages[0]


def test_malformed_model_fails_without_leaking_paths(tmp_path: Path):
    model = tmp_path / "private-model.json"
    model.write_text(json.dumps({"ecus": []}), encoding="utf-8")
    with pytest.raises(SwcMappingValidationError) as caught:
        validate_swc_mapping(load_swc_mapping(FIXTURES / "validation_valid.yaml"), DBC, model)
    assert caught.value.diagnostics[0].code == "SWCMAP008"
    assert str(tmp_path) not in str(caught.value)


@pytest.mark.parametrize(
    ("duplicate", "address"),
    [
        ("swc", "input.swc_model.ecus.cvc.swcs.Swc_Pedal"),
        ("runnable", "input.swc_model.ecus.cvc.runnables.Swc_Pedal_Init"),
    ],
)
def test_extracted_identities_must_resolve_exactly_once(
    tmp_path: Path, duplicate: str, address: str
):
    raw = json.loads(MODEL.read_text(encoding="utf-8"))
    pedal = raw["ecus"]["cvc"]["swcs"][0]
    if duplicate == "swc":
        raw["ecus"]["cvc"]["swcs"].append(dict(pedal))
    else:
        pedal["functions"].append("Swc_Pedal_Init")
    model = tmp_path / "model.json"
    model.write_text(json.dumps(raw), encoding="utf-8")

    with pytest.raises(SwcMappingValidationError) as caught:
        validate_swc_mapping(
            load_swc_mapping(FIXTURES / "validation_valid.yaml"), DBC, model
        )

    assert ("SWCMAP008", address) in _diagnostics(caught.value)
