#!/usr/bin/env python3
"""
Suspect Link Detection — Change Impact Analysis

Compares the current branch against a base ref to find which requirement IDs
were modified, then walks the traceability graph downstream to show impact.

Pure stdlib — no pip dependencies.

Usage:
    python scripts/suspect-links.py --base origin/main --json /tmp/graph.json
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

# Requirement heading pattern: ### SYS-042: Title
HEADING_RE = re.compile(
    r"^[+-]\s*#{2,4}\s+"
    r"(STK-\d+|SYS-\d+|SG-\d+|FSR-\d+|TSR-\d+|"
    r"SSR-[A-Z]+-\d+|HSR-[A-Z]+-\d+|SWR-[A-Z]+-\d+)"
    r":\s*"
)

# Requirement ID pattern for inline reference changes
REQ_ID_RE = re.compile(
    r"(STK-\d+|SYS-\d+|SG-\d+|FSR-\d+|TSR-\d+|"
    r"SSR-[A-Z]+-\d+|HSR-[A-Z]+-\d+|SWR-[A-Z]+-\d+)"
)

# Docs directory (relative to repo root)
DOC_GLOBS = ["docs/**/*.md"]


def git_diff_docs(base_ref):
    """Get unified diff of docs/ against the base ref."""
    try:
        result = subprocess.run(
            ["git", "diff", f"{base_ref}...HEAD", "--unified=0", "--", "docs/"],
            capture_output=True, text=True, check=True,
        )
        return result.stdout
    except subprocess.CalledProcessError:
        return ""


def extract_changed_ids(diff_text):
    """Extract requirement IDs from changed heading lines in the diff."""
    changed = set()
    for line in diff_text.splitlines():
        # Only look at added/removed lines (not context)
        if not line.startswith(("+", "-")):
            continue
        # Skip diff headers
        if line.startswith(("+++", "---")):
            continue
        m = HEADING_RE.match(line)
        if m:
            changed.add(m.group(1))
    return changed


def load_graph(json_path):
    """Load the traceability graph JSON produced by trace-gen.py."""
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data.get("requirements", {})


def walk_downstream(graph, req_id, visited=None):
    """Recursively collect all downstream (children) requirements."""
    if visited is None:
        visited = set()
    if req_id in visited:
        return []
    visited.add(req_id)

    node = graph.get(req_id)
    if not node:
        return []

    children = []
    for child_id in sorted(node.get("children", [])):
        # Only follow requirement IDs (skip source file paths)
        if REQ_ID_RE.fullmatch(child_id):
            child_node = graph.get(child_id, {})
            has_tests = bool(
                child_node.get("unit_tests")
                or child_node.get("int_tests")
                or child_node.get("sil_scenarios")
            )
            subtree = walk_downstream(graph, child_id, visited)
            children.append({
                "id": child_id,
                "title": child_node.get("title", ""),
                "tested": has_tests,
                "children": subtree,
            })
    return children


def format_tree(children, indent=0):
    """Format a downstream tree as markdown bullet list."""
    lines = []
    prefix = "  " * indent + "- "
    for child in children:
        status = " (TESTED)" if child["tested"] else ""
        title = f" -- {child['title']}" if child["title"] else ""
        lines.append(f"{prefix}{child['id']}{title}{status}")
        lines.extend(format_tree(child["children"], indent + 1))
    return lines


def main():
    parser = argparse.ArgumentParser(
        description="Detect suspect links from requirement changes")
    parser.add_argument(
        "--base", required=True,
        help="Git ref to diff against (e.g. origin/main)")
    parser.add_argument(
        "--json", required=True, dest="json_path",
        help="Path to traceability graph JSON from trace-gen.py")
    args = parser.parse_args()

    # Get diff and find changed requirement IDs
    diff_text = git_diff_docs(args.base)
    if not diff_text:
        print("No doc changes detected -- no suspect links.")
        sys.exit(0)

    changed_ids = extract_changed_ids(diff_text)
    if not changed_ids:
        print("No requirement heading changes detected -- no suspect links.")
        sys.exit(0)

    # Load graph
    graph = load_graph(args.json_path)

    # Build output
    lines = []
    lines.append("## Suspect Links -- Requirements Changed in This PR")
    lines.append("")

    for req_id in sorted(changed_ids):
        node = graph.get(req_id, {})
        title = node.get("title", "Unknown")
        lines.append(f"### {req_id}: {title} (MODIFIED)")
        lines.append("")

        downstream = walk_downstream(graph, req_id)
        if downstream:
            lines.append("Downstream impact:")
            lines.extend(format_tree(downstream))
            lines.append("")
            lines.append(
                f"**Action required**: Review all downstream requirements "
                f"for consistency with changes to {req_id}."
            )
        else:
            lines.append("No downstream requirements found (leaf requirement).")
        lines.append("")

    output = "\n".join(lines)
    print(output)
    sys.exit(0)


if __name__ == "__main__":
    main()
