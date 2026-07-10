"""Typed, disconnected parser for explicit signal-to-SWC mappings.

This module deliberately has no converter or emitter integration.  Callers must
supply the sidecar path explicitly; parsing the section alone never activates
mapping behavior.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
from typing import Any, NoReturn

import yaml
from yaml.nodes import MappingNode, Node, ScalarNode, SequenceNode


_SHORT_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
_PATTERN_CHARACTERS = frozenset("*?[]{}()^$|\\")
_PATTERN_PREFIXES = ("glob:", "prefix:", "regex:", "substring:", "suffix:")


class SwcMappingError(ValueError):
    """Stable, object-addressed schema diagnostic."""

    def __init__(self, code: str, object_address: str, detail: str):
        self.code = code
        self.object_address = object_address
        self.detail = detail
        super().__init__(f"{code} {object_address}: {detail}")


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


@dataclass(frozen=True, order=True)
class UnmappedSignal:
    direction: str
    identity: SignalIdentity
    disposition: str
    rationale: str


@dataclass(frozen=True, order=True)
class UnmappedSignalSet:
    """One normalized message awaiting DBC signal expansion in SM2."""

    direction: str
    message: str
    disposition: str
    rationale: str


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
        {"name", "direction", "message", "signal", "interface", "sharing_group", "write_owner"},
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
    )


def _parse_unmapped_signal(value: Any, address: str) -> UnmappedSignal:
    raw = _mapping(value, address)
    _known(raw, {"direction", "message", "signal", "disposition", "rationale"}, address)
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
    )


def _parse_unmapped_signal_set(value: Any, address: str) -> tuple[UnmappedSignalSet, ...]:
    raw = _mapping(value, address)
    _known(raw, {"direction", "messages", "disposition", "rationale"}, address)
    direction = _choice(
        _required(raw, "direction", address), {"provided", "required"}, f"{address}.direction"
    )
    messages = _sequence(_required(raw, "messages", address), f"{address}.messages")
    if not messages:
        _error("SWCMAP005", f"{address}.messages", "must contain at least one exact message")
    disposition = _string(_required(raw, "disposition", address), f"{address}.disposition")
    rationale = _string(_required(raw, "rationale", address), f"{address}.rationale")
    return tuple(
        UnmappedSignalSet(direction, _identity(message, f"{address}.messages[{index}]"), disposition, rationale)
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


__all__ = [
    "EcuMapping",
    "ExplicitSwcMapping",
    "MappingPolicy",
    "PortMapping",
    "RunnableMapping",
    "SignalIdentity",
    "SwcMapping",
    "SwcMappingError",
    "UnmappedRunnable",
    "UnmappedSignal",
    "UnmappedSignalSet",
    "load_swc_mapping",
]
