"""Typed, disconnected parser for explicit signal-to-SWC mappings.

This module deliberately has no converter or emitter integration.  Callers must
supply the sidecar path explicitly; parsing the section alone never activates
mapping behavior.
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
import json
from pathlib import Path
import re
from typing import Any, Callable, Iterable, NoReturn, TypeVar

import cantools
import yaml
from yaml.nodes import MappingNode, Node, ScalarNode, SequenceNode


_SHORT_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
_PATTERN_CHARACTERS = frozenset("*?[]{}()^$|\\")
_PATTERN_PREFIXES = ("glob:", "prefix:", "regex:", "substring:", "suffix:")
_APPROVAL_REF = re.compile(r"^[A-Z][A-Z0-9_-]*-[0-9]{3,}$")
_T = TypeVar("_T")


class SwcMappingError(ValueError):
    """Stable, object-addressed schema diagnostic."""

    def __init__(self, code: str, object_address: str, detail: str):
        self.code = code
        self.object_address = object_address
        self.detail = detail
        super().__init__(f"{code} {object_address}: {detail}")


@dataclass(frozen=True, order=True)
class ValidationDiagnostic:
    """One deterministic cross-input validation diagnostic."""

    object_address: str
    code: str
    detail: str


class SwcMappingValidationError(ValueError):
    """All deterministic validation failures for one mapping."""

    def __init__(
        self,
        diagnostics: tuple[ValidationDiagnostic, ...],
        expanded_unmapped_signals: tuple[ExpandedUnmappedSignal, ...] = (),
    ):
        self.diagnostics = diagnostics
        self.expanded_unmapped_signals = expanded_unmapped_signals
        super().__init__("\n".join(_render_diagnostic(item) for item in diagnostics))


@dataclass(frozen=True, order=True)
class SignalIdentity:
    message: str
    signal: str


@dataclass(frozen=True, order=True)
class RunnableMapping:
    function: str
    trigger: str
    period_ms: int | None = None


@dataclass(frozen=True, order=True)
class PortMapping:
    name: str
    direction: str
    identity: SignalIdentity
    interface: str
    sharing_group: str | None = None
    write_owner: bool | None = None
    safety_approval_ref: str | None = None


@dataclass(frozen=True, order=True)
class UnmappedSignal:
    direction: str
    identity: SignalIdentity
    disposition: str
    rationale: str
    safety_approval_ref: str | None = None


@dataclass(frozen=True, order=True)
class UnmappedSignalSet:
    """One normalized message awaiting DBC signal expansion in SM2."""

    direction: str
    message: str
    disposition: str
    rationale: str
    safety_approval_ref: str | None = None


@dataclass(frozen=True, order=True)
class UnmappedRunnable:
    function: str
    disposition: str
    rationale: str


@dataclass(frozen=True, order=True)
class SwcMapping:
    name: str
    arxml_short_name: str
    runnables: tuple[RunnableMapping, ...]
    ports: tuple[PortMapping, ...]


@dataclass(frozen=True, order=True)
class EcuMapping:
    name: str
    swcs: tuple[SwcMapping, ...]
    unmapped_signals: tuple[UnmappedSignal, ...]
    unmapped_signal_sets: tuple[UnmappedSignalSet, ...]
    unmapped_runnables: tuple[UnmappedRunnable, ...]


@dataclass(frozen=True, order=True)
class MappingPolicy:
    unmapped_signal: str
    unmapped_runnable: str


@dataclass(frozen=True, order=True)
class ExplicitSwcMapping:
    schema_version: int
    policy: MappingPolicy
    ecus: tuple[EcuMapping, ...]


@dataclass(frozen=True, order=True)
class ExpandedUnmappedSignal:
    """Exact review output expanded from a migration-only message set."""

    ecu: str
    direction: str
    identity: SignalIdentity
    disposition: str
    rationale: str
    safety_approval_ref: str | None = None


@dataclass(frozen=True, order=True)
class SwcMappingValidationResult:
    """Normalized successful validation result."""

    expanded_unmapped_signals: tuple[ExpandedUnmappedSignal, ...]


def _error(code: str, address: str, detail: str) -> NoReturn:
    raise SwcMappingError(code, address, detail)


def _key_text(node: Node, address: str) -> str:
    if not isinstance(node, ScalarNode) or node.tag != "tag:yaml.org,2002:str":
        _error("SWCMAP004", address, "mapping keys must be strings")
    return node.value


def _check_duplicate_keys(node: Node, address: str = "document") -> None:
    if isinstance(node, MappingNode):
        seen: set[str] = set()
        for key_node, value_node in node.value:
            key = _key_text(key_node, address)
            child = key if address == "document" else f"{address}.{key}"
            if key in seen:
                _error("SWCMAP001", child, "duplicate YAML key")
            seen.add(key)
            _check_duplicate_keys(value_node, child)
    elif isinstance(node, SequenceNode):
        for index, item in enumerate(node.value):
            _check_duplicate_keys(item, f"{address}[{index}]")


def _load_yaml(path: Path) -> Any:
    text = path.read_text(encoding="utf-8")
    try:
        node = yaml.compose(text, Loader=yaml.SafeLoader)
        if node is None:
            _error("SWCMAP003", "swc_signal_mapping", "required section is missing")
        _check_duplicate_keys(node)
        return yaml.safe_load(text)
    except SwcMappingError:
        raise
    except yaml.YAMLError:
        _error("SWCMAP001", "document", "invalid safe YAML")


def _mapping(value: Any, address: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        _error("SWCMAP004", address, "expected a mapping")
    return value


def _sequence(value: Any, address: str) -> list[Any]:
    if not isinstance(value, list):
        _error("SWCMAP004", address, "expected a sequence")
    return value


def _known(mapping: dict[str, Any], allowed: set[str], address: str) -> None:
    unknown = sorted(set(mapping) - allowed)
    if unknown:
        _error("SWCMAP002", f"{address}.{unknown[0]}", "unknown key")


def _required(mapping: dict[str, Any], key: str, address: str) -> Any:
    if key not in mapping:
        _error("SWCMAP003", f"{address}.{key}", "required field is missing")
    return mapping[key]


def _string(value: Any, address: str) -> str:
    if not isinstance(value, str):
        _error("SWCMAP004", address, "expected a string")
    if not value or value != value.strip():
        _error("SWCMAP005", address, "must be a non-empty exact value")
    return value


def _short_name(value: Any, address: str) -> str:
    result = _string(value, address)
    if len(result) > 128 or not _SHORT_NAME.fullmatch(result):
        _error("SWCMAP006", address, "invalid AUTOSAR SHORT-NAME")
    return result


def _identity(value: Any, address: str) -> str:
    result = _string(value, address)
    lowered = result.lower()
    if (
        any(character in result for character in _PATTERN_CHARACTERS)
        or ".*" in result
        or lowered.startswith(_PATTERN_PREFIXES)
    ):
        _error("SWCMAP007", address, "pattern matching syntax is not permitted")
    return result


def _choice(value: Any, allowed: set[str], address: str) -> str:
    result = _string(value, address)
    if result not in allowed:
        _error("SWCMAP005", address, f"expected one of {', '.join(sorted(allowed))}")
    return result


def _positive_int(value: Any, address: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        _error("SWCMAP004", address, "expected an integer")
    if value <= 0:
        _error("SWCMAP005", address, "must be greater than zero")
    return value


def _optional_bool(mapping: dict[str, Any], key: str, address: str) -> bool | None:
    if key not in mapping:
        return None
    value = mapping[key]
    if not isinstance(value, bool):
        _error("SWCMAP004", f"{address}.{key}", "expected a boolean")
    return value


def _optional_string(mapping: dict[str, Any], key: str, address: str) -> str | None:
    if key not in mapping:
        return None
    return _string(mapping[key], f"{address}.{key}")


def _parse_runnable(value: Any, address: str) -> RunnableMapping:
    raw = _mapping(value, address)
    _known(raw, {"function", "trigger", "period_ms"}, address)
    function = _short_name(_required(raw, "function", address), f"{address}.function")
    trigger = _choice(
        _required(raw, "trigger", address), {"event", "init", "periodic"}, f"{address}.trigger"
    )
    period = None
    if trigger == "periodic":
        period = _positive_int(_required(raw, "period_ms", address), f"{address}.period_ms")
    elif "period_ms" in raw:
        _error("SWCMAP005", f"{address}.period_ms", "is valid only for a periodic runnable")
    return RunnableMapping(function, trigger, period)


def _parse_port(value: Any, address: str) -> PortMapping:
    raw = _mapping(value, address)
    _known(
        raw,
        {
            "name",
            "direction",
            "message",
            "signal",
            "interface",
            "sharing_group",
            "write_owner",
            "safety_approval_ref",
        },
        address,
    )
    name = _short_name(_required(raw, "name", address), f"{address}.name")
    direction = _choice(
        _required(raw, "direction", address), {"provided", "required"}, f"{address}.direction"
    )
    message = _identity(_required(raw, "message", address), f"{address}.message")
    signal = _identity(_required(raw, "signal", address), f"{address}.signal")
    interface = _short_name(_required(raw, "interface", address), f"{address}.interface")
    sharing_group = None
    if "sharing_group" in raw:
        sharing_group = _string(raw["sharing_group"], f"{address}.sharing_group")
    return PortMapping(
        name=name,
        direction=direction,
        identity=SignalIdentity(message, signal),
        interface=interface,
        sharing_group=sharing_group,
        write_owner=_optional_bool(raw, "write_owner", address),
        safety_approval_ref=_optional_string(raw, "safety_approval_ref", address),
    )


def _parse_unmapped_signal(value: Any, address: str) -> UnmappedSignal:
    raw = _mapping(value, address)
    _known(
        raw,
        {
            "direction",
            "message",
            "signal",
            "disposition",
            "rationale",
            "safety_approval_ref",
        },
        address,
    )
    direction = _choice(
        _required(raw, "direction", address), {"provided", "required"}, f"{address}.direction"
    )
    identity = SignalIdentity(
        _identity(_required(raw, "message", address), f"{address}.message"),
        _identity(_required(raw, "signal", address), f"{address}.signal"),
    )
    return UnmappedSignal(
        direction,
        identity,
        _string(_required(raw, "disposition", address), f"{address}.disposition"),
        _string(_required(raw, "rationale", address), f"{address}.rationale"),
        _optional_string(raw, "safety_approval_ref", address),
    )


def _parse_unmapped_signal_set(value: Any, address: str) -> tuple[UnmappedSignalSet, ...]:
    raw = _mapping(value, address)
    _known(
        raw,
        {
            "direction",
            "messages",
            "disposition",
            "rationale",
            "safety_approval_ref",
        },
        address,
    )
    direction = _choice(
        _required(raw, "direction", address), {"provided", "required"}, f"{address}.direction"
    )
    messages = _sequence(_required(raw, "messages", address), f"{address}.messages")
    if not messages:
        _error("SWCMAP005", f"{address}.messages", "must contain at least one exact message")
    disposition = _string(_required(raw, "disposition", address), f"{address}.disposition")
    rationale = _string(_required(raw, "rationale", address), f"{address}.rationale")
    approval = _optional_string(raw, "safety_approval_ref", address)
    return tuple(
        UnmappedSignalSet(
            direction,
            _identity(message, f"{address}.messages[{index}]"),
            disposition,
            rationale,
            approval,
        )
        for index, message in enumerate(messages)
    )


def _parse_unmapped_runnable(value: Any, address: str) -> UnmappedRunnable:
    raw = _mapping(value, address)
    _known(raw, {"function", "disposition", "rationale"}, address)
    return UnmappedRunnable(
        _short_name(_required(raw, "function", address), f"{address}.function"),
        _string(_required(raw, "disposition", address), f"{address}.disposition"),
        _string(_required(raw, "rationale", address), f"{address}.rationale"),
    )


def _parse_swc(name: str, value: Any, address: str) -> SwcMapping:
    raw = _mapping(value, address)
    _known(raw, {"arxml_short_name", "runnables", "ports"}, address)
    runnables = _sequence(raw.get("runnables", []), f"{address}.runnables")
    ports = _sequence(raw.get("ports", []), f"{address}.ports")
    return SwcMapping(
        name=_identity(name, address),
        arxml_short_name=_short_name(
            _required(raw, "arxml_short_name", address), f"{address}.arxml_short_name"
        ),
        runnables=tuple(
            sorted(
                (
                    _parse_runnable(item, f"{address}.runnables[{index}]")
                    for index, item in enumerate(runnables)
                ),
                key=lambda runnable: (
                    runnable.function,
                    runnable.trigger,
                    runnable.period_ms or 0,
                ),
            )
        ),
        ports=tuple(
            sorted(
                (
                    _parse_port(item, f"{address}.ports[{index}]")
                    for index, item in enumerate(ports)
                ),
                key=lambda port: (
                    port.direction,
                    port.identity.message,
                    port.identity.signal,
                    port.name,
                ),
            )
        ),
    )


def _parse_ecu(name: str, value: Any, address: str) -> EcuMapping:
    raw = _mapping(value, address)
    _known(raw, {"swcs", "unmapped_signals", "unmapped_signal_sets", "unmapped_runnables"}, address)
    swcs = _mapping(_required(raw, "swcs", address), f"{address}.swcs")
    unmapped_signals = _sequence(raw.get("unmapped_signals", []), f"{address}.unmapped_signals")
    signal_sets = _sequence(raw.get("unmapped_signal_sets", []), f"{address}.unmapped_signal_sets")
    unmapped_runnables = _sequence(raw.get("unmapped_runnables", []), f"{address}.unmapped_runnables")
    expanded_sets = (
        item
        for index, value in enumerate(signal_sets)
        for item in _parse_unmapped_signal_set(value, f"{address}.unmapped_signal_sets[{index}]")
    )
    return EcuMapping(
        name=_identity(name, address),
        swcs=tuple(sorted(_parse_swc(key, item, f"{address}.swcs.{key}") for key, item in swcs.items())),
        unmapped_signals=tuple(
            sorted(
                _parse_unmapped_signal(item, f"{address}.unmapped_signals[{index}]")
                for index, item in enumerate(unmapped_signals)
            )
        ),
        unmapped_signal_sets=tuple(sorted(expanded_sets)),
        unmapped_runnables=tuple(
            sorted(
                _parse_unmapped_runnable(item, f"{address}.unmapped_runnables[{index}]")
                for index, item in enumerate(unmapped_runnables)
            )
        ),
    )


def load_swc_mapping(path: str | Path) -> ExplicitSwcMapping:
    """Parse the mapping section from exactly ``path`` without side effects."""

    supplied_path = Path(path)
    document = _mapping(_load_yaml(supplied_path), "document")
    section = _mapping(
        _required(document, "swc_signal_mapping", "document"), "swc_signal_mapping"
    )
    _known(section, {"schema_version", "policy", "ecus"}, "swc_signal_mapping")

    version = _required(section, "schema_version", "swc_signal_mapping")
    if isinstance(version, bool) or not isinstance(version, int):
        _error("SWCMAP004", "swc_signal_mapping.schema_version", "expected an integer")
    if version != 1:
        _error("SWCMAP005", "swc_signal_mapping.schema_version", "only schema version 1 is supported")

    policy_raw = _mapping(_required(section, "policy", "swc_signal_mapping"), "policy")
    _known(policy_raw, {"unmapped_signal", "unmapped_runnable"}, "policy")
    policy = MappingPolicy(
        unmapped_signal=_choice(
            _required(policy_raw, "unmapped_signal", "policy"), {"error"}, "policy.unmapped_signal"
        ),
        unmapped_runnable=_choice(
            _required(policy_raw, "unmapped_runnable", "policy"), {"error"}, "policy.unmapped_runnable"
        ),
    )

    ecus_raw = _mapping(_required(section, "ecus", "swc_signal_mapping"), "ecus")
    ecus = tuple(
        sorted(_parse_ecu(name, value, f"ecus.{name}") for name, value in ecus_raw.items())
    )
    return ExplicitSwcMapping(schema_version=version, policy=policy, ecus=ecus)


def _render_diagnostic(diagnostic: ValidationDiagnostic) -> str:
    return f"{diagnostic.code} {diagnostic.object_address}: {diagnostic.detail}"


def _diagnostic(
    diagnostics: list[ValidationDiagnostic], code: str, address: str, detail: str
) -> None:
    diagnostics.append(ValidationDiagnostic(address, code, detail))


def _safe_identity_name(identity: SignalIdentity) -> str:
    value = f"{identity.message}_{identity.signal}"
    return re.sub(r"_+", "_", re.sub(r"[^A-Za-z0-9_]", "_", value)).lower()


def _port_address(ecu: str, swc: str, port: PortMapping) -> str:
    return (
        f"ecus.{ecu}.swcs.{swc}.ports.{port.direction}."
        f"{port.identity.message}.{port.identity.signal}"
    )


def _signal_address(ecu: str, direction: str, identity: SignalIdentity) -> str:
    return f"ecus.{ecu}.{direction}.{identity.message}.{identity.signal}"


def _approval_is_reviewable(value: str | None) -> bool:
    return value is not None and _APPROVAL_REF.fullmatch(value) is not None


def _structured_safety_identities(database: Any) -> set[SignalIdentity]:
    """Read safety relevance only from structured DBC message attributes."""

    result: set[SignalIdentity] = set()
    for message in database.messages:
        attributes = getattr(getattr(message, "dbc", None), "attributes", {})
        asil_attribute = attributes.get("ASIL")
        asil_value = getattr(asil_attribute, "value", "")
        choices = getattr(getattr(asil_attribute, "definition", None), "choices", None)
        if isinstance(asil_value, int) and isinstance(choices, list):
            asil_value = choices[asil_value] if 0 <= asil_value < len(choices) else ""
        asil = str(asil_value).upper()
        has_safety_level = bool(asil) and asil != "QM"
        has_e2e_profile = "E2E_DataID" in attributes or "E2E_Profile" in attributes
        if has_safety_level or has_e2e_profile:
            result.update(
                SignalIdentity(message.name, signal.name) for signal in message.signals
            )
    return result


def _load_validation_inputs(
    dbc_path: str | Path,
    model_path: str | Path,
    diagnostics: list[ValidationDiagnostic],
) -> tuple[Any | None, dict[str, dict[str, set[str]]]]:
    try:
        database = cantools.database.load_file(str(dbc_path))
    except Exception:
        _diagnostic(
            diagnostics,
            "SWCMAP008",
            "input.dbc",
            "DBC input could not be loaded",
        )
        database = None

    model: dict[str, dict[str, set[str]]] = {}
    try:
        with Path(model_path).open(encoding="utf-8") as stream:
            raw = json.load(stream)
        raw_ecus = raw.get("ecus") if isinstance(raw, dict) else None
        if not isinstance(raw_ecus, dict):
            raise ValueError
        for ecu, entry in raw_ecus.items():
            if not isinstance(entry, dict) or not isinstance(entry.get("swcs", []), list):
                raise ValueError
            swcs: dict[str, set[str]] = {}
            function_owners: dict[str, str] = {}
            for item in entry.get("swcs", []):
                if not isinstance(item, dict) or not isinstance(item.get("name"), str):
                    raise ValueError
                functions = item.get("functions", [])
                if not isinstance(functions, list) or not all(
                    isinstance(value, str) for value in functions
                ):
                    raise ValueError
                if item["name"] in swcs:
                    _diagnostic(
                        diagnostics,
                        "SWCMAP008",
                        f"input.swc_model.ecus.{ecu}.swcs.{item['name']}",
                        "extracted SWC identity does not resolve exactly once",
                    )
                for function in functions:
                    if function in function_owners:
                        _diagnostic(
                            diagnostics,
                            "SWCMAP008",
                            f"input.swc_model.ecus.{ecu}.runnables.{function}",
                            "extracted runnable identity does not resolve exactly once",
                        )
                    else:
                        function_owners[function] = item["name"]
                swcs[item["name"]] = set(functions)
            model[str(ecu).lower()] = swcs
    except (OSError, TypeError, ValueError, json.JSONDecodeError):
        _diagnostic(
            diagnostics,
            "SWCMAP008",
            "input.swc_model",
            "SWC model input is not a valid ECU/SWC inventory",
        )
    return database, model


def _dbc_inventory(
    database: Any,
) -> tuple[
    set[str],
    dict[str, dict[str, Any]],
    dict[str, dict[str, set[SignalIdentity]]],
]:
    ecu_names = {
        (node.name if hasattr(node, "name") else str(node)).lower()
        for node in database.nodes
    }
    messages: dict[str, dict[str, Any]] = {}
    routes: dict[str, dict[str, set[SignalIdentity]]] = defaultdict(
        lambda: {"provided": set(), "required": set()}
    )
    for message in sorted(database.messages, key=lambda item: item.name):
        signals = {signal.name: signal for signal in message.signals}
        messages[message.name] = signals
        senders = {str(sender).lower() for sender in message.senders}
        for signal in sorted(message.signals, key=lambda item: item.name):
            identity = SignalIdentity(message.name, signal.name)
            for sender in senders:
                routes[sender]["provided"].add(identity)
            for receiver in signal.receivers:
                routes[str(receiver).lower()]["required"].add(identity)
    return ecu_names, messages, routes


def validate_swc_mapping(
    mapping: ExplicitSwcMapping,
    dbc_path: str | Path,
    model_path: str | Path,
    governed_ecus: Iterable[str] = (),
) -> SwcMappingValidationResult:
    """Validate an explicit mapping against exact DBC and extracted identities.

    Validation is disconnected from conversion and publishing.  All failures
    are collected and sorted before the caller may mutate output state.
    """

    diagnostics: list[ValidationDiagnostic] = []
    database, model = _load_validation_inputs(dbc_path, model_path, diagnostics)
    if database is None or any(
        item.object_address.startswith("input.swc_model") for item in diagnostics
    ):
        ordered = tuple(sorted(set(diagnostics)))
        raise SwcMappingValidationError(ordered)

    ecu_names, messages, routes = _dbc_inventory(database)
    safety_identities = _structured_safety_identities(database)
    expanded: list[ExpandedUnmappedSignal] = []

    required_ecus = set(model) | {str(ecu).lower() for ecu in governed_ecus}
    mapped_ecus = {ecu.name.lower() for ecu in mapping.ecus}
    for ecu in sorted(required_ecus - mapped_ecus):
        if ecu not in ecu_names:
            _diagnostic(
                diagnostics,
                "SWCMAP008",
                f"validation_scope.ecus.{ecu}",
                "governed ECU is not present in the DBC",
            )
        _diagnostic(
            diagnostics,
            "SWCMAP011",
            f"ecus.{ecu}.coverage.mapping",
            "governed ECU has no explicit mapping section",
        )

    sanitized: dict[str, SignalIdentity] = {}
    for message_name in sorted(messages):
        for signal_name in sorted(messages[message_name]):
            identity = SignalIdentity(message_name, signal_name)
            safe_name = _safe_identity_name(identity)
            previous = sanitized.get(safe_name)
            if previous is not None and previous != identity:
                _diagnostic(
                    diagnostics,
                    "SWCMAP009",
                    f"dbc.{identity.message}.{identity.signal}",
                    "sanitized identity collides with another exact DBC signal",
                )
            else:
                sanitized[safe_name] = identity

    arxml_names: dict[str, tuple[str, str]] = {}
    runnable_owners: dict[tuple[str, str], str] = {}
    interface_identities: dict[str, SignalIdentity] = {}

    for ecu in mapping.ecus:
        ecu_address = f"ecus.{ecu.name}"
        if ecu.name.lower() not in ecu_names:
            _diagnostic(diagnostics, "SWCMAP008", ecu_address, "unknown DBC ECU")
        model_swcs = model.get(ecu.name.lower(), {})

        mapped: set[tuple[str, SignalIdentity]] = set()
        unmapped: set[tuple[str, SignalIdentity]] = set()
        mapped_runnables: set[str] = set()
        unmapped_runnables: set[str] = set()
        bindings: dict[SignalIdentity, list[tuple[SwcMapping, PortMapping]]] = defaultdict(list)

        for swc in ecu.swcs:
            swc_address = f"{ecu_address}.swcs.{swc.name}"
            if swc.name not in model_swcs:
                _diagnostic(diagnostics, "SWCMAP008", swc_address, "unknown extracted SWC")
            previous_arxml = arxml_names.get(swc.arxml_short_name)
            if previous_arxml is not None:
                _diagnostic(
                    diagnostics,
                    "SWCMAP012",
                    f"{swc_address}.arxml_short_name",
                    "ARXML SWC SHORT-NAME is not globally unique",
                )
            else:
                arxml_names[swc.arxml_short_name] = (ecu.name, swc.name)

            port_names: set[str] = set()
            for port in swc.ports:
                address = _port_address(ecu.name, swc.name, port)
                if port.name in port_names:
                    _diagnostic(
                        diagnostics,
                        "SWCMAP012",
                        f"{swc_address}.ports.{port.name}",
                        "port SHORT-NAME is duplicated within the SWC",
                    )
                port_names.add(port.name)
                message_signals = messages.get(port.identity.message)
                if message_signals is None:
                    _diagnostic(diagnostics, "SWCMAP008", address, "unknown DBC message")
                elif port.identity.signal not in message_signals:
                    _diagnostic(diagnostics, "SWCMAP008", address, "unknown DBC signal")
                elif port.identity not in routes[ecu.name.lower()][port.direction]:
                    _diagnostic(
                        diagnostics,
                        "SWCMAP010",
                        address,
                        "port direction does not match exact DBC sender/receiver routing",
                    )
                mapped.add((port.direction, port.identity))
                bindings[port.identity].append((swc, port))
                previous_identity = interface_identities.get(port.interface)
                if previous_identity is not None and previous_identity != port.identity:
                    _diagnostic(
                        diagnostics,
                        "SWCMAP008",
                        f"{address}.interface",
                        "interface resolves to more than one exact signal identity",
                    )
                else:
                    interface_identities[port.interface] = port.identity

            for runnable in swc.runnables:
                address = f"{swc_address}.runnables.{runnable.function}"
                functions = model_swcs.get(swc.name, set())
                if runnable.function not in functions:
                    _diagnostic(diagnostics, "SWCMAP008", address, "unknown extracted runnable")
                owner_key = (ecu.name, runnable.function)
                previous_owner = runnable_owners.get(owner_key)
                if previous_owner is not None:
                    _diagnostic(
                        diagnostics,
                        "SWCMAP012",
                        f"{ecu_address}.runnables.{runnable.function}",
                        "runnable is assigned to more than one SWC",
                    )
                else:
                    runnable_owners[owner_key] = swc.name
                if runnable.function in mapped_runnables:
                    _diagnostic(
                        diagnostics,
                        "SWCMAP012",
                        f"{ecu_address}.runnables.{runnable.function}",
                        "runnable is assigned more than once",
                    )
                mapped_runnables.add(runnable.function)

        for item in ecu.unmapped_signals:
            address = _signal_address(ecu.name, item.direction, item.identity)
            message_signals = messages.get(item.identity.message)
            if message_signals is None:
                _diagnostic(diagnostics, "SWCMAP008", address, "unknown DBC message")
            elif item.identity.signal not in message_signals:
                _diagnostic(diagnostics, "SWCMAP008", address, "unknown DBC signal")
            elif item.identity not in routes[ecu.name.lower()][item.direction]:
                _diagnostic(
                    diagnostics,
                    "SWCMAP010",
                    address,
                    "unmapped direction does not match exact DBC routing",
                )
            if (item.direction, item.identity) in unmapped:
                _diagnostic(
                    diagnostics,
                    "SWCMAP015",
                    address,
                    "duplicate explicit unmapped entries have no precedence",
                )
            unmapped.add((item.direction, item.identity))
            if item.identity in safety_identities and not _approval_is_reviewable(
                item.safety_approval_ref
            ):
                _diagnostic(
                    diagnostics,
                    "SWCMAP014",
                    f"{address}.safety_approval_ref",
                    "safety-sensitive unmapped signal requires a reviewable approval reference",
                )

        for signal_set in ecu.unmapped_signal_sets:
            set_address = (
                f"{ecu_address}.unmapped_signal_sets.{signal_set.direction}."
                f"{signal_set.message}"
            )
            message_signals = messages.get(signal_set.message)
            if message_signals is None:
                _diagnostic(diagnostics, "SWCMAP008", set_address, "unknown DBC message")
                continue
            expanded_for_set = [
                identity
                for identity in sorted(routes[ecu.name.lower()][signal_set.direction])
                if identity.message == signal_set.message
            ]
            if not expanded_for_set:
                _diagnostic(
                    diagnostics,
                    "SWCMAP010",
                    set_address,
                    "message set has no signals for the exact ECU direction",
                )
            for identity in expanded_for_set:
                expanded.append(
                    ExpandedUnmappedSignal(
                        ecu.name,
                        signal_set.direction,
                        identity,
                        signal_set.disposition,
                        signal_set.rationale,
                        signal_set.safety_approval_ref,
                    )
                )
                if identity in safety_identities and not _approval_is_reviewable(
                    signal_set.safety_approval_ref
                ):
                    _diagnostic(
                        diagnostics,
                        "SWCMAP014",
                        f"{_signal_address(ecu.name, signal_set.direction, identity)}."
                        "safety_approval_ref",
                        "safety-sensitive signal-set expansion requires approval",
                    )

        extracted_functions = {
            function for functions in model_swcs.values() for function in functions
        }
        for item in ecu.unmapped_runnables:
            address = f"{ecu_address}.unmapped_runnables.{item.function}"
            if item.function not in extracted_functions:
                _diagnostic(diagnostics, "SWCMAP008", address, "unknown extracted runnable")
            if item.function in unmapped_runnables:
                _diagnostic(
                    diagnostics,
                    "SWCMAP015",
                    address,
                    "duplicate explicit unmapped runnables have no precedence",
                )
            unmapped_runnables.add(item.function)

        for direction, identity in sorted(mapped & unmapped):
            _diagnostic(
                diagnostics,
                "SWCMAP015",
                _signal_address(ecu.name, direction, identity),
                "signal is both mapped and unmapped; no entry has precedence",
            )
        for function in sorted(mapped_runnables & unmapped_runnables):
            _diagnostic(
                diagnostics,
                "SWCMAP015",
                f"{ecu_address}.runnables.{function}",
                "runnable is both mapped and unmapped; no entry has precedence",
            )

        for direction in ("provided", "required"):
            covered = {
                identity
                for entry_direction, identity in mapped | unmapped
                if entry_direction == direction
            }
            for identity in sorted(routes[ecu.name.lower()][direction] - covered):
                _diagnostic(
                    diagnostics,
                    "SWCMAP011",
                    f"{ecu_address}.coverage.{direction}.{identity.message}.{identity.signal}",
                    "routed signal has no exact mapped or unmapped entry",
                )
        for function in sorted(extracted_functions - mapped_runnables - unmapped_runnables):
            _diagnostic(
                diagnostics,
                "SWCMAP011",
                f"{ecu_address}.coverage.runnable.{function}",
                "extracted runnable has no exact mapped or unmapped assignment",
            )

        for identity, identity_bindings in sorted(bindings.items()):
            if len(identity_bindings) < 2:
                continue
            address = _signal_address(
                ecu.name, identity_bindings[0][1].direction, identity
            )
            directions = {port.direction for _, port in identity_bindings}
            interfaces = {port.interface for _, port in identity_bindings}
            groups = {port.sharing_group for _, port in identity_bindings}
            if len(directions) != 1 or len(interfaces) != 1 or len(groups) != 1 or None in groups:
                _diagnostic(
                    diagnostics,
                    "SWCMAP013",
                    address,
                    "repeated binding requires one direction, interface, and sharing group",
                )
            if directions == {"provided"}:
                owners = sum(port.write_owner is True for _, port in identity_bindings)
                if owners != 1:
                    _diagnostic(
                        diagnostics,
                        "SWCMAP013",
                        f"{address}.write_owner",
                        "shared provided binding requires exactly one write owner",
                    )
            if identity in safety_identities:
                approvals = {port.safety_approval_ref for _, port in identity_bindings}
                if (
                    len(approvals) != 1
                    or None in approvals
                    or not _approval_is_reviewable(next(iter(approvals)))
                ):
                    _diagnostic(
                        diagnostics,
                        "SWCMAP014",
                        f"{address}.safety_approval_ref",
                        "shared safety-sensitive binding requires one approval reference",
                    )

    ordered_expanded = tuple(sorted(set(expanded)))
    ordered_diagnostics = tuple(sorted(set(diagnostics)))
    if ordered_diagnostics:
        raise SwcMappingValidationError(ordered_diagnostics, ordered_expanded)
    return SwcMappingValidationResult(ordered_expanded)


def validate_before_publish(
    mapping: ExplicitSwcMapping,
    dbc_path: str | Path,
    model_path: str | Path,
    publish: Callable[[], _T],
    governed_ecus: Iterable[str] = (),
) -> _T:
    """Validate all inputs before invoking a caller-owned atomic publisher."""

    validate_swc_mapping(mapping, dbc_path, model_path, governed_ecus)
    return publish()


__all__ = [
    "EcuMapping",
    "ExpandedUnmappedSignal",
    "ExplicitSwcMapping",
    "MappingPolicy",
    "PortMapping",
    "RunnableMapping",
    "SignalIdentity",
    "SwcMapping",
    "SwcMappingError",
    "SwcMappingValidationError",
    "SwcMappingValidationResult",
    "UnmappedRunnable",
    "UnmappedSignal",
    "UnmappedSignalSet",
    "ValidationDiagnostic",
    "load_swc_mapping",
    "validate_before_publish",
    "validate_swc_mapping",
]
