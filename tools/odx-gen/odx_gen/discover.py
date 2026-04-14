"""Discover ECUs by scanning the firmware directory tree.

Returns whatever ECUs are present. There is no hardcoded list.
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Iterable


def find_repo_root(start: Path | None = None) -> Path:
    """Walk upwards from `start` until a directory containing `firmware/` is found."""
    here = (start or Path(__file__)).resolve()
    for parent in [here] + list(here.parents):
        if (parent / "firmware" / "ecu").is_dir():
            return parent
    raise FileNotFoundError(
        "Could not locate a `firmware/ecu` directory walking upwards "
        f"from {here}. Pass --repo-root explicitly."
    )


def discover_ecus(repo_root: Path | str) -> list[str]:
    """List ECUs that have a `Dcm_Cfg_<Ecu>.c` file under firmware/ecu/<name>/cfg/.

    The ECU short name is taken from the directory name (lowercased), not
    from the C filename, so spelling discrepancies don't break discovery.
    """
    root = Path(repo_root)
    ecu_root = root / "firmware" / "ecu"
    if not ecu_root.is_dir():
        return []

    pat = re.compile(r"^Dcm_Cfg_.+\.c$")
    found: list[str] = []
    for child in sorted(ecu_root.iterdir()):
        if not child.is_dir():
            continue
        cfg_dir = child / "cfg"
        if not cfg_dir.is_dir():
            continue
        if any(pat.match(f.name) for f in cfg_dir.iterdir() if f.is_file()):
            found.append(child.name)
    return found


def dcm_cfg_path_for(repo_root: Path | str, ecu_name: str) -> Path:
    """Return the canonical Dcm_Cfg_<Ecu>.c path for an ECU.

    Tries (in order):
      1. Dcm_Cfg_<EcuName>.c with first-letter-capitalised
      2. any single Dcm_Cfg_*.c in the ECU's cfg directory

    Raises FileNotFoundError if none exist.
    """
    cfg_dir = Path(repo_root) / "firmware" / "ecu" / ecu_name / "cfg"
    capitalized = cfg_dir / f"Dcm_Cfg_{ecu_name.capitalize()}.c"
    if capitalized.is_file():
        return capitalized

    matches = sorted(cfg_dir.glob("Dcm_Cfg_*.c")) if cfg_dir.is_dir() else []
    if len(matches) == 1:
        return matches[0]
    if len(matches) == 0:
        raise FileNotFoundError(
            f"No Dcm_Cfg_*.c found in {cfg_dir} for ECU `{ecu_name}`"
        )
    raise FileNotFoundError(
        f"Multiple Dcm_Cfg_*.c files in {cfg_dir} for ECU `{ecu_name}`: "
        f"{[m.name for m in matches]}; cannot pick one unambiguously"
    )
