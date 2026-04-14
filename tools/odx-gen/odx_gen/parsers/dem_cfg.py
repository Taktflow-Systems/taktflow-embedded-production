"""Parser for `firmware/ecu/<ecu>/cfg/Dem_Cfg_<Ecu>.c`.

Currently the firmware tree does not contain Dem configuration files. The
parser tolerates this: when the file is absent it returns an empty DTC
list and emits a TODO note. It NEVER fabricates DTCs from defaults.

Once Phase 1 introduces real Dem_Cfg files, the regex stubs below can be
extended to extract real DTC tables. They are written defensively so that
a missing or malformed file produces an empty list rather than a crash.
"""

from __future__ import annotations

import re
from pathlib import Path

from ..model import DtcEntry


# Generic shape of a Dem event row: { id, code, severity, "name" } — the
# real schema isn't fixed yet, so this regex is intentionally tolerant.
_EVENT_ROW_RE = re.compile(
    r"\{\s*"
    r"(?P<event>0[xX][0-9A-Fa-f]+|\d+)[uU]?\s*,\s*"
    r"(?P<code>0[xX][0-9A-Fa-f]+|\d+)[uU]?\s*,\s*"
    r"(?:DEM_SEV_)?(?P<sev>[A-Za-z_]+)\s*,\s*"
    r"\"(?P<name>[^\"]*)\""
    r"\s*\}",
)


def parse_dem_cfg(
    repo_root: Path | str,
    ecu_name: str,
) -> tuple[list[DtcEntry], list[str]]:
    """Return (dtc_entries, todos) for a given ECU.

    If the Dem_Cfg file does not exist, returns ([], [todo_message]).
    """
    repo_root = Path(repo_root)
    todos: list[str] = []

    cfg_path = (
        repo_root / "firmware" / "ecu" / ecu_name / "cfg"
        / f"Dem_Cfg_{ecu_name.capitalize()}.c"
    )

    if not cfg_path.is_file():
        todos.append(
            f"TODO: Dem_Cfg_{ecu_name.capitalize()}.c not present at "
            f"{cfg_path}; DTC section empty. Populate once a Dem config "
            f"file is added to the firmware."
        )
        return [], todos

    text = cfg_path.read_text(encoding="utf-8", errors="replace")

    entries: list[DtcEntry] = []
    for m in _EVENT_ROW_RE.finditer(text):
        try:
            event_raw = m.group("event")
            code_raw = m.group("code")
            event_id = int(event_raw, 16) if event_raw.lower().startswith("0x") else int(event_raw)
            dtc_code = int(code_raw, 16) if code_raw.lower().startswith("0x") else int(code_raw)
            entries.append(
                DtcEntry(
                    dtc_code=dtc_code,
                    event_id=event_id,
                    severity=m.group("sev"),
                    name=m.group("name"),
                )
            )
        except ValueError:
            continue

    if not entries:
        todos.append(
            f"TODO: Dem_Cfg file {cfg_path} present but no DTC rows extracted; "
            f"parser regex may need updating once the file format stabilises."
        )

    return entries, todos
