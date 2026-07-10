from __future__ import annotations

import json
import subprocess
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path

import cantools

from tools.arxml.swc_mapping import load_swc_mapping
from tools.ci.check_swc_mapping import compare_parity_snapshots


ROOT = Path(__file__).resolve().parents[3]
CHECKER = ROOT / "tools" / "ci" / "check_swc_mapping.py"
DBC = ROOT / "gateway" / "taktflow_vehicle.dbc"
SWC_MODEL = ROOT / "arxml_v2" / "swc_model.json"
ARXML = ROOT / "arxml" / "TaktflowSystem.arxml"
MAPPING = ROOT / "model" / "ecu_sidecar.yaml"
GOLDEN = (
    ROOT
    / "tools"
    / "arxml"
    / "test"
    / "golden"
    / "swc_mapping"
    / "legacy_inventory.json"
)
SHADOW_GOLDEN = GOLDEN.with_name("shadow_parity.json")
PREFERRED_GOLDEN = GOLDEN.with_name("preferred_parity.json")


def run_checker(arxml: Path, output: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(CHECKER),
            str(DBC),
            str(SWC_MODEL),
            str(arxml),
            "--output",
            str(output),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def test_legacy_inventory_matches_golden_and_is_path_scrubbed(tmp_path):
    first = tmp_path / "first" / "inventory.json"
    second = tmp_path / "second" / "inventory.json"

    first_result = run_checker(ARXML, first)
    second_result = run_checker(ARXML, second)

    assert first_result.returncode == 0, first_result.stderr
    assert second_result.returncode == 0, second_result.stderr
    assert first.read_bytes() == second.read_bytes() == GOLDEN.read_bytes()

    raw = first.read_text(encoding="utf-8")
    inventory = json.loads(raw)
    assert list(inventory["ecus"]) == ["BCM", "CVC", "FZC", "ICU", "RZC", "SC", "TCU"]
    assert inventory["summary"] == {
        "arxml_events": 71,
        "arxml_ports": 848,
        "arxml_runnables": 71,
        "arxml_swcs": 48,
        "arxml_interfaces": 182,
        "dbc_required_routes": 386,
        "dbc_provided_routes": 173,
        "extracted_functions": 140,
        "extracted_swcs": 48,
    }
    assert inventory["ecus"]["SC"]["unmapped_counts"] == {
        "provided": 8,
        "required": 67,
    }
    assert inventory["ecus"]["ICU"]["unmapped_counts"] == {
        "provided": 5,
        "required": 97,
    }
    assert inventory["ecus"]["TCU"]["unmapped_counts"] == {
        "provided": 5,
        "required": 3,
    }
    assert str(ROOT) not in raw
    assert "\\\\" not in raw


def test_legacy_inventory_fails_on_unmapped_baseline_drift(tmp_path):
    tree = ET.parse(ARXML)
    root = tree.getroot()
    namespace = root.tag.split("}", 1)[0].removeprefix("{")
    ns = {"a": namespace}
    icu = next(
        swc
        for swc in root.findall(".//a:APPLICATION-SW-COMPONENT-TYPE", ns)
        if (swc.findtext("a:SHORT-NAME", namespaces=ns) or "").startswith("ICU_")
        and swc.find("a:PORTS/a:R-PORT-PROTOTYPE", ns) is not None
    )
    ports = icu.find("a:PORTS", ns)
    assert ports is not None
    required = ports.find("a:R-PORT-PROTOTYPE", ns)
    assert required is not None
    ports.remove(required)
    mutated = tmp_path / "mutated.arxml"
    tree.write(mutated, encoding="utf-8", xml_declaration=True)

    result = run_checker(mutated, tmp_path / "inventory.json")

    assert result.returncode == 1
    assert result.stdout == ""
    assert (
        "legacy inventory drift: ICU unmapped required routes: expected 97, actual 98"
        in result.stderr
    )
    assert not (tmp_path / "inventory.json").exists()


def test_shadow_parity_reports_four_exact_differences_deterministically():
    legacy = {
        "swcs": {"cvc/Swc_Pedal": "CVC_Swc_Pedal"},
        "ports": {
            "cvc/Swc_Pedal/provided/Torque_Request/PedalPosition": "PP_PedalPosition"
        },
        "interfaces": {
            "cvc/Swc_Pedal/provided/Torque_Request/PedalPosition": "SRI_PedalPosition"
        },
        "runnables": {
            "cvc/Swc_Pedal/Swc_Pedal_Init": "Swc_Pedal_Init"
        },
        "events": {
            "cvc/Swc_Pedal/Swc_Pedal_Init": {
                "name": "IE_Swc_Pedal_Init",
                "type": "init",
            }
        },
    }
    explicit = {
        "swcs": {"cvc/Swc_Pedal": "CVC_Swc_Pedal"},
        "ports": {
            "cvc/Swc_Pedal/provided/Torque_Request/PedalPosition": "PP_Changed"
        },
        "interfaces": {
            "cvc/Swc_Pedal/provided/Torque_Request/PedalPosition": "SRI_Changed"
        },
        "runnables": {},
        "events": {
            "cvc/Swc_Pedal/Swc_Pedal_Init": {
                "name": "TE_Swc_Pedal_Init_10ms",
                "period_seconds": "0.01",
                "type": "timing",
            }
        },
    }

    first = compare_parity_snapshots(legacy, explicit)
    second = compare_parity_snapshots(legacy, explicit)

    assert first == second == (
        {
            "category": "event",
            "object": "cvc/Swc_Pedal/Swc_Pedal_Init",
            "legacy": {"name": "IE_Swc_Pedal_Init", "type": "init"},
            "explicit": {
                "name": "TE_Swc_Pedal_Init_10ms",
                "period_seconds": "0.01",
                "type": "timing",
            },
        },
        {
            "category": "interface",
            "object": "cvc/Swc_Pedal/provided/Torque_Request/PedalPosition",
            "legacy": "SRI_PedalPosition",
            "explicit": "SRI_Changed",
        },
        {
            "category": "port",
            "object": "cvc/Swc_Pedal/provided/Torque_Request/PedalPosition",
            "legacy": "PP_PedalPosition",
            "explicit": "PP_Changed",
        },
        {
            "category": "runnable",
            "object": "cvc/Swc_Pedal/Swc_Pedal_Init",
            "legacy": "Swc_Pedal_Init",
            "explicit": None,
        },
    )


def run_shadow_checker(
    mapping: Path, output: Path
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(CHECKER),
            str(DBC),
            str(SWC_MODEL),
            str(mapping),
            str(ARXML),
            "--mode",
            "shadow",
            "--output",
            str(output),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def run_preferred_checker(output: Path, check: bool = False):
    command = [
        sys.executable,
        str(CHECKER),
        str(DBC),
        str(SWC_MODEL),
        str(MAPPING),
        str(ARXML),
        "--mode",
        "preferred",
    ]
    command.extend(["--check"] if check else ["--output", str(output)])
    return subprocess.run(
        command, cwd=ROOT, text=True, capture_output=True, check=False
    )


def test_project_preferred_report_has_distinct_repeatable_golden(tmp_path: Path):
    first = tmp_path / "first.json"
    second = tmp_path / "second.json"

    first_result = run_preferred_checker(first)
    second_result = run_preferred_checker(second)
    check_result = run_preferred_checker(tmp_path / "unused.json", check=True)

    assert first_result.returncode == second_result.returncode == 0
    assert check_result.returncode == 0, check_result.stderr
    assert first.read_bytes() == second.read_bytes() == PREFERRED_GOLDEN.read_bytes()
    report = json.loads(first.read_text(encoding="utf-8"))
    assert report["mode"] == "preferred"
    assert report["summary"]["differences"] == 0


def test_project_shadow_report_is_exact_repeatable_and_path_scrubbed(tmp_path: Path):
    first = tmp_path / "first" / "shadow.json"
    second = tmp_path / "second" / "shadow.json"

    first_result = run_shadow_checker(MAPPING, first)
    second_result = run_shadow_checker(MAPPING, second)

    assert first_result.returncode == 0, first_result.stderr
    assert second_result.returncode == 0, second_result.stderr
    assert first.read_bytes() == second.read_bytes() == SHADOW_GOLDEN.read_bytes()
    report = json.loads(first.read_text(encoding="utf-8"))
    assert report["summary"] == {
        "differences": 0,
        "events": 71,
        "interfaces": 182,
        "mapped_signal_identities": 374,
        "ports": 848,
        "runnables": 71,
        "swcs": 48,
        "unmapped_runnables": 69,
        "unmapped_signals": 185,
    }
    raw = first.read_text(encoding="utf-8")
    assert str(ROOT) not in raw
    assert "\\\\" not in raw


def test_project_mapping_has_exact_coverage_and_reviewed_shared_writers():
    mapping = load_swc_mapping(MAPPING)
    database = cantools.database.load_file(str(DBC))
    attributes = {
        (message.name, signal.name): {
            name: value.value if hasattr(value, "value") else value
            for name, value in (
                getattr(getattr(signal, "dbc", None), "attributes", {}) or {}
            ).items()
        }
        for message in database.messages
        for signal in message.signals
    }

    assert len(mapping.ecus) == 7
    assert sum(len(ecu.swcs) for ecu in mapping.ecus) == 48
    assert sum(len(swc.ports) for ecu in mapping.ecus for swc in ecu.swcs) == 848
    assert sum(len(swc.runnables) for ecu in mapping.ecus for swc in ecu.swcs) == 71
    assert sum(len(ecu.unmapped_signals) for ecu in mapping.ecus) == 185
    assert sum(len(ecu.unmapped_runnables) for ecu in mapping.ecus) == 69
    assert not any(ecu.unmapped_signal_sets for ecu in mapping.ecus)

    shared_provided = 0
    for ecu in mapping.ecus:
        bindings = defaultdict(list)
        for swc in ecu.swcs:
            for port in swc.ports:
                bindings[(port.direction, port.identity)].append((swc.name, port))
        for (direction, identity), rows in bindings.items():
            if direction != "provided" or len(rows) < 2:
                continue
            shared_provided += 1
            candidates = [swc for swc, _ in rows]
            domain = [
                swc
                for swc in candidates
                if not any(
                    token in swc.lower() for token in ("com", "canmonitor", "main")
                )
            ]
            signal_attributes = attributes[(identity.message, identity.signal)]
            owner = signal_attributes.get("Owner")
            produced_by = signal_attributes.get("ProducedBy")
            if len(domain) == 1:
                expected = domain[0]
            elif owner in candidates:
                expected = owner
            elif produced_by in candidates:
                expected = produced_by
            else:
                com = [
                    swc
                    for swc in candidates
                    if "com" in swc.lower() and "canmonitor" not in swc.lower()
                ]
                observers = [
                    swc for swc in candidates if "canmonitor" in swc.lower()
                ]
                assert len(com) == 1
                assert len(com) + len(observers) == len(candidates)
                expected = com[0]
            actual = [swc for swc, port in rows if port.write_owner is True]
            assert actual == [expected]
            assert all(port.sharing_group for _, port in rows)

    assert shared_provided == 147


def test_shadow_validation_failure_does_not_replace_parity_report(tmp_path: Path):
    invalid = tmp_path / "invalid.yaml"
    text = MAPPING.read_text(encoding="utf-8")
    invalid.write_text(
        text.replace(
            "signal: Body_Control_Cmd_DoorLockCmd", "signal: UnknownSignal", 1
        ),
        encoding="utf-8",
    )
    report = tmp_path / "shadow.json"
    report.write_bytes(b"existing-parity\n")

    result = run_shadow_checker(invalid, report)

    assert result.returncode == 1
    assert "SWCMAP008" in result.stderr
    assert report.read_bytes() == b"existing-parity\n"
