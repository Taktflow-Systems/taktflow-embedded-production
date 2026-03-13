#!/usr/bin/env python3
"""
Full V-Model Traceability Matrix Generator

Scans docs/ and firmware/ for requirement trace tags, builds a bidirectional
graph, validates links, and generates docs/aspice/traceability/traceability-matrix.md.

Pure stdlib — no pip dependencies.

Usage:
    python scripts/trace-gen.py                    # Generate matrix (default)
    python scripts/trace-gen.py --check            # CI mode: exit 1 on broken links/gaps
    python scripts/trace-gen.py --stats            # Print summary stats only
    python scripts/trace-gen.py --json             # Output as JSON
    python scripts/trace-gen.py --output FILE      # Custom output path

Tags scanned:
    Markdown:  **Traces up**: / **Traces down**: / **Source**: / **Verified by**: / **ASIL**:
    Code:      @verifies SWR-XXX-NNN, @safety_req SSR-XXX-NNN
    YAML:      verifies: ["SWR-XXX-NNN"]
"""

import argparse
import json
import os
import re
import sys
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent
DOCS_DIR = ROOT_DIR / "docs"
FW_DIR = ROOT_DIR / "firmware"
INT_DIR = ROOT_DIR / "test" / "integration"
SIL_DIR = ROOT_DIR / "test" / "sil" / "scenarios"
DEFAULT_OUTPUT = DOCS_DIR / "aspice" / "traceability" / "traceability-matrix.md"

# Requirement document map: (level, relative_path, heading_pattern)
# heading_pattern: regex to match the heading line and capture the ID
REQ_DOCS = [
    ("STK", "aspice/system/stakeholder-requirements.md",
     r"^#{2,4}\s+(STK-\d+):\s*(.+)"),
    ("SYS", "aspice/system/system-requirements.md",
     r"^#{2,4}\s+(SYS-\d+):\s*(.+)"),
    ("SG",  "safety/concept/safety-goals.md",
     r"^#{2,4}\s+(SG-\d+):\s*(.+)"),
    ("FSR", "safety/requirements/functional-safety-reqs.md",
     r"^#{2,4}\s+(FSR-\d+):\s*(.+)"),
    ("TSR", "safety/requirements/technical-safety-reqs.md",
     r"^#{2,4}\s+(TSR-\d+):\s*(.+)"),
    ("SSR", "safety/requirements/sw-safety-reqs.md",
     r"^#{2,4}\s+(SSR-[A-Z]+-\d+):\s*(.+)"),
    ("HSR", "safety/requirements/hw-safety-reqs.md",
     r"^#{2,4}\s+(HSR-[A-Z]+-\d+):\s*(.+)"),
]

# SWR files are per-ECU: docs/aspice/software/sw-requirements/SWR-*.md
SWR_DIR = DOCS_DIR / "aspice" / "software" / "sw-requirements"
SWR_HEADING = r"^#{2,4}\s+(SWR-[A-Z]+-\d+):\s*(.+)"

# Regex for extracting requirement IDs from free text
REQ_ID_RE = re.compile(
    r"(STK-\d+|SYS-\d+|SG-\d+|FSR-\d+|TSR-\d+|"
    r"SSR-[A-Z]+-\d+|HSR-[A-Z]+-\d+|SWR-[A-Z]+-\d+)"
)

# ASIL values
ASIL_ORDER = {"QM": 0, "A": 1, "B": 2, "C": 3, "D": 4}

# Level classification for requirement IDs
LEVEL_PREFIXES = [
    ("STK", re.compile(r"^STK-\d+$")),
    ("SYS", re.compile(r"^SYS-\d+$")),
    ("SG",  re.compile(r"^SG-\d+$")),
    ("FSR", re.compile(r"^FSR-\d+$")),
    ("TSR", re.compile(r"^TSR-\d+$")),
    ("SSR", re.compile(r"^SSR-[A-Z]+-\d+$")),
    ("HSR", re.compile(r"^HSR-[A-Z]+-\d+$")),
    ("SWR", re.compile(r"^SWR-[A-Z]+-\d+$")),
]


