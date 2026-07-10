#!/usr/bin/env python3
"""Check normalized inventory and strict explicit SWC mapping parity."""

from __future__ import annotations

import argparse
import json
import os
import sys
import tempfile
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path
from typing import Any

import cantools

TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from arxml.swc_mapping import (
    ExplicitSwcMapping,
    SwcMappingError,
    SwcMappingValidationError,
    load_swc_mapping,
    validate_swc_mapping,
)


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
DEFAULT_STRICT_GOLDEN = DEFAULT_GOLDEN.with_name("strict_parity.json")


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


def _legacy_parity_snapshot(inventory: dict[str, Any]) -> dict[str, dict[str, Any]]:
    snapshot: dict[str, dict[str, Any]] = {
        name: {} for name in ("swcs", "ports", "interfaces", "runnables", "events")
    }
    for ecu, entry in inventory["ecus"].items():
        for swc in entry["generated_swcs"]:
            source_name = swc["name"].removeprefix(f"{ecu}_")
            swc_key = f"{ecu.lower()}/{source_name}"
            snapshot["swcs"][swc_key] = swc["name"]
            for port in swc["ports"]:
                key = "/".join(
                    (swc_key, port["direction"], port["message"], port["signal"])
                )
                snapshot["ports"][key] = port["name"]
                snapshot["interfaces"][key] = port["interface"]
            for runnable in swc["runnables"]:
                key = f"{swc_key}/{runnable}"
                snapshot["runnables"][key] = runnable
            for event in swc["events"]:
                key = f"{swc_key}/{event['runnable']}"
                snapshot["events"][key] = {
                    field: event[field]
                    for field in ("name", "period_seconds", "type")
                    if field in event
                }
    return snapshot


def _explicit_parity_snapshot(mapping: ExplicitSwcMapping) -> dict[str, dict[str, Any]]:
    snapshot: dict[str, dict[str, Any]] = {
        name: {} for name in ("swcs", "ports", "interfaces", "runnables", "events")
    }
    for ecu in mapping.ecus:
        for swc in ecu.swcs:
            swc_key = f"{ecu.name.lower()}/{swc.name}"
            snapshot["swcs"][swc_key] = swc.arxml_short_name
            for port in swc.ports:
                key = "/".join(
                    (
                        swc_key,
                        port.direction,
                        port.identity.message,
                        port.identity.signal,
                    )
                )
                snapshot["ports"][key] = port.name
                snapshot["interfaces"][key] = port.interface
            for runnable in swc.runnables:
                key = f"{swc_key}/{runnable.function}"
                snapshot["runnables"][key] = runnable.function
                if runnable.trigger == "init":
                    event = {"name": f"IE_{runnable.function}", "type": "init"}
                elif runnable.trigger == "periodic":
                    event = {
                        "name": f"TE_{runnable.function}_{runnable.period_ms}ms",
                        "period_seconds": str(runnable.period_ms / 1000.0),
                        "type": "timing",
                    }
                else:
                    event = {
                        "name": f"DE_{runnable.function}",
                        "type": "data-received",
                    }
                snapshot["events"][key] = event
    return snapshot


def compare_parity_snapshots(
    legacy: dict[str, dict[str, Any]],
    explicit: dict[str, dict[str, Any]],
) -> tuple[dict[str, Any], ...]:
    """Return one stable difference per normalized semantic object."""

    singular = {
        "events": "event",
        "interfaces": "interface",
        "ports": "port",
        "runnables": "runnable",
        "swcs": "swc",
    }
    differences: list[dict[str, Any]] = []
    for category in sorted(singular):
        legacy_rows = legacy.get(category, {})
        explicit_rows = explicit.get(category, {})
        for key in sorted(set(legacy_rows) | set(explicit_rows)):
            legacy_value = legacy_rows.get(key)
            explicit_value = explicit_rows.get(key)
            if legacy_value != explicit_value:
                differences.append(
                    {
                        "category": singular[category],
                        "object": key,
                        "legacy": legacy_value,
                        "explicit": explicit_value,
                    }
                )
    return tuple(differences)


