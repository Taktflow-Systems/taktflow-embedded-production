"""Structured, deterministic reporting of DBC conversion assumptions."""

from __future__ import annotations

import json
import os
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from pipeline_diagnostics import PipelineDiagnostic


@dataclass(frozen=True, order=True)
class ConventionRecord:
    """Presence and usage count for one recognized input convention."""

    name: str
    scope: str
    defined: bool
    explicit_instances: int


@dataclass(frozen=True, order=True)
class ValueRecord:
    """One object-addressed default or sidecar-supplied value."""

    identity: str
    field: str
    value: str
    reason: str


@dataclass(frozen=True, order=True)
class TimingRecord:
    """Resolved send-type and cycle-time behavior for one message."""

    identity: str
    send_type: str
    cycle_ms: int
    decision: str
    reason: str


@dataclass(frozen=True, order=True)
class DiagnosticRecord:
    """Portable projection of a pipeline diagnostic."""

    code: str
    severity: str
    source: str
    location: str
    message: str


def _display_path(path: str | Path | None) -> str:
    if not path:
        return "not supplied"
    return Path(path).name


def _stable_value(value: object) -> str:
    if value is None:
        return "none"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (list, tuple, dict)):
        return json.dumps(value, sort_keys=True, separators=(",", ":"))
    return str(value)


def _cell(value: object) -> str:
    return str(value).replace("|", "\\|").replace("\r", " ").replace("\n", " ")


def _code(value: object) -> str:
    return "`%s`" % _cell(value).replace("`", "'")