def classify_level(req_id):
    """Return the requirement level (STK, SYS, SG, etc.) for a given ID."""
    for level, pattern in LEVEL_PREFIXES:
        if pattern.match(req_id):
            return level
    return "UNKNOWN"


# ---------------------------------------------------------------------------
# Parsing helpers
# ---------------------------------------------------------------------------

def extract_ids(text):
    """Extract all requirement IDs from a text string."""
    return REQ_ID_RE.findall(text)


def parse_traces_field(line):
    """Parse a **Traces up/down** line, handling the FSR concatenation quirk.

    Known quirk: FSR docs have lines like:
        - **Traces down**: TSR-001, TSR-002- **Safety mechanism**: SM-001
    We must stop extracting IDs before the next `- **` marker.
    """
    # Split at any `- **` that starts a new field (the quirk)
    parts = re.split(r"-\s*\*\*(?:Safety mechanism|Allocation|Status)", line)
    return extract_ids(parts[0])


def parse_asil(text):
    """Extract ASIL level from a field value like 'ASIL D' or just 'D'."""
    text = text.strip()
    for level in ("D", "C", "B", "A", "QM"):
        if level in text:
            return level
    return None


# ---------------------------------------------------------------------------
# Requirement document parsers
# ---------------------------------------------------------------------------

def parse_requirements(filepath, heading_re):
    """Parse a requirement markdown file.

    Returns dict: {req_id: {title, asil, traces_up, traces_down, verified_by,
                            source, source_file}}
    """
    reqs = {}
    if not filepath.is_file():
        return reqs

    heading_pattern = re.compile(heading_re)
    current_id = None
    current_title = None
    relpath = str(filepath.relative_to(ROOT_DIR)).replace("\\", "/")

    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")

            # Check for new heading
            m = heading_pattern.match(line)
            if m:
                current_id = m.group(1)
                current_title = m.group(2).strip()
                reqs[current_id] = {
                    "title": current_title,
                    "asil": None,
                    "traces_up": [],
                    "traces_down": [],
                    "verified_by": [],
                    "source": [],
                    "source_file": relpath,
                }
                continue

            if current_id is None:
                continue

            entry = reqs[current_id]

            # Parse field lines (- **Field**: value)
            if line.startswith("- **Traces up**:"):
                entry["traces_up"] = parse_traces_field(line)
            elif line.startswith("- **Traces down**:"):
                entry["traces_down"] = parse_traces_field(line)
            elif line.startswith("- **ASIL**:") or line.startswith("- **Safety relevance**:"):
                entry["asil"] = parse_asil(line)
            elif line.startswith("- **Verified by**:"):
                # TC-XXX-NNN test case IDs (not requirement IDs)
                entry["verified_by"] = [
                    tc.strip() for tc in
                    re.findall(r"TC-[A-Z]+-\d+", line)
                ]
            elif line.startswith("- **Source**:"):
                # SG-specific: Source: HE-NNN
                entry["source"] = re.findall(r"HE-\d+", line)

    return reqs


def parse_swr_files():
    """Parse all SWR-*.md files in the sw-requirements directory."""
    all_reqs = {}
    if not SWR_DIR.is_dir():
        return all_reqs

    heading_pattern = re.compile(SWR_HEADING)
    for swr_file in sorted(SWR_DIR.glob("SWR-*.md")):
        reqs = parse_requirements(swr_file, SWR_HEADING)
        all_reqs.update(reqs)
    return all_reqs


# ---------------------------------------------------------------------------
# Code / test scanners
# ---------------------------------------------------------------------------

