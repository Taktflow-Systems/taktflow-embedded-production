#!/usr/bin/env python3
"""Freeze the normalized legacy Signal-to-SWC inventory."""

from __future__ import annotations

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path
from typing import Any

import cantools


ECUS = ("BCM", "CVC", "FZC", "ICU", "RZC", "SC", "TCU")
EXPECTED_SWC_COUNT = 48
EXPECTED_UNMAPPED = {
    "ICU": {"provided": 5, "required": 97},
    "SC": {"provided": 8, "required": 67},
    "TCU": {"provided": 5, "required": 3},
}
DEFAULT_GOLDEN = (
    Path(__file__).resolve().parents[1]
    / "arxml"
    / "test"
    / "golden"
    / "swc_mapping"
    / "legacy_inventory.json"
)


class InventoryError(Exception):
    """A stable, path-scrubbed legacy inventory failure."""


def _safe_name(name: str) -> str:
    return name.replace(" ", "_").replace("-", "_")


def _short_name(element: ET.Element, ns: dict[str, str]) -> str:
    return element.findtext("a:SHORT-NAME", default="", namespaces=ns)


def _ref_name(element: ET.Element, tag: str, ns: dict[str, str]) -> str:
    value = element.findtext(f"a:{tag}", default="", namespaces=ns)
    return value.rsplit("/", 1)[-1]


def _route(message: str, signal: str) -> dict[str, str]:
    return {"message": message, "signal": signal}


def _route_key(route: dict[str, str]) -> tuple[str, str]:
    return route["message"], route["signal"]


def _load_routes(
    dbc_path: Path,
) -> tuple[dict[str, dict[str, list[dict[str, str]]]], dict[str, tuple[str, str]]]:
    database = cantools.database.load_file(str(dbc_path))
    routes = {
        ecu: {"provided": [], "required": []}
        for ecu in ECUS
    }
    interfaces: dict[str, tuple[str, str]] = {}
    for message in sorted(database.messages, key=lambda item: item.name):
        senders = set(message.senders)
        for signal in sorted(message.signals, key=lambda item: item.name):
            identity = (message.name, signal.name)
            interface = f"SRI_{_safe_name(signal.name)}"
            previous = interfaces.get(interface)
            if previous is not None and previous != identity:
                raise InventoryError(
                    f"legacy inventory ambiguity: interface '{interface}' resolves to "
                    f"both {previous!r} and {identity!r}"
                )
            interfaces[interface] = identity
            for ecu in ECUS:
                if ecu in senders:
                    routes[ecu]["provided"].append(_route(*identity))
                if ecu in signal.receivers:
                    routes[ecu]["required"].append(_route(*identity))
    return routes, interfaces


def _load_extracted_model(model_path: Path) -> dict[str, list[dict[str, Any]]]:
    with model_path.open(encoding="utf-8") as stream:
        raw = json.load(stream)
    raw_ecus = raw.get("ecus")
    if not isinstance(raw_ecus, dict):
        raise InventoryError("legacy inventory input: SWC model has no 'ecus' object")

    result: dict[str, list[dict[str, Any]]] = {}
    for ecu in ECUS:
        entry = raw_ecus.get(ecu.lower(), {})
        swcs = entry.get("swcs", []) if isinstance(entry, dict) else []
        result[ecu] = sorted(
            (
                {
                    "name": str(swc["name"]),
                    "functions": sorted(str(value) for value in swc.get("functions", [])),
                }
                for swc in swcs
            ),
            key=lambda item: item["name"],
        )
    return result


