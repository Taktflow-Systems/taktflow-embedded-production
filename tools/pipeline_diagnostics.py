"""Stable, portable diagnostics shared by the DBC-to-C pipeline."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path
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
    path: str = ""
    line: int | None = None
    column: int | None = None

    def __str__(self) -> str:
        location = self.path
        if location and self.line is not None:
            location += f":{self.line}"
            if self.column is not None:
                location += f":{self.column}"
        prefix = f"{location} " if location else ""
        return (
            f"[{self.code}] {self.severity.value} "
            f"{prefix}{self.source}: {self.message}"
        )


def portable_path(path: str | Path) -> str:
    """Return a repository-relative path or basename, never a host path."""
    candidate = Path(path)
    try:
        return candidate.resolve().relative_to(Path.cwd().resolve()).as_posix()
    except (OSError, ValueError):
        return candidate.name


def locate_text(path: str | Path, needle: str) -> tuple[str, int | None, int | None]:
    """Locate a token in a text input using portable one-based coordinates."""
    display_path = portable_path(path)
    try:
        text = Path(path).read_text(encoding="utf-8")
    except (OSError, UnicodeError):
        return display_path, None, None
    index = text.find(needle)
    if index < 0:
        return display_path, None, None
    line = text.count("\n", 0, index) + 1
    last_newline = text.rfind("\n", 0, index)
    column = index - last_newline
    return display_path, line, column


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

    def warning(
        self,
        code: str,
        source: str,
        message: str,
        *,
        path: str = "",
        line: int | None = None,
        column: int | None = None,
    ) -> PipelineDiagnostic:
        return self.add(
            PipelineDiagnostic(
                code, DiagnosticSeverity.WARNING, source, message,
                path, line, column,
            )
        )

    def error(
        self,
        code: str,
        source: str,
        message: str,
        *,
        path: str = "",
        line: int | None = None,
        column: int | None = None,
    ) -> PipelineDiagnosticError:
        diagnostic = self.add(
            PipelineDiagnostic(
                code, DiagnosticSeverity.ERROR, source, message,
                path, line, column,
            )
        )
        return PipelineDiagnosticError([diagnostic])

    def raise_if_errors(self) -> None:
        errors = [
            item for item in self.items if item.severity is DiagnosticSeverity.ERROR
        ]
        if errors:
            raise PipelineDiagnosticError(errors)