def parse_code_tags(fw_dir):
    """Scan firmware source files for @safety_req and @verifies tags.

    Returns:
        source_map: {req_id: [relative_paths]}
    """
    source_map = defaultdict(set)
    if not fw_dir.is_dir():
        return source_map

    # Source files: firmware/*/src/*.c, firmware/shared/bsw/**/*.c/*.h
    # Exclude test directories
    for src_file in fw_dir.rglob("*.c"):
        rel = str(src_file.relative_to(ROOT_DIR)).replace("\\", "/")
        if "/test/" in rel or "/unity/" in rel:
            continue
        try:
            content = src_file.read_text(encoding="utf-8", errors="replace")
        except (OSError, UnicodeDecodeError):
            continue
        # Extract @safety_req IDs
        for m in re.finditer(r"@safety_req\s+([\w, -]+)", content):
            for req_id in extract_ids(m.group(1)):
                source_map[req_id].add(rel)
        # Also extract bare SWR/SSR references in comments
        for m in re.finditer(r"(?:SWR|SSR)-[A-Z]+-\d+", content):
            source_map[m.group(0)].add(rel)

    # Also scan .h files
    for hdr_file in fw_dir.rglob("*.h"):
        rel = str(hdr_file.relative_to(ROOT_DIR)).replace("\\", "/")
        if "/test/" in rel or "/unity/" in rel:
            continue
        try:
            content = hdr_file.read_text(encoding="utf-8", errors="replace")
        except (OSError, UnicodeDecodeError):
            continue
        for m in re.finditer(r"@safety_req\s+([\w, -]+)", content):
            for req_id in extract_ids(m.group(1)):
                source_map[req_id].add(rel)

    return source_map


def parse_test_verifies(search_dir, pattern):
    """Scan test files matching a glob pattern for @verifies tags.

    Returns: {req_id: [relative_paths]}
    """
    test_map = defaultdict(set)
    if not search_dir.is_dir():
        return test_map

    for test_file in sorted(search_dir.rglob(pattern)):
        rel = str(test_file.relative_to(ROOT_DIR)).replace("\\", "/")
        try:
            content = test_file.read_text(encoding="utf-8", errors="replace")
        except (OSError, UnicodeDecodeError):
            continue
        # Extract @verifies tags — may span continuation lines
        for m in re.finditer(
            r"@verifies\s+((?:[\w-]+(?:\s*,\s*[\w-]+)*)(?:\s*\n\s*\*\s*(?:[\w-]+(?:\s*,\s*[\w-]+)*))*)",
            content
        ):
            for req_id in extract_ids(m.group(0)):
                test_map[req_id].add(rel)

    return test_map


def parse_sil_scenarios(sil_dir):
    """Scan SIL scenario YAML files for verifies: lists.

    Returns: {req_id: [relative_paths]}
    """
    sil_map = defaultdict(set)
    if not sil_dir.is_dir():
        return sil_map

    for yaml_file in sorted(sil_dir.glob("**/*.yaml")):
        rel = str(yaml_file.relative_to(ROOT_DIR)).replace("\\", "/")
        try:
            content = yaml_file.read_text(encoding="utf-8", errors="replace")
        except (OSError, UnicodeDecodeError):
            continue
        # Extract requirement IDs from verifies: list items
        in_verifies = False
        for line in content.split("\n"):
            stripped = line.strip()
            if stripped.startswith("verifies:"):
                in_verifies = True
                continue
            if in_verifies:
                if stripped.startswith("- "):
                    for req_id in extract_ids(stripped):
                        sil_map[req_id].add(rel)
                else:
                    in_verifies = False

    return sil_map


# ---------------------------------------------------------------------------
# Graph building
# ---------------------------------------------------------------------------

def build_graph(all_reqs, source_map, unit_map, int_map, sil_map):
    """Build a unified bidirectional graph from all parsed data.

    Returns: {req_id: {title, asil, level, parents, children,
                       source_files, unit_tests, int_tests, sil_scenarios,
                       verified_by, doc_file}}
    """
    graph = {}

    # Initialize nodes from parsed requirements
    for req_id, data in all_reqs.items():
        level = classify_level(req_id)
        graph[req_id] = {
            "title": data["title"],
            "asil": data["asil"],
            "level": level,
            "parents": list(data["traces_up"]),
            "children": list(data["traces_down"]),
            "source_files": sorted(source_map.get(req_id, set())),
            "unit_tests": sorted(unit_map.get(req_id, set())),
            "int_tests": sorted(int_map.get(req_id, set())),
            "sil_scenarios": sorted(sil_map.get(req_id, set())),
            "verified_by": data.get("verified_by", []),
            "doc_file": data["source_file"],
        }

    # Build bidirectional links:
    # If A.children contains B, ensure B.parents contains A (and vice versa)
    for req_id, node in list(graph.items()):
        for child_id in node["children"]:
            if child_id in graph and req_id not in graph[child_id]["parents"]:
                graph[child_id]["parents"].append(req_id)
        for parent_id in node["parents"]:
            if parent_id in graph and req_id not in graph[parent_id]["children"]:
                graph[parent_id]["children"].append(req_id)

    # Filter children to only requirement IDs (not code paths like firmware/...)
    for req_id, node in graph.items():
        node["children"] = [
            c for c in node["children"] if REQ_ID_RE.fullmatch(c)
        ]

    return graph


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------