def _load_arxml(
    arxml_path: Path,
    interface_identities: dict[str, tuple[str, str]],
) -> tuple[dict[str, list[dict[str, Any]]], list[dict[str, str]]]:
    root = ET.parse(arxml_path).getroot()
    namespace = root.tag.split("}", 1)[0].removeprefix("{")
    ns = {"a": namespace}
    result: dict[str, list[dict[str, Any]]] = {ecu: [] for ecu in ECUS}

    interface_rows: list[dict[str, str]] = []
    for interface in root.findall(".//a:SENDER-RECEIVER-INTERFACE", ns):
        name = _short_name(interface, ns)
        identity = interface_identities.get(name)
        if identity is None:
            raise InventoryError(
                f"legacy inventory reference: interface '{name}' has no exact DBC signal identity"
            )
        interface_rows.append(
            {"name": name, "message": identity[0], "signal": identity[1]}
        )
    interface_rows.sort(key=lambda item: (item["name"], item["message"], item["signal"]))

    for swc in root.findall(".//a:APPLICATION-SW-COMPONENT-TYPE", ns):
        swc_name = _short_name(swc, ns)
        ecu = swc_name.split("_", 1)[0]
        if ecu not in result:
            continue
        ports: list[dict[str, str]] = []
        for direction, tag, ref_tag in (
            ("provided", "P-PORT-PROTOTYPE", "PROVIDED-INTERFACE-TREF"),
            ("required", "R-PORT-PROTOTYPE", "REQUIRED-INTERFACE-TREF"),
        ):
            for port in swc.findall(f"a:PORTS/a:{tag}", ns):
                interface = _ref_name(port, ref_tag, ns)
                identity = interface_identities.get(interface)
                if identity is None:
                    raise InventoryError(
                        f"legacy inventory reference: {swc_name} port "
                        f"'{_short_name(port, ns)}' uses unknown interface '{interface}'"
                    )
                ports.append(
                    {
                        "direction": direction,
                        "name": _short_name(port, ns),
                        "interface": interface,
                        "message": identity[0],
                        "signal": identity[1],
                    }
                )
        ports.sort(
            key=lambda item: (
                item["direction"], item["message"], item["signal"], item["name"]
            )
        )

        runnables = sorted(
            _short_name(item, ns)
            for item in swc.findall(".//a:RUNNABLE-ENTITY", ns)
        )
        events: list[dict[str, str]] = []
        for kind, tag in (("init", "INIT-EVENT"), ("timing", "TIMING-EVENT")):
            for event in swc.findall(f".//a:{tag}", ns):
                row = {
                    "type": kind,
                    "name": _short_name(event, ns),
                    "runnable": _ref_name(event, "START-ON-EVENT-REF", ns),
                }
                if kind == "timing":
                    row["period_seconds"] = event.findtext(
                        "a:PERIOD", default="", namespaces=ns
                    )
                events.append(row)
        events.sort(key=lambda item: (item["type"], item["name"], item["runnable"]))
        result[ecu].append(
            {
                "name": swc_name,
                "ports": ports,
                "runnables": runnables,
                "events": events,
            }
        )

    for swcs in result.values():
        swcs.sort(key=lambda item: item["name"])
    return result, interface_rows


def _reconcile_swcs(
    ecu: str,
    extracted: list[dict[str, Any]],
    generated: list[dict[str, Any]],
) -> dict[str, Any]:
    extracted_by_name = {item["name"]: item for item in extracted}
    generated_by_source_name = {
        item["name"].removeprefix(f"{ecu}_"): item for item in generated
    }
    missing = sorted(set(extracted_by_name) - set(generated_by_source_name))
    unexpected = sorted(set(generated_by_source_name) - set(extracted_by_name))
    function_differences = []
    for name in sorted(set(extracted_by_name) & set(generated_by_source_name)):
        source = set(extracted_by_name[name]["functions"])
        emitted = set(generated_by_source_name[name]["runnables"])
        not_emitted = sorted(source - emitted)
        not_extracted = sorted(emitted - source)
        if not_emitted or not_extracted:
            function_differences.append(
                {
                    "swc": name,
                    "extracted_not_emitted": not_emitted,
                    "emitted_not_extracted": not_extracted,
                }
            )
    return {
        "missing_generated_swcs": missing,
        "unexpected_generated_swcs": unexpected,
        "function_differences": function_differences,
    }


