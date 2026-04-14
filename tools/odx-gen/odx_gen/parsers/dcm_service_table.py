"""Extract the set of UDS service IDs implemented by Dcm.c.

Strategy:

1. Parse `Dcm.h` for `#define DCM_SID_<NAME>  0xXXu` constants. These give
   us a mapping from C-symbolic SID name to integer. The macro names are
   derived directly from the firmware header — nothing is hardcoded here.

2. Parse `Dcm.c` for `case DCM_SID_<NAME>:` labels inside the dispatch
   switch. Only SIDs that appear as dispatch cases count as implemented.

3. For each implemented SID, derive the canonical UDS service name from
   the macro name (e.g. `DCM_SID_READ_DID` -> `ReadDataByIdentifier`)
   using a small lookup of standard ISO 14229 names. The lookup keys are
   the macro suffixes that actually appear in the firmware header — we
   never invent SIDs that aren't in the source.

If a macro suffix is found in firmware but is not in the canonical name
table, the parser still records the SID using a CamelCase'd version of
the suffix and emits a TODO note so it can be reviewed.
"""

from __future__ import annotations

import re
from pathlib import Path

from ..model import ServiceEntry


# Recognised UDS service macro suffixes -> canonical names.
# The keys are SUFFIX strings (the part after `DCM_SID_`). They are not
# ECU-specific data; they are stable token spellings as used by the AUTOSAR
# Dcm SWS and replicated in our firmware header. The canonical names are
# from ISO 14229 (UDS) and likewise standardised.
_UDS_SUFFIX_TO_NAME: dict[str, str] = {
    "SESSION_CTRL": "DiagnosticSessionControl",
    "ECU_RESET": "ECUReset",
    "CLEAR_DTC": "ClearDiagnosticInformation",
    "READ_DTC": "ReadDTCInformation",
    "READ_DID": "ReadDataByIdentifier",
    "READ_MEM": "ReadMemoryByAddress",
    "READ_SCALING": "ReadScalingDataByIdentifier",
    "SECURITY_ACCESS": "SecurityAccess",
    "COMM_CTRL": "CommunicationControl",
    "READ_PERIODIC": "ReadDataByPeriodicIdentifier",
    "DYNAMIC_DEFINE": "DynamicallyDefineDataIdentifier",
    "WRITE_DID": "WriteDataByIdentifier",
    "IO_CTRL": "InputOutputControlByIdentifier",
    "ROUTINE_CTRL": "RoutineControl",
    "REQUEST_DOWNLOAD": "RequestDownload",
    "REQUEST_UPLOAD": "RequestUpload",
    "TRANSFER_DATA": "TransferData",
    "REQUEST_TRANSFER_EXIT": "RequestTransferExit",
    "WRITE_MEM": "WriteMemoryByAddress",
    "TESTER_PRESENT": "TesterPresent",
    "ACCESS_TIMING": "AccessTimingParameter",
    "SECURED_DATA": "SecuredDataTransmission",
    "CONTROL_DTC_SETTING": "ControlDTCSetting",
    "RESPONSE_ON_EVENT": "ResponseOnEvent",
    "LINK_CONTROL": "LinkControl",
}


_SID_DEFINE_RE = re.compile(
    r"^[ \t]*#[ \t]*define[ \t]+DCM_SID_(?P<suffix>[A-Z0-9_]+)[ \t]+"
    r"(?P<value>0[xX][0-9A-Fa-f]+)[uUlL]*",
    re.MULTILINE,
)


def _to_camel_case(suffix: str) -> str:
    parts = suffix.lower().split("_")
    return "".join(p.capitalize() for p in parts if p)


def parse_sid_macros(dcm_header_path: str | Path) -> dict[str, tuple[int, int]]:
    """Return a mapping of `DCM_SID_<SUFFIX>` -> (int value, line number)."""
    p = Path(dcm_header_path)
    if not p.is_file():
        raise FileNotFoundError(f"Dcm header not found: {p}")
    text = p.read_text(encoding="utf-8", errors="replace")

    out: dict[str, tuple[int, int]] = {}
    for m in _SID_DEFINE_RE.finditer(text):
        suffix = m.group("suffix")
        value = int(m.group("value"), 16)
        line = text.count("\n", 0, m.start()) + 1
        out[suffix] = (value, line)
    return out


def parse_implemented_sids(
    dcm_source_path: str | Path,
    sid_macros: dict[str, tuple[int, int]],
) -> set[str]:
    """Return the set of `DCM_SID_<SUFFIX>` macro names actually dispatched."""
    p = Path(dcm_source_path)
    if not p.is_file():
        raise FileNotFoundError(f"Dcm.c not found: {p}")
    text = p.read_text(encoding="utf-8", errors="replace")

    implemented: set[str] = set()
    for suffix in sid_macros:
        macro = f"DCM_SID_{suffix}"
        # Look for `case DCM_SID_<SUFFIX>:` in the dispatcher
        pattern = re.compile(rf"\bcase\b\s+{re.escape(macro)}\s*:")
        if pattern.search(text):
            implemented.add(suffix)
    return implemented


def parse_services(
    dcm_header_path: str | Path,
    dcm_source_path: str | Path,
) -> tuple[list[ServiceEntry], list[str]]:
    """Build the list of implemented ServiceEntry plus any TODO notes."""
    todos: list[str] = []

    macros = parse_sid_macros(dcm_header_path)
    if not macros:
        todos.append(
            f"TODO: no `#define DCM_SID_*` macros found in {dcm_header_path}"
        )
        return [], todos

    implemented = parse_implemented_sids(dcm_source_path, macros)
    if not implemented:
        todos.append(
            f"TODO: no `case DCM_SID_*:` labels matched in {dcm_source_path}; "
            f"Dcm dispatcher format may differ from the expected switch pattern"
        )

    services: list[ServiceEntry] = []
    header_path_str = str(Path(dcm_header_path))
    for suffix in sorted(implemented, key=lambda s: macros[s][0]):
        sid_int, line_no = macros[suffix]
        if suffix in _UDS_SUFFIX_TO_NAME:
            name = _UDS_SUFFIX_TO_NAME[suffix]
        else:
            name = _to_camel_case(suffix)
            todos.append(
                f"TODO: DCM_SID_{suffix} (0x{sid_int:02X}) implemented in "
                f"firmware but has no canonical UDS name mapping; using "
                f"derived name `{name}`"
            )
        services.append(
            ServiceEntry(
                sid=sid_int,
                name=name,
                source_file=header_path_str,
                source_line=line_no,
            )
        )
    return services, todos