def validate_links(graph, all_req_ids):
    """Run traceability validation checks.

    Returns: {broken_links, orphans, asymmetric, untested, asil_warnings}
    """
    issues = {
        "broken_links": [],     # Referenced ID doesn't exist
        "orphans": [],          # No parent AND no child
        "asymmetric": [],       # A->B but B-/>A (warning)
        "untested": [],         # Leaf req (SSR/SWR) with no test
        "asil_warnings": [],    # Child lower ASIL than parent
    }

    for req_id, node in graph.items():
        # Broken links: parent/child not in graph
        for parent_id in node["parents"]:
            if parent_id not in graph:
                issues["broken_links"].append(
                    f"{req_id} traces up to {parent_id} (not found)")
        for child_id in node["children"]:
            if child_id not in graph:
                issues["broken_links"].append(
                    f"{req_id} traces down to {child_id} (not found)")

        # Orphans: no parent AND no child (disconnected)
        if not node["parents"] and not node["children"]:
            # STK requirements are top-level, SG sources from HE (not in graph)
            if node["level"] not in ("STK", "SG"):
                issues["orphans"].append(req_id)

        # Asymmetric links
        for child_id in node["children"]:
            if child_id in graph and req_id not in graph[child_id]["parents"]:
                issues["asymmetric"].append(
                    f"{req_id} -> {child_id} (child doesn't trace back)")

        # Untested leaf requirements (SWR only — SSR checked transitively)
        if node["level"] == "SWR":
            has_test = (
                node["unit_tests"] or node["int_tests"] or
                node["sil_scenarios"]
            )
            if not has_test:
                issues["untested"].append(req_id)

        # ASIL consistency: child should not be lower than parent
        if node["asil"] and node["asil"] in ASIL_ORDER:
            parent_asil_val = ASIL_ORDER[node["asil"]]
            for child_id in node["children"]:
                if child_id in graph:
                    child_node = graph[child_id]
                    if (child_node["asil"] and
                            child_node["asil"] in ASIL_ORDER):
                        if ASIL_ORDER[child_node["asil"]] < parent_asil_val:
                            issues["asil_warnings"].append(
                                f"{child_id} ({child_node['asil']}) < "
                                f"{req_id} ({node['asil']})")

    return issues


# ---------------------------------------------------------------------------
# Coverage statistics
# ---------------------------------------------------------------------------

def _has_test_coverage(graph, req_id, visited=None):
    """Check if a requirement has direct or transitive test coverage.

    Transitive: if any child requirement is tested, parent counts as covered.
    This handles SSR -> SWR chains where tests use @verifies SWR-* tags.
    """
    if visited is None:
        visited = set()
    if req_id in visited:
        return False
    visited.add(req_id)

    node = graph.get(req_id)
    if not node:
        return False

    # Direct test coverage
    if node["unit_tests"] or node["int_tests"] or node["sil_scenarios"]:
        return True

    # Transitive: check children
    for child_id in node["children"]:
        if _has_test_coverage(graph, child_id, visited):
            return True

    return False


