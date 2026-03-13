#!/usr/bin/env python3
"""
HIL preflight audit.

Static audit focused on SIL/HIL divergence risks discovered in repository reviews.
Outputs machine + human readable findings for nightly triage.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import List


@dataclass
class Finding:
    id: str
    severity: str
    category: str
    file: str
    line: int
    title: str
    evidence: str
    recommendation: str


def read_lines(path: Path) -> List[str]:
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def find_first(lines: List[str], pattern: str) -> int:
    rx = re.compile(pattern)
    for i, line in enumerate(lines, start=1):
        if rx.search(line):
            return i
    return 0


def find_all(lines: List[str], pattern: str) -> List[int]:
    rx = re.compile(pattern)
    out: List[int] = []
    for i, line in enumerate(lines, start=1):
        if rx.search(line):
            out.append(i)
    return out


def add(findings: List[Finding], seq: int, severity: str, category: str, file: str, line: int, title: str, evidence: str, recommendation: str) -> int:
    findings.append(
        Finding(
            id=f"HIL-PF-{seq:03d}",
            severity=severity,
            category=category,
            file=file,
            line=line,
            title=title,
            evidence=evidence,
            recommendation=recommendation,
        )
    )
    return seq + 1


def audit(repo_root: Path) -> List[Finding]:
    findings: List[Finding] = []
    seq = 1

    # 1) MCAL boundary purity: SPI POSIX stub contains simulation + override behavior.
    spi_posix = repo_root / "firmware/shared/bsw/mcal/posix/Spi_Posix.c"
    spi_lines = read_lines(spi_posix)
    l_osc = find_first(spi_lines, r"oscillat|SPI_OVERRIDE_STEP|dead-zone")
    l_udp = find_first(spi_lines, r"UDP|SPI_PEDAL_UDP_PORT|recv\(")
    l_inj = find_first(spi_lines, r"Spi_Posix_InjectAngle")
    if l_osc and l_udp and l_inj:
        seq = add(
            findings,
            seq,
            "high",
            "boundary",
            spi_posix.relative_to(repo_root).as_posix(),
            min(l_osc, l_udp, l_inj),
            "MCAL stub mixes simulation dynamics and fault-injection transport",
            "Spi_Posix.c contains oscillation synthesis + UDP override + injection API in one MCAL unit.",
            "Move plant dynamics and fault injection out of MCAL; keep MCAL transport-only.",
        )

    # 2) Platform-specific safety behavior in critical modules.
    safety_paths = [
        repo_root / "firmware/cvc/src/Swc_VehicleState.c",
        repo_root / "firmware/fzc/src/Swc_FzcSafety.c",
    ]
    for p in safety_paths:
        lines = read_lines(p)
        hits = find_all(lines, r"#ifdef\s+PLATFORM_POSIX|#if\s+defined\(PLATFORM_POSIX\)")
        for ln in hits:
            seq = add(
                findings,
                seq,
                "high",
                "equivalence",
                p.relative_to(repo_root).as_posix(),
                ln,
                "Platform guard present in safety-critical logic path",
                "Conditional platform behavior detected in module with safety decisions.",
                "Consolidate to platform-equivalent behavior or document justified deviation with dedicated tests.",
            )

    # 3) SAFE_STOP recovery relay-state semantic check.
    vsm = repo_root / "firmware/cvc/src/Swc_VehicleState.c"
    vsm_lines = read_lines(vsm)
    l_comment = find_first(vsm_lines, r"RelayState.*1=energized.*0=killed")
    l_recover = find_first(vsm_lines, r"\(sc_relay_kill\s*==\s*0u\)")
    if l_comment and l_recover:
        seq = add(
            findings,
            seq,
            "critical",
            "state-recovery",
            vsm.relative_to(repo_root).as_posix(),
            l_recover,
            "Potential relay-state semantic mismatch in SAFE_STOP recovery gating",
            "Recovery condition uses sc_relay_kill == 0 while nearby comments describe 0 as killed.",
            "Align signal semantics/comment/condition and add regression tests for recovery eligibility.",
        )

    # 4) Brake sequencing check: feedback compare before fresh sensor read.
    brake = repo_root / "firmware/fzc/src/Swc_Brake.c"
    brake_lines = read_lines(brake)
    l_cmp = find_first(brake_lines, r"deviation\s*=\s*|Compare commanded duty")
    l_read = find_first(brake_lines, r"IoHwAb_ReadBrakePosition")
    if l_cmp and l_read and l_cmp < l_read:
        seq = add(
            findings,
            seq,
            "high",
            "actuation",
            brake.relative_to(repo_root).as_posix(),
            l_cmp,
            "Brake feedback comparison uses previous-cycle position",
            "Deviation check occurs before IoHwAb_ReadBrakePosition refresh.",
            "Read brake position first, then compute command-vs-feedback deviation in same cycle.",
        )

    # 5) Steering plausibility reference check.
    steer = repo_root / "firmware/fzc/src/Swc_Steering.c"
    steer_lines = read_lines(steer)
    l_plaus = find_first(steer_lines, r"Steering_AbsDiffSint16\(cmd_angle,\s*actual_angle\)")
    if l_plaus:
        seq = add(
            findings,
            seq,
            "medium",
            "actuation",
            steer.relative_to(repo_root).as_posix(),
            l_plaus,
            "Steering plausibility compares command vs actual (not post-limiter output)",
            "Plausibility uses cmd_angle while rate limiter can constrain actual commanded output.",
            "Compare actual feedback against the effective output angle after rate limiting.",
        )

    # 6) ESM init status.
    sc_main = repo_root / "firmware/sc/src/sc_main.c"
    sc_main_lines = read_lines(sc_main)
    l_esm_guard = find_first(sc_main_lines, r"#ifdef\s+SC_ESM_ENABLED")
    l_esm_call = find_first(sc_main_lines, r"SC_ESM_Init\(\)")
    if l_esm_guard and l_esm_call:
        seq = add(
            findings,
            seq,
            "high",
            "safety-mechanism",
            sc_main.relative_to(repo_root).as_posix(),
            l_esm_guard,
            "ESM lockstep init is compile-time optional",
            "SC_ESM_Init is guarded by SC_ESM_ENABLED with TODO note in startup path.",
            "Resolve root cause and enforce ESM init policy for hardware builds; document waiver if temporarily disabled.",
        )

    # 7) MPU presence note (informational pass/fail style).
    sc_startup = repo_root / "firmware/sc/src/sc_startup.S"
    sc_startup_lines = read_lines(sc_startup)
    l_mpu = find_first(sc_startup_lines, r"_mpuInit_|Enable MPU")
    if not l_mpu:
        seq = add(
            findings,
            seq,
            "high",
            "memory-protection",
            sc_startup.relative_to(repo_root).as_posix(),
            1,
            "No MPU initialization found in SC startup",
            "MPU init symbols were not detected in sc_startup.S.",
            "Add MPU region initialization and runtime verification hooks.",
        )

    return findings


def write_markdown(path: Path, findings: List[Finding]) -> None:
    sev_order = {"critical": 0, "high": 1, "medium": 2, "low": 3}
    ordered = sorted(findings, key=lambda x: (sev_order.get(x.severity, 9), x.file, x.line))

    lines: List[str] = []
    lines.append("# HIL Preflight Audit")
    lines.append("")
    lines.append(f"- Generated: `{datetime.now(timezone.utc).isoformat()}`")
    lines.append(f"- Findings: `{len(findings)}`")
    lines.append(
        f"- Severity: critical `{sum(1 for f in findings if f.severity == 'critical')}`, "
        f"high `{sum(1 for f in findings if f.severity == 'high')}`, "
        f"medium `{sum(1 for f in findings if f.severity == 'medium')}`, "
        f"low `{sum(1 for f in findings if f.severity == 'low')}`"
    )
    lines.append("")
    lines.append("| ID | Sev | Category | Location | Title |")
    lines.append("|---|---|---|---|---|")
    for f in ordered:
        lines.append(
            f"| {f.id} | {f.severity} | {f.category} | `{f.file}:{f.line}` | {f.title} |"
        )
    lines.append("")
    lines.append("## Detail")
    lines.append("")
    for f in ordered:
        lines.append(f"### {f.id} â€” {f.title}")
        lines.append(f"- Severity: `{f.severity}`")
        lines.append(f"- Category: `{f.category}`")
        lines.append(f"- Location: `{f.file}:{f.line}`")
        lines.append(f"- Evidence: {f.evidence}")
        lines.append(f"- Recommendation: {f.recommendation}")
        lines.append("")

    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--output-dir", type=Path, default=Path("build"))
    args = ap.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    findings = audit(repo_root)

    out = repo_root / args.output_dir
    out.mkdir(parents=True, exist_ok=True)

    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "summary": {
            "total": len(findings),
            "critical": sum(1 for f in findings if f.severity == "critical"),
            "high": sum(1 for f in findings if f.severity == "high"),
            "medium": sum(1 for f in findings if f.severity == "medium"),
            "low": sum(1 for f in findings if f.severity == "low"),
        },
        "findings": [asdict(f) for f in findings],
    }

    json_path = out / "hil-preflight-audit.json"
    md_path = out / "hil-preflight-audit.md"

    json_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    write_markdown(md_path, findings)

    print(f"Wrote: {json_path}")
    print(f"Wrote: {md_path}")


if __name__ == "__main__":
    main()
