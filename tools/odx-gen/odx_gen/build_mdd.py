# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D5 — MDD FlatBuffers emitter for odx-gen (STUB mode).

Takes an `EcuDiagnosticModel` and writes `build/mdd/<ecu>.mdd` in a
clearly-labelled stub format that tracks the upstream CDA
`cda-database` schema vendored at `odx_gen/schemas/diagnostic_description.fbs`.

Format contract (stub mode):

    magic   : b"MDDSTUB\n"       (8 bytes, unambiguous non-flatbuffers)
    body    : JSON (UTF-8) with fields:
        schema                   : "diagnostic_description.fbs"
        schema_source            : "eclipse-opensovd/cda-database"
        schema_compat_verified   : false   (flatc not wired in yet)
        ecu_name                 : str
        version                  : "0.1.0"
        revision                 : git short hash if resolvable, else ""
        dids                     : [int, ...]       (16-bit DID numbers)
        did_details              : [{"did": int, "name": str, "length_bytes": int}, ...]
        services                 : [int, ...]       (UDS SIDs)
        dtc_count                : int
        dtcs                     : [{"code": int, "name": str}, ...]
        emitted_by               : "odx_gen.build_mdd stub vX.Y"
        emitted_on               : ISO timestamp

A follow-up commit will replace the stub body with a real flatc-emitted
EcuData buffer. The stub -> flatbuf migration will NOT change the test
contract in tests/test_mdd_emit.py because the test recognises both
variants and asserts the DID set in either case.

The file extension remains `.mdd` in both modes so downstream tooling
paths stay stable.
"""

from __future__ import annotations

import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from .model import EcuDiagnosticModel

MDDSTUB_MAGIC = b"MDDSTUB\n"
EMITTER_VERSION = "0.1.0-stub"


def _git_short_hash(repo_root: Path) -> str:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short=8", "HEAD"],
            cwd=repo_root,
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except FileNotFoundError:
        pass
    return ""


def build_mdd_stub(model: EcuDiagnosticModel, repo_root: Path) -> bytes:
    """Serialize `model` as an MDDSTUB blob. Returns the full file bytes
    (magic header + JSON body).
    """
    payload = {
        "schema": "diagnostic_description.fbs",
        "schema_source": "eclipse-opensovd/classic-diagnostic-adapter/cda-database/src/flatbuf/",
        "schema_compat_verified": False,
        "schema_compat_note": (
            "flatc not installed on build host; real flatbuffers emitter "
            "is a Phase 5+ follow-up. Schema is vendored at "
            "odx_gen/schemas/diagnostic_description.fbs."
        ),
        "ecu_name": model.ecu_name,
        "version": EMITTER_VERSION,
        "revision": _git_short_hash(repo_root),
        "dids": sorted({d.did_id for d in model.dids}),
        "did_details": [
            {
                "did": d.did_id,
                "name": d.name,
                "length_bytes": d.length_bytes,
                "callback": d.callback,
            }
            for d in model.dids
        ],
        "services": sorted({s.sid for s in model.services}),
        "dtc_count": len(model.dtcs),
        "dtcs": [
            {
                "code": dtc.dtc_code,
                "name": dtc.name,
                "severity": dtc.severity,
                "event_id": dtc.event_id,
            }
            for dtc in model.dtcs
        ],
        "emitted_by": f"odx_gen.build_mdd stub v{EMITTER_VERSION}",
        "emitted_on": datetime.now(timezone.utc).isoformat(),
    }
    body = json.dumps(payload, indent=2, sort_keys=True).encode("utf-8")
    return MDDSTUB_MAGIC + body


def write_mdd(model: EcuDiagnosticModel, output_path: Path, repo_root: Path) -> None:
    """Write `model` to `output_path` as an MDDSTUB blob, creating parent
    directories as needed."""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    blob = build_mdd_stub(model, repo_root)
    output_path.write_bytes(blob)