def compute_stats(graph):
    """Compute per-level coverage statistics.

    Returns: [{level, total, traced_up, traced_down, tested, coverage_pct}]
    """
    level_order = ["STK", "SYS", "SG", "FSR", "TSR", "SSR", "HSR", "SWR"]
    stats = []

    for level in level_order:
        reqs = {k: v for k, v in graph.items() if v["level"] == level}
        total = len(reqs)
        if total == 0:
            continue

        traced_up = sum(1 for v in reqs.values() if v["parents"])
        traced_down = sum(1 for v in reqs.values() if v["children"])

        # Direct test coverage
        tested_direct = sum(
            1 for v in reqs.values()
            if v["unit_tests"] or v["int_tests"] or v["sil_scenarios"]
        )
        # Transitive test coverage (includes children's tests)
        tested_transitive = sum(
            1 for k in reqs
            if _has_test_coverage(graph, k)
        )

        # Use transitive for mid/upper levels, direct for SWR
        if level == "SWR":
            tested = tested_direct
        else:
            tested = tested_transitive

        # Coverage metric per level type
        if level in ("STK", "SG"):
            # Top-level: coverage = traced down
            coverage_pct = (traced_down * 100 // total) if total else 0
        elif level in ("SSR", "HSR"):
            # Safety leaf: coverage = transitive test coverage
            coverage_pct = (tested * 100 // total) if total else 0
        elif level == "SWR":
            # SW leaf: coverage = direct test coverage
            coverage_pct = (tested * 100 // total) if total else 0
        else:
            # Mid level: coverage = traced both ways
            both = sum(
                1 for v in reqs.values()
                if v["parents"] and v["children"]
            )
            coverage_pct = (both * 100 // total) if total else 0

        stats.append({
            "level": level,
            "total": total,
            "traced_up": traced_up,
            "traced_down": traced_down,
            "tested": tested,
            "coverage_pct": coverage_pct,
        })

    return stats


# ---------------------------------------------------------------------------
# Output: Markdown traceability matrix
# ---------------------------------------------------------------------------

def build_chain_rows(graph):
    """Build full V-model traceability chain rows.

    Each row traces from a Safety Goal down through FSR -> TSR -> SSR/HSR -> SWR,
    including source and test references.
    """
    rows = []

    # Start from SG and trace down
    sgs = sorted(k for k, v in graph.items() if v["level"] == "SG")
    for sg_id in sgs:
        sg = graph[sg_id]
        fsrs = sorted(c for c in sg["children"] if classify_level(c) == "FSR")
        if not fsrs:
            rows.append(_chain_row(graph, sg_id, "—", "—", "—", "—"))
            continue
        for fsr_id in fsrs:
            fsr = graph.get(fsr_id, {})
            tsrs = sorted(
                c for c in fsr.get("children", [])
                if classify_level(c) == "TSR"
            )
            if not tsrs:
                rows.append(_chain_row(graph, sg_id, fsr_id, "—", "—", "—"))
                continue
            for tsr_id in tsrs:
                tsr = graph.get(tsr_id, {})
                ssrs_hsrs = sorted(
                    c for c in tsr.get("children", [])
                    if classify_level(c) in ("SSR", "HSR")
                )
                if not ssrs_hsrs:
                    rows.append(_chain_row(
                        graph, sg_id, fsr_id, tsr_id, "—", "—"))
                    continue
                for sh_id in ssrs_hsrs:
                    sh = graph.get(sh_id, {})
                    # Find related SWR via children or same TSR parent
                    swrs = sorted(
                        c for c in tsr.get("children", [])
                        if classify_level(c) == "SWR"
                    )
                    if not swrs:
                        rows.append(_chain_row(
                            graph, sg_id, fsr_id, tsr_id, sh_id, "—"))
                    else:
                        for swr_id in swrs:
                            rows.append(_chain_row(
                                graph, sg_id, fsr_id, tsr_id, sh_id, swr_id))

    # Also include SWR requirements traced via SYS (non-safety path)
    # These won't appear in the SG chain — add them separately
    swr_in_chain = set()
    for row in rows:
        if row["swr"] != "—":
            swr_in_chain.add(row["swr"])

    for req_id, node in sorted(graph.items()):
        if node["level"] == "SWR" and req_id not in swr_in_chain:
            rows.append(_chain_row(graph, "—", "—", "—", "—", req_id))

    return rows


def _chain_row(graph, sg, fsr, tsr, ssr_hsr, swr):
    """Build a single chain row dict."""
    # Determine test status and source from the leaf nodes
    test_files = set()
    source_files = set()
    sil_files = set()

    for req_id in (ssr_hsr, swr):
        if req_id != "—" and req_id in graph:
            node = graph[req_id]
            test_files.update(node["unit_tests"])
            test_files.update(node["int_tests"])
            sil_files.update(node["sil_scenarios"])
            source_files.update(node["source_files"])

    has_test = bool(test_files) or bool(sil_files)
    has_source = bool(source_files)

    if has_test:
        status = "COVERED"
    elif has_source:
        status = "PARTIAL"
    elif swr == "—" and ssr_hsr == "—":
        status = "—"
    else:
        status = "UNCOVERED"

    return {
        "sg": sg, "fsr": fsr, "tsr": tsr, "ssr_hsr": ssr_hsr, "swr": swr,
        "source": _trunc(", ".join(sorted(source_files)), 40),
        "test": _trunc(", ".join(sorted(test_files)), 40),
        "sil": _trunc(", ".join(sorted(sil_files)), 30),
        "status": status,
    }


def _trunc(s, maxlen):
    """Truncate string with ellipsis."""
    if not s:
        return "—"
    if len(s) <= maxlen:
        return s
    return s[:maxlen - 3] + "..."


def generate_markdown(graph, stats, issues, chain_rows):
    """Generate the full traceability matrix markdown."""
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    total_reqs = len(graph)

    lines = []
    lines.append("---")
    lines.append('document_id: TRACE-FULL')
    lines.append('title: "Full V-Model Traceability Matrix"')
    lines.append('aspice_processes: "SYS.1-5, SWE.1-6"')
    lines.append('iso_26262_part: "3, 4, 5, 6"')
    lines.append(f'generated: "{ts}"')
    lines.append("---")
    lines.append("")
    lines.append("# Full V-Model Traceability Matrix")
    lines.append("")
    lines.append("> **Auto-generated** by `scripts/trace-gen.py`")
    lines.append("> Do not edit manually — regenerate after requirement "
                 "or test changes.")
    lines.append("")
    lines.append("**Standard references:**")
    lines.append("- ISO 26262:2018 Parts 3-6 — Functional safety lifecycle")
    lines.append("- Automotive SPICE 4.0 SYS.1-5, SWE.1-6 — "
                 "V-model traceability")
    lines.append("")

    # Coverage summary
    lines.append("## Coverage Summary")
    lines.append("")
    lines.append(f"**Total requirements**: {total_reqs}")
    lines.append("")
    lines.append("| Level | Total | Traced Up | Traced Down | Tested | "
                 "Coverage |")
    lines.append("|-------|-------|-----------|-------------|--------|"
                 "----------|")
    for s in stats:
        up_str = "—" if s["level"] in ("STK", "SG") else str(s["traced_up"])
        tested_str = str(s["tested"]) if s["tested"] else "—"
        lines.append(
            f"| {s['level']} | {s['total']} | {up_str} | "
            f"{s['traced_down']} | {tested_str} | "
            f"{s['coverage_pct']}% |"
        )
    lines.append("")

    # Full traceability chains (safety path)
    lines.append("## Full Traceability Chains")
    lines.append("")
    lines.append("| SG | FSR | TSR | SSR/HSR | SWR | Source | Test | "
                 "SIL | Status |")
    lines.append("|---|---|---|---|---|---|---|---|---|")

    for row in chain_rows:
        status_cell = row["status"]
        if status_cell in ("PARTIAL", "UNCOVERED"):
            status_cell = f"**{status_cell}**"
        lines.append(
            f"| {row['sg']} | {row['fsr']} | {row['tsr']} | "
            f"{row['ssr_hsr']} | {row['swr']} | {row['source']} | "
            f"{row['test']} | {row['sil']} | {status_cell} |"
        )
    lines.append("")

    # Per-level requirement index
    lines.append("## Requirement Index by Level")
    lines.append("")
    level_order = ["STK", "SYS", "SG", "FSR", "TSR", "SSR", "HSR", "SWR"]
    for level in level_order:
        reqs = sorted(k for k, v in graph.items() if v["level"] == level)
        if not reqs:
            continue
        lines.append(f"### {level} ({len(reqs)} requirements)")
        lines.append("")
        lines.append("| ID | Title | ASIL | Parents | Children | Tested |")
        lines.append("|---|---|---|---|---|---|")
        for req_id in reqs:
            node = graph[req_id]
            asil = node["asil"] or "—"
            parents = ", ".join(node["parents"][:3]) or "—"
            if len(node["parents"]) > 3:
                parents += "..."
            children = ", ".join(node["children"][:3]) or "—"
            if len(node["children"]) > 3:
                children += "..."
            has_test = bool(
                node["unit_tests"] or node["int_tests"] or
                node["sil_scenarios"]
            )
            tested = "Yes" if has_test else "—"
            lines.append(
                f"| {req_id} | {_trunc(node['title'], 50)} | {asil} | "
                f"{parents} | {children} | {tested} |"
            )
        lines.append("")

    # Gap analysis
    lines.append("## Gap Analysis")
    lines.append("")

    if issues["broken_links"]:
        lines.append("### Broken Links")
        lines.append("")
        for msg in sorted(issues["broken_links"]):
            lines.append(f"- {msg}")
        lines.append("")
    else:
        lines.append("### Broken Links")
        lines.append("")
        lines.append("None found.")
        lines.append("")

    if issues["orphans"]:
        lines.append("### Orphan Requirements")
        lines.append("")
        for req_id in sorted(issues["orphans"]):
            lines.append(f"- `{req_id}`")
        lines.append("")
    else:
        lines.append("### Orphan Requirements")
        lines.append("")
        lines.append("None found.")
        lines.append("")

    if issues["untested"]:
        lines.append("### Untested Requirements (SSR/SWR without test)")
        lines.append("")
        for req_id in sorted(issues["untested"]):
            lines.append(f"- `{req_id}`")
        lines.append("")
    else:
        lines.append("### Untested Requirements")
        lines.append("")
        lines.append("None found.")
        lines.append("")

    if issues["asymmetric"]:
        lines.append("### Asymmetric Links (warning)")
        lines.append("")
        for msg in sorted(issues["asymmetric"]):
            lines.append(f"- {msg}")
        lines.append("")

    if issues["asil_warnings"]:
        lines.append("### ASIL Consistency Warnings")
        lines.append("")
        for msg in sorted(issues["asil_warnings"]):
            lines.append(f"- {msg}")
        lines.append("")

    lines.append("---")
    lines.append("")
    lines.append(f"*Generated: {ts}*")
    lines.append("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Output: JSON
# ---------------------------------------------------------------------------

def generate_json(graph, stats, issues):
    """Generate machine-readable JSON output."""
    return json.dumps({
        "generated": datetime.now(timezone.utc).isoformat(),
        "total_requirements": len(graph),
        "stats": stats,
        "issues": issues,
        "requirements": {
            k: {
                "title": v["title"],
                "asil": v["asil"],
                "level": v["level"],
                "parents": v["parents"],
                "children": v["children"],
                "source_files": v["source_files"],
                "unit_tests": v["unit_tests"],
                "int_tests": v["int_tests"],
                "sil_scenarios": v["sil_scenarios"],
                "verified_by": v["verified_by"],
                "doc_file": v["doc_file"],
            }
            for k, v in sorted(graph.items())
        },
    }, indent=2)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def print_stats(stats, issues, total):
    """Print summary statistics to stdout."""
    print("=== V-Model Traceability Summary ===")
    print(f"Total requirements: {total}")
    print()
    print(f"{'Level':<6} {'Total':>6} {'Up':>6} {'Down':>6} "
          f"{'Tested':>7} {'Coverage':>9}")
    print("-" * 48)
    for s in stats:
        up = "-" if s["level"] in ("STK", "SG") else str(s["traced_up"])
        tested = str(s["tested"]) if s["tested"] else "-"
        print(f"{s['level']:<6} {s['total']:>6} {up:>6} "
              f"{s['traced_down']:>6} {tested:>7} "
              f"{s['coverage_pct']:>8}%")
    print()
    print(f"Broken links:   {len(issues['broken_links'])}")
    print(f"Orphans:        {len(issues['orphans'])}")
    print(f"Untested:       {len(issues['untested'])}")
    print(f"Asymmetric:     {len(issues['asymmetric'])}")
    print(f"ASIL warnings:  {len(issues['asil_warnings'])}")


def main():
    parser = argparse.ArgumentParser(
        description="Full V-Model Traceability Matrix Generator")
    parser.add_argument(
        "--check", action="store_true",
        help="CI mode: exit 1 on broken links or untested leaf requirements")
    parser.add_argument(
        "--stats", action="store_true",
        help="Print summary stats only (no file output)")
    parser.add_argument(
        "--json", action="store_true",
        help="Output as JSON instead of Markdown")
    parser.add_argument(
        "--output", type=str, default=None,
        help="Custom output file path")
    args = parser.parse_args()

    print("=== Full V-Model Traceability Generator ===")
    print(f"Root: {ROOT_DIR}")
    print()

    # 1. Parse all requirement documents
    print("--- [1/5] Parsing requirement documents ---")
    all_reqs = {}
    for level, relpath, heading_re in REQ_DOCS:
        filepath = DOCS_DIR / relpath
        reqs = parse_requirements(filepath, heading_re)
        all_reqs.update(reqs)
        print(f"  {level}: {len(reqs)} requirements ({relpath})")

    # Parse SWR files
    swr_reqs = parse_swr_files()
    all_reqs.update(swr_reqs)
    print(f"  SWR: {len(swr_reqs)} requirements "
          f"(aspice/software/sw-requirements/SWR-*.md)")
    print(f"  Total: {len(all_reqs)} requirements parsed")
    print()

    # 2. Scan code for @safety_req tags
    print("--- [2/5] Scanning source code ---")
    source_map = parse_code_tags(FW_DIR)
    print(f"  {len(source_map)} requirements referenced in source code")

    # 3. Scan tests for @verifies tags
    print("--- [3/5] Scanning test files ---")
    unit_map = parse_test_verifies(FW_DIR, "test_*.c")
    print(f"  Unit tests: {len(unit_map)} requirements verified")
    int_map = parse_test_verifies(INT_DIR, "test_int_*.c")
    print(f"  Integration tests: {len(int_map)} requirements verified")

    # 4. Scan SIL scenarios
    print("--- [4/5] Scanning SIL scenarios ---")
    sil_map = parse_sil_scenarios(SIL_DIR)
    print(f"  SIL scenarios: {len(sil_map)} requirements verified")
    print()

    # 5. Build graph and validate
    print("--- [5/5] Building graph and validating ---")
    graph = build_graph(all_reqs, source_map, unit_map, int_map, sil_map)
    all_req_ids = set(graph.keys())
    issues = validate_links(graph, all_req_ids)
    stats = compute_stats(graph)
    print()

    # Print stats
    print_stats(stats, issues, len(graph))

    # Output
    if args.stats:
        sys.exit(0)

    if args.json:
        output_path = Path(args.output) if args.output else None
        json_str = generate_json(graph, stats, issues)
        if output_path:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(json_str, encoding="utf-8")
            print(f"\nJSON written to: {output_path}")
        else:
            print()
            print(json_str)
        sys.exit(0)

    # Generate markdown
    chain_rows = build_chain_rows(graph)
    md = generate_markdown(graph, stats, issues, chain_rows)
    output_path = Path(args.output) if args.output else DEFAULT_OUTPUT
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(md, encoding="utf-8")
    print(f"\nMatrix written to: {output_path}")

    # CI check mode
    if args.check:
        errors = len(issues["broken_links"]) + len(issues["untested"])
        if errors > 0:
            print(f"\nFAIL: {errors} traceability issues found "
                  f"({len(issues['broken_links'])} broken links, "
                  f"{len(issues['untested'])} untested)")
            sys.exit(1)
        else:
            print("\nPASS: No broken links or untested leaf requirements")
            sys.exit(0)

    sys.exit(0)


if __name__ == "__main__":
    main()