def build_mapping_report(
    dbc_path: Path,
    model_path: Path,
    mapping_path: Path,
    arxml_path: Path,
    mode: str = "strict",
) -> dict[str, Any]:
    mapping = load_swc_mapping(mapping_path)
    result = validate_swc_mapping(mapping, dbc_path, model_path, governed_ecus=ECUS)
    if result.expanded_unmapped_signals:
        raise InventoryError(
            "SWCMAP016 strict parity: migration-only unmapped signal sets are forbidden"
        )
    inventory = build_inventory(dbc_path, model_path, arxml_path)
    legacy = _legacy_parity_snapshot(inventory)
    explicit = _explicit_parity_snapshot(mapping)
    differences = compare_parity_snapshots(legacy, explicit)
    coverage: list[dict[str, Any]] = []
    unmapped_signals: list[dict[str, Any]] = []
    for ecu in mapping.ecus:
        mapped = {
            (port.direction, port.identity)
            for swc in ecu.swcs
            for port in swc.ports
        }
        coverage.append(
            {
                "ecu": ecu.name.upper(),
                "swcs": len(ecu.swcs),
                "port_bindings": sum(len(swc.ports) for swc in ecu.swcs),
                "mapped_provided_signals": sum(
                    direction == "provided" for direction, _ in mapped
                ),
                "mapped_required_signals": sum(
                    direction == "required" for direction, _ in mapped
                ),
                "unmapped_provided_signals": sum(
                    item.direction == "provided" for item in ecu.unmapped_signals
                ),
                "unmapped_required_signals": sum(
                    item.direction == "required" for item in ecu.unmapped_signals
                ),
                "mapped_runnables": sum(len(swc.runnables) for swc in ecu.swcs),
                "unmapped_runnables": len(ecu.unmapped_runnables),
            }
        )
        for item in ecu.unmapped_signals:
            row = {
                "ecu": ecu.name.upper(),
                "direction": item.direction,
                "message": item.identity.message,
                "signal": item.identity.signal,
                "disposition": item.disposition,
            }
            if item.safety_approval_ref is not None:
                row["safety_approval_ref"] = item.safety_approval_ref
            unmapped_signals.append(row)
    return {
        "schema": "swc-mapping-%s-parity-v1" % mode,
        "mode": mode,
        "inputs": {
            "dbc": dbc_path.name,
            "swc_model": model_path.name,
            "swc_mapping": mapping_path.name,
            "arxml": arxml_path.name,
        },
        "summary": {
            "swcs": len(legacy["swcs"]),
            "ports": len(legacy["ports"]),
            "interfaces": inventory["summary"]["arxml_interfaces"],
            "runnables": len(legacy["runnables"]),
            "events": len(legacy["events"]),
            "mapped_signal_identities": sum(
                row["mapped_provided_signals"] + row["mapped_required_signals"]
                for row in coverage
            ),
            "unmapped_signals": len(unmapped_signals),
            "unmapped_runnables": sum(row["unmapped_runnables"] for row in coverage),
            "differences": len(differences),
        },
        "coverage": coverage,
        "unmapped_signals": unmapped_signals,
        "differences": list(differences),
    }


def _write_atomic(path: Path, content: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            prefix=f".{path.name}.",
            suffix=".tmp",
            dir=path.parent,
            delete=False,
        ) as stream:
            temporary = Path(stream.name)
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dbc", type=Path)
    parser.add_argument("swc_model", type=Path)
    parser.add_argument("mapping_or_arxml", type=Path)
    parser.add_argument("arxml", nargs="?", type=Path)
    parser.add_argument("--mode", choices=["strict"])
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--check",
        nargs="?",
        const="__default__",
        help="compare with a golden (default depends on selected mode)",
    )
    args = parser.parse_args()
    if args.output is None and args.check is None:
        parser.error("one of --output or --check is required")

    try:
        if args.mode == "strict":
            if args.arxml is None:
                parser.error("strict mode requires SWC mapping and ARXML positional inputs")
            report = build_mapping_report(
                args.dbc,
                args.swc_model,
                args.mapping_or_arxml,
                args.arxml,
                mode=args.mode,
            )
            default_golden = DEFAULT_STRICT_GOLDEN
        else:
            if args.arxml is not None:
                parser.error("the fourth positional input requires a mapping mode")
            report = build_inventory(args.dbc, args.swc_model, args.mapping_or_arxml)
            default_golden = DEFAULT_GOLDEN
        rendered = render_inventory(report)
        if args.check is not None:
            check_path = (
                default_golden if args.check == "__default__" else Path(args.check)
            )
            expected = check_path.read_bytes()
            if rendered != expected:
                raise InventoryError(
                    f"SWC mapping report drift: generated report differs from '{check_path.name}'"
                )
        if args.output is not None:
            _write_atomic(args.output, rendered)
    except (
        InventoryError,
        OSError,
        ET.ParseError,
        json.JSONDecodeError,
        SwcMappingError,
        SwcMappingValidationError,
    ) as exc:
        print(str(exc), file=sys.stderr)
        return 1

    summary = json.loads(rendered)["summary"]
    if args.mode == "strict":
        print(
            "SWC mapping %s parity passed: " % args.mode +
            f"{summary['swcs']} SWCs, {summary['ports']} ports, "
            f"{summary['runnables']} runnables, {summary['events']} events, "
            f"{summary['differences']} differences."
        )
    else:
        print(
            "Legacy SWC mapping inventory passed: "
            f"{summary['arxml_swcs']} SWCs, {summary['arxml_ports']} ports, "
            f"{summary['arxml_runnables']} runnables, {summary['arxml_events']} events."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
