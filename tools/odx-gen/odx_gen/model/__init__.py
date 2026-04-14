"""Model dataclasses for parsed ECU diagnostic information."""

from .ecu_diagnostic import (
    DidEntry,
    ServiceEntry,
    DtcEntry,
    EcuDiagnosticModel,
)

__all__ = [
    "DidEntry",
    "ServiceEntry",
    "DtcEntry",
    "EcuDiagnosticModel",
]
