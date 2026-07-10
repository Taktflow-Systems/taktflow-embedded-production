from __future__ import annotations

import json
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
CHECKER = ROOT / "tools" / "ci" / "check_swc_mapping.py"
DBC = ROOT / "gateway" / "taktflow_vehicle.dbc"
SWC_MODEL = ROOT / "arxml_v2" / "swc_model.json"
ARXML = ROOT / "arxml" / "TaktflowSystem.arxml"
GOLDEN = (
    ROOT
    / "tools"
    / "arxml"
    / "test"
    / "golden"
    / "swc_mapping"
    / "legacy_inventory.json"
)


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
