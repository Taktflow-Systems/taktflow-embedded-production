"""Shared fail-closed strict validation for emitted AUTOSAR XML."""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

import autosar_data as ad

try:
    from tools.pipeline_diagnostics import locate_text, portable_path
except ModuleNotFoundError:
    from pipeline_diagnostics import locate_text, portable_path

if __name__ == "arxml_validation":
    sys.modules.setdefault("tools.arxml_validation", sys.modules[__name__])
elif __name__ == "tools.arxml_validation":
    sys.modules.setdefault("arxml_validation", sys.modules[__name__])


@dataclass(frozen=True)
class StrictValidationResult:
    """Successful strict-validation counts."""

    identifiable_count: int
    warning_count: int = 0
    reference_error_count: int = 0


class StrictArxmlValidationError(Exception):
    """A portable, machine-readable strict validation failure."""

    def __init__(
        self,
        *,
        code: str,
        path: str,
        message: str,
        line: int | None = None,
        column: int | None = None,
        warning_count: int = 0,
        reference_error_count: int = 0,
    ) -> None:
        self.code = code
        self.path = path
        self.message = message
        self.line = line
        self.column = column
        self.warning_count = warning_count
        self.reference_error_count = reference_error_count
        super().__init__(str(self))

    def __str__(self) -> str:
        location = self.path
        if self.line is not None:
            location += f":{self.line}"
            if self.column is not None:
                location += f":{self.column}"
        return f"[{self.code}] {location}: {self.message}"


def _portable_exception(exc: Exception, path: Path) -> tuple[str, int | None, int | None]:
    raw = str(exc)
    coordinates = re.search(r":(\d+)(?::(\d+))?:", raw)
    line = int(coordinates.group(1)) if coordinates else None
    column = (
        int(coordinates.group(2))
        if coordinates and coordinates.group(2) is not None
        else None
    )
    display_path = portable_path(path)
    message = raw.replace(str(path), display_path)
    message = message.replace(str(path).replace("\\", "/"), display_path)
    return message, line, column


def validate_arxml_strict(path: str | Path) -> StrictValidationResult:
    """Strict-load one ARXML file and reject warnings or broken references."""
    input_path = Path(path)
    display_path = portable_path(input_path)
    model = ad.AutosarModel()
    try:
        _, warnings = model.load_file(str(input_path), strict=True)
    except Exception as exc:
        message, line, column = _portable_exception(exc, input_path)
        raise StrictArxmlValidationError(
            code="ARXML200",
            path=display_path,
            message=message,
            line=line,
            column=column,
        ) from exc

    if warnings:
        first = str(warnings[0])
        raise StrictArxmlValidationError(
            code="ARXML201",
            path=display_path,
            message="strict load produced %d warning(s): %s" % (
                len(warnings), first
            ),
            warning_count=len(warnings),
        )

    reference_errors = model.check_references()
    if reference_errors:
        first = reference_errors[0]
        target = getattr(first, "character_data", "") or "<missing target>"
        _, line, column = locate_text(input_path, target)
        raise StrictArxmlValidationError(
            code="ARXML202",
            path=display_path,
            message="%d unresolved reference(s); first target: %s" % (
                len(reference_errors), target
            ),
            line=line,
            column=column,
            reference_error_count=len(reference_errors),
        )

    return StrictValidationResult(
        identifiable_count=len(list(model.identifiable_elements))
    )
