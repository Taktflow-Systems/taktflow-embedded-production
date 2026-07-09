"""Stable, portable diagnostics shared by the DBC-to-C pipeline."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Iterable


class DiagnosticSeverity(str, Enum):
    """Machine-readable diagnostic severity."""

    WARNING = "warning"
    ERROR = "error"


@dataclass(frozen=True)
class PipelineDiagnostic:
    """One object-addressed pipeline diagnostic."""

    code: str
    severity: DiagnosticSeverity
    source: str
    message: str

    def __str__(self) -> str:
        return f"[{self.code}] {self.severity.value} {self.source}: {self.message}"


class PipelineDiagnosticError(Exception):
    """Raised when error diagnostics make further output unsafe."""

    def __init__(self, diagnostics: Iterable[PipelineDiagnostic]):
        self.diagnostics = tuple(diagnostics)
        super().__init__("\n".join(str(item) for item in self.diagnostics))


class DiagnosticBag:
    """Collect diagnostics and enforce the no-output-after-error invariant."""

    def __init__(self) -> None:
        self.items: list[PipelineDiagnostic] = []

    def add(self, diagnostic: PipelineDiagnostic) -> PipelineDiagnostic:
        self.items.append(diagnostic)
        return diagnostic

    def warning(self, code: str, source: str, message: str) -> PipelineDiagnostic:
        return self.add(
            PipelineDiagnostic(code, DiagnosticSeverity.WARNING, source, message)
        )

    def error(self, code: str, source: str, message: str) -> PipelineDiagnosticError:
        diagnostic = self.add(
            PipelineDiagnostic(code, DiagnosticSeverity.ERROR, source, message)
        )
        return PipelineDiagnosticError([diagnostic])

    def raise_if_errors(self) -> None:
        errors = [
            item for item in self.items if item.severity is DiagnosticSeverity.ERROR
        ]
        if errors:
            raise PipelineDiagnosticError(errors)