def build_inventory(dbc_path: Path, model_path: Path, arxml_path: Path) -> dict[str, Any]:
    routes, interface_identities = _load_routes(dbc_path)
    extracted = _load_extracted_model(model_path)
    generated, interfaces = _load_arxml(arxml_path, interface_identities)
    ecus: dict[str, Any] = {}

    for ecu in ECUS:
        bound: dict[str, set[tuple[str, str]]] = defaultdict(set)
        for swc in generated[ecu]:
            for port in swc["ports"]:
                bound[port["direction"]].add((port["message"], port["signal"]))

        unmapped: dict[str, list[dict[str, str]]] = {}
        unexpected: dict[str, list[dict[str, str]]] = {}
        for direction in ("provided", "required"):
            routed = {_route_key(item) for item in routes[ecu][direction]}
            unmapped[direction] = [_route(*item) for item in sorted(routed - bound[direction])]
            unexpected[direction] = [_route(*item) for item in sorted(bound[direction] - routed)]

        ecus[ecu] = {
            "route_counts": {
                "provided": len(routes[ecu]["provided"]),
                "required": len(routes[ecu]["required"]),
            },
            "unmapped_counts": {
                "provided": len(unmapped["provided"]),
                "required": len(unmapped["required"]),
            },
            "dbc_routes": routes[ecu],
            "unmapped_routes": unmapped,
            "unexpected_port_bindings": unexpected,
            "extracted_swcs": extracted[ecu],
            "generated_swcs": generated[ecu],
            "reconciliation": _reconcile_swcs(ecu, extracted[ecu], generated[ecu]),
        }

    inventory = {
        "schema": "legacy-swc-mapping-inventory-v1",
        "inputs": {
            "dbc": dbc_path.name,
            "swc_model": model_path.name,
            "arxml": arxml_path.name,
        },
        "summary": {
            "dbc_provided_routes": sum(item["route_counts"]["provided"] for item in ecus.values()),
            "dbc_required_routes": sum(item["route_counts"]["required"] for item in ecus.values()),
            "extracted_swcs": sum(len(item["extracted_swcs"]) for item in ecus.values()),
            "extracted_functions": sum(
                len(swc["functions"])
                for item in ecus.values()
                for swc in item["extracted_swcs"]
            ),
            "arxml_swcs": sum(len(item["generated_swcs"]) for item in ecus.values()),
            "arxml_ports": sum(
                len(swc["ports"])
                for item in ecus.values()
                for swc in item["generated_swcs"]
            ),
            "arxml_interfaces": len(interfaces),
            "arxml_runnables": sum(
                len(swc["runnables"])
                for item in ecus.values()
                for swc in item["generated_swcs"]
            ),
            "arxml_events": sum(
                len(swc["events"])
                for item in ecus.values()
                for swc in item["generated_swcs"]
            ),
        },
        "arxml_interfaces": interfaces,
        "ecus": ecus,
    }
    _validate_baseline(inventory)
    return inventory


def _validate_baseline(inventory: dict[str, Any]) -> None:
    summary = inventory["summary"]
    for label in ("extracted_swcs", "arxml_swcs"):
        actual = summary[label]
        if actual != EXPECTED_SWC_COUNT:
            raise InventoryError(
                f"legacy inventory drift: {label.replace('_', ' ')}: "
                f"expected {EXPECTED_SWC_COUNT}, actual {actual}"
            )
    for ecu, directions in EXPECTED_UNMAPPED.items():
        for direction, expected in directions.items():
            actual = inventory["ecus"][ecu]["unmapped_counts"][direction]
            if actual != expected:
                raise InventoryError(
                    f"legacy inventory drift: {ecu} unmapped {direction} routes: "
                    f"expected {expected}, actual {actual}"
                )


def render_inventory(inventory: dict[str, Any]) -> bytes:
    return (json.dumps(inventory, indent=2, ensure_ascii=True) + "\n").encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dbc", type=Path)
    parser.add_argument("swc_model", type=Path)
    parser.add_argument("arxml", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--check",
        nargs="?",
        type=Path,
        const=DEFAULT_GOLDEN,
        help="compare with a golden (default: committed legacy inventory)",
    )
    args = parser.parse_args()
    if args.output is None and args.check is None:
        parser.error("one of --output or --check is required")

    try:
        rendered = render_inventory(build_inventory(args.dbc, args.swc_model, args.arxml))
        if args.check is not None:
            expected = args.check.read_bytes()
            if rendered != expected:
                raise InventoryError(
                    f"legacy inventory drift: generated report differs from '{args.check.name}'"
                )
        if args.output is not None:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(rendered)
    except (InventoryError, OSError, ET.ParseError, json.JSONDecodeError) as exc:
        print(str(exc), file=sys.stderr)
        return 1

    summary = json.loads(rendered)["summary"]
    print(
        "Legacy SWC mapping inventory passed: "
        f"{summary['arxml_swcs']} SWCs, {summary['arxml_ports']} ports, "
        f"{summary['arxml_runnables']} runnables, {summary['arxml_events']} events."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