class AssumptionsCollector:
    """Collect conversion decisions once and render them in stable order."""

    def __init__(
        self,
        source_path: str | Path,
        sidecar_path: str | Path | None = None,
    ) -> None:
        self.source = _display_path(source_path)
        self.sidecar = _display_path(sidecar_path)
        self._conventions: dict[tuple[str, str], ConventionRecord] = {}
        self._defaults: set[ValueRecord] = set()
        self._overrides: set[ValueRecord] = set()
        self._timings: dict[str, TimingRecord] = {}
        self._diagnostics: set[DiagnosticRecord] = set()

    def record_convention(
        self,
        name: str,
        scope: str,
        defined: bool,
        explicit_instances: int,
    ) -> None:
        self._conventions[(scope, name)] = ConventionRecord(
            name, scope, bool(defined), int(explicit_instances)
        )

    def record_default(
        self,
        identity: str,
        field: str,
        value: object,
        reason: str,
    ) -> None:
        self._defaults.add(
            ValueRecord(identity, field, _stable_value(value), reason)
        )

    def record_override(
        self,
        identity: str,
        field: str,
        value: object,
        reason: str,
    ) -> None:
        self._overrides.add(
            ValueRecord(identity, field, _stable_value(value), reason)
        )

    def record_timing(
        self,
        identity: str,
        send_type: str,
        cycle_ms: int,
        decision: str,
        reason: str,
    ) -> None:
        self._timings[identity] = TimingRecord(
            identity, send_type, int(cycle_ms), decision, reason
        )

    def record_diagnostics(
        self, diagnostics: Iterable[PipelineDiagnostic]
    ) -> None:
        for diagnostic in diagnostics:
            location = _display_path(diagnostic.path) if diagnostic.path else "-"
            if diagnostic.line is not None:
                location += ":%d" % diagnostic.line
                if diagnostic.column is not None:
                    location += ":%d" % diagnostic.column
            self._diagnostics.add(
                DiagnosticRecord(
                    diagnostic.code,
                    diagnostic.severity.value,
                    diagnostic.source,
                    location,
                    diagnostic.message,
                )
            )

    @staticmethod
    def _value_section(title: str, records: Iterable[ValueRecord]) -> list[str]:
        ordered = sorted(records)
        lines = ["## %s" % title, ""]
        if not ordered:
            lines.extend(["None.", ""])
            return lines
        lines.extend(
            [
                "| Object | Field | Value | Reason |",
                "|---|---|---|---|",
            ]
        )
        lines.extend(
            "| %s | %s | %s | %s |"
            % (
                _code(record.identity),
                _code(record.field),
                _code(record.value),
                _cell(record.reason),
            )
            for record in ordered
        )
        lines.append("")
        return lines

    def render(self) -> str:
        """Render deterministic Markdown with no timestamps or host paths."""
        lines = [
            "# DBC Conversion Assumptions",
            "",
            "Source DBC: %s" % _code(self.source),
            "",
            "ECU-model sidecar: %s" % _code(self.sidecar),
            "",
            "This report records input conventions and fallback decisions used by "
            "the DBC-to-ARXML conversion.",
            "",
            "## Recognized conventions",
            "",
            "| Convention | Scope | Defined | Explicit instances |",
            "|---|---|---:|---:|",
        ]
        for record in sorted(self._conventions.values()):
            lines.append(
                "| %s | %s | %s | %d |"
                % (
                    _code(record.name),
                    _code(record.scope),
                    "yes" if record.defined else "no",
                    record.explicit_instances,
                )
            )
        if not self._conventions:
            lines.append("| _None recorded_ | - | no | 0 |")
        lines.append("")

        lines.extend(self._value_section("Applied defaults", self._defaults))
        lines.extend(
            self._value_section("Sidecar-supplied overrides", self._overrides)
        )

        lines.extend(
            [
                "## Send-type to timing mapping",
                "",
                "The converter applies the following mapping, aligned with the "
                "network-design concepts identified by AUTOSAR TPS_SYST_01077 "
                "Table 6.62:",
                "",
                "- ARXML I-PDU timing is emitted for every positive cycle time, "
                "including event messages that define a repetition period.",
                "- Downstream C generation maps `event` to `DIRECT` and `cyclic` "
                "to `PERIODIC`; an explicit C-generation sidecar value wins.",
                "- `noMsgSendType` or an absent send type uses cycle zero as "
                "`DIRECT` and a positive cycle as `PERIODIC` downstream.",
                "",
                "| Message | Resolved send type | Cycle (ms) | Decision | Reason |",
                "|---|---|---:|---|---|",
            ]
        )
        for record in sorted(self._timings.values()):
            lines.append(
                "| %s | %s | %d | %s | %s |"
                % (
                    _code(record.identity),
                    _code(record.send_type),
                    record.cycle_ms,
                    _cell(record.decision),
                    _cell(record.reason),
                )
            )
        if not self._timings:
            lines.append("| _None recorded_ | - | 0 | - | - |")
        lines.append("")

        lines.extend(
            [
                "## Pipeline diagnostics",
                "",
                "| Code | Severity | Source | Location | Message |",
                "|---|---|---|---|---|",
            ]
        )
        for diagnostic in sorted(self._diagnostics):
            lines.append(
                "| %s | %s | %s | %s | %s |"
                % (
                    _code(diagnostic.code),
                    _code(diagnostic.severity),
                    _cell(diagnostic.source),
                    _code(diagnostic.location),
                    _cell(diagnostic.message),
                )
            )
        if not self._diagnostics:
            lines.append("| - | - | No diagnostics emitted. | - | - |")
        lines.append("")
        return "\n".join(lines)

    def write_atomic(self, path: str | Path) -> None:
        """Atomically replace one report, preserving the old file on failure."""
        destination = Path(path)
        destination.parent.mkdir(parents=True, exist_ok=True)
        temp_path: Path | None = None
        try:
            fd, raw_temp_path = tempfile.mkstemp(
                prefix=".%s." % destination.name,
                suffix=".tmp",
                dir=destination.parent,
                text=True,
            )
            temp_path = Path(raw_temp_path)
            with os.fdopen(fd, "w", encoding="utf-8", newline="") as stream:
                stream.write(self.render())
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temp_path, destination)
            temp_path = None
        finally:
            if temp_path is not None and temp_path.exists():
                try:
                    temp_path.unlink()
                except OSError:
                    pass
