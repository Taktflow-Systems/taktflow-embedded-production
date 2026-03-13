#!/usr/bin/env python3
"""
merge_xil_results.py — Combine SIL + HIL JUnit XML results and scenario
traceability into a unified xIL nightly report.

Outputs:
  combined-results/summary.txt       — Human-readable (also posted to GITHUB_STEP_SUMMARY)
  combined-results/coverage.json     — Machine-readable for dashboards
  combined-results/combined-junit.xml — Merged JUnit XML

Standards:
  ISO 26262 Part 6, Section 10 — Software Integration & Testing
  ASPICE SWE.5 / SWE.6

Traces: TSR-035, TSR-038, TSR-046, GAP-6
"""

import argparse
import glob
import json
import os
import sys
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path

import yaml


def parse_args():
    p = argparse.ArgumentParser(description="Merge SIL + HIL xIL test results")
    p.add_argument("--sil-results", default="sil-test-results/",
                    help="Directory containing SIL JUnit XML and logs")
    p.add_argument("--hil-results", default="hil-test-results/",
                    help="Directory containing HIL JUnit XML and logs")
    p.add_argument("--sil-scenarios", default="test/sil/scenarios/",
                    help="Directory containing SIL scenario YAML files")
    p.add_argument("--hil-scenarios", default="test/hil/scenarios/",
                    help="Directory containing HIL scenario YAML files")
    p.add_argument("--output", default="combined-results/",
                    help="Output directory for combined results")
    p.add_argument("--sil-status", default="success",
                    help="GitHub Actions job result for SIL (success/failure/skipped/cancelled)")
    p.add_argument("--hil-status", default="success",
                    help="GitHub Actions job result for HIL (success/failure/skipped/cancelled)")
    return p.parse_args()


def find_junit_xmls(results_dir):
    """Find all JUnit XML files in a results directory."""
    xmls = []
    if not os.path.isdir(results_dir):
        return xmls
    for pattern in ["**/*.xml", "**/*junit*.xml", "**/*results*.xml"]:
        xmls.extend(glob.glob(os.path.join(results_dir, pattern), recursive=True))
    # Deduplicate
    return list(set(xmls))


def parse_junit_xml(xml_path):
    """Parse a JUnit XML file and return test case dicts."""
    cases = []
    try:
        tree = ET.parse(xml_path)
    except ET.ParseError:
        return cases

    root = tree.getroot()

    # Handle both <testsuites> and <testsuite> root
    suites = []
    if root.tag == "testsuites":
        suites = root.findall("testsuite")
    elif root.tag == "testsuite":
        suites = [root]

    for suite in suites:
        suite_name = suite.get("name", "unknown")
        for tc in suite.findall("testcase"):
            case = {
                "name": tc.get("name", ""),
                "classname": tc.get("classname", suite_name),
                "time": float(tc.get("time", "0")),
                "status": "passed",
            }
            if tc.find("failure") is not None:
                case["status"] = "failed"
                case["message"] = tc.find("failure").get("message", "")
            elif tc.find("error") is not None:
                case["status"] = "error"
                case["message"] = tc.find("error").get("message", "")
            elif tc.find("skipped") is not None:
                case["status"] = "skipped"
                case["message"] = tc.find("skipped").get("message", "")
            cases.append(case)
    return cases


def parse_scenarios(scenario_dir):
    """Parse all YAML scenario files and extract verifies + asil fields."""
    scenarios = {}
    if not os.path.isdir(scenario_dir):
        return scenarios
    for yaml_path in sorted(glob.glob(os.path.join(scenario_dir, "*.yaml"))):
        try:
            with open(yaml_path, "r", encoding="utf-8") as f:
                data = yaml.safe_load(f)
        except (yaml.YAMLError, OSError):
            continue
        if not data or not isinstance(data, dict):
            continue
        scenario_id = data.get("id", Path(yaml_path).stem)
        scenarios[scenario_id] = {
            "name": data.get("name", ""),
            "file": os.path.basename(yaml_path),
            "verifies": data.get("verifies", []),
            "asil": data.get("asil", "QM"),
        }
    return scenarios


def build_coverage_matrix(sil_scenarios, hil_scenarios, sil_cases, hil_cases):
    """Build requirement coverage matrix from scenarios and test results."""
    # Map scenario ID -> test result status
    result_map = {}
    for case in sil_cases:
        # Try matching by classname or name containing scenario ID
        for key in [case["classname"], case["name"]]:
            result_map[key.upper()] = case["status"]

    for case in hil_cases:
        for key in [case["classname"], case["name"]]:
            result_map[key.upper()] = case["status"]

    # Build requirement -> test mapping
    req_coverage = {}  # req_id -> [{"scenario": id, "tier": sil/hil, "status": ...}]

    for scenarios, tier in [(sil_scenarios, "SIL"), (hil_scenarios, "HIL")]:
        for scenario_id, info in scenarios.items():
            # Determine pass/fail from JUnit results
            status = "unknown"
            sid_upper = scenario_id.upper()
            for key, val in result_map.items():
                if sid_upper in key or key in sid_upper:
                    status = val
                    break

            for req in info["verifies"]:
                if req not in req_coverage:
                    req_coverage[req] = []
                req_coverage[req].append({
                    "scenario": scenario_id,
                    "tier": tier,
                    "status": status,
                    "asil": info["asil"],
                })

    return req_coverage


def classify_req(req_id):
    """Classify a requirement ID into TSR, SSR, or SWR."""
    req_upper = req_id.upper()
    if req_upper.startswith("TSR"):
        return "TSR"
    elif req_upper.startswith("SSR"):
        return "SSR"
    elif req_upper.startswith("SWR"):
        return "SWR"
    return "OTHER"


def make_bar(pct, width=20):
    """Create a simple ASCII progress bar."""
    filled = int(pct / 100 * width)
    return "\u2588" * filled + "\u2591" * (width - filled)


def count_results(cases):
    """Count passed/failed/skipped/error from test cases."""
    counts = {"passed": 0, "failed": 0, "skipped": 0, "error": 0}
    for c in cases:
        counts[c["status"]] = counts.get(c["status"], 0) + 1
    return counts


def generate_combined_junit(sil_cases, hil_cases, output_path):
    """Generate a combined JUnit XML from both tiers."""
    root = ET.Element("testsuites")

    for tier_name, cases in [("SIL", sil_cases), ("HIL", hil_cases)]:
        if not cases:
            continue
        counts = count_results(cases)
        suite = ET.SubElement(root, "testsuite", {
            "name": tier_name,
            "tests": str(len(cases)),
            "failures": str(counts["failed"]),
            "errors": str(counts["error"]),
            "skipped": str(counts["skipped"]),
            "time": str(sum(c["time"] for c in cases)),
        })
        for c in cases:
            tc = ET.SubElement(suite, "testcase", {
                "name": c["name"],
                "classname": c["classname"],
                "time": str(c["time"]),
            })
            if c["status"] == "failed":
                ET.SubElement(tc, "failure", {"message": c.get("message", "")})
            elif c["status"] == "error":
                ET.SubElement(tc, "error", {"message": c.get("message", "")})
            elif c["status"] == "skipped":
                ET.SubElement(tc, "skipped", {"message": c.get("message", "")})

    tree = ET.ElementTree(root)
    ET.indent(tree, space="  ")
    tree.write(output_path, encoding="unicode", xml_declaration=True)


def generate_summary(sil_cases, hil_cases, sil_status, hil_status,
                     req_coverage, output_path):
    """Generate the human-readable summary.txt."""
    now = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    commit = os.environ.get("GITHUB_SHA", "local")[:8]

    sil_counts = count_results(sil_cases)
    hil_counts = count_results(hil_cases)
    total_passed = sil_counts["passed"] + hil_counts["passed"]
    total_failed = sil_counts["failed"] + hil_counts["failed"] + \
                   sil_counts["error"] + hil_counts["error"]
    total_skipped = sil_counts["skipped"] + hil_counts["skipped"]

    # Requirement classification
    req_by_type = {"TSR": set(), "SSR": set(), "SWR": set(), "OTHER": set()}
    req_all_pass = {"TSR": True, "SSR": True, "SWR": True, "OTHER": True}

    for req_id, tests in req_coverage.items():
        rtype = classify_req(req_id)
        req_by_type[rtype].add(req_id)
        for t in tests:
            if t["status"] in ("failed", "error"):
                req_all_pass[rtype] = False

    # ASIL breakdown
    asil_counts = {}
    asil_pass = {}
    all_scenarios = {}
    for tier_scenarios in [sil_cases, hil_cases]:
        for c in tier_scenarios:
            # We'll aggregate from coverage data below
            pass

    for req_id, tests in req_coverage.items():
        for t in tests:
            asil = t.get("asil", "QM")
            if asil not in asil_counts:
                asil_counts[asil] = 0
                asil_pass[asil] = True
            asil_counts[asil] += 1
            if t["status"] in ("failed", "error"):
                asil_pass[asil] = False

    lines = []
    lines.append("## xIL Nightly Test Results")
    lines.append("")
    lines.append(f"**Run date:** {now}")
    lines.append(f"**Commit:** `{commit}`")
    lines.append("")
    lines.append("### Tier Results")
    lines.append("")
    lines.append("| Tier | Passed | Failed | Skipped | Status |")
    lines.append("|------|--------|--------|---------|--------|")

    if sil_status == "skipped":
        lines.append("| SIL (vcan0 / Docker) | — | — | — | SKIPPED |")
    else:
        sil_icon = "PASS" if sil_counts["failed"] + sil_counts["error"] == 0 else "FAIL"
        lines.append(f"| SIL (vcan0 / Docker) | {sil_counts['passed']} | "
                     f"{sil_counts['failed'] + sil_counts['error']} | "
                     f"{sil_counts['skipped']} | {sil_icon} |")

    if hil_status == "skipped":
        lines.append("| HIL (can0 / Pi) | — | — | — | SKIPPED |")
    elif hil_status == "cancelled":
        lines.append("| HIL (can0 / Pi) | — | — | — | CANCELLED (runner unavailable?) |")
    else:
        hil_icon = "PASS" if hil_counts["failed"] + hil_counts["error"] == 0 else "FAIL"
        lines.append(f"| HIL (can0 / Pi) | {hil_counts['passed']} | "
                     f"{hil_counts['failed'] + hil_counts['error']} | "
                     f"{hil_counts['skipped']} | {hil_icon} |")

    lines.append(f"| **TOTAL** | **{total_passed}** | **{total_failed}** | "
                 f"**{total_skipped}** | |")
    lines.append("")

    # Requirement coverage
    lines.append("### Requirement Coverage")
    lines.append("")
    lines.append("| Type | Covered | Status |")
    lines.append("|------|---------|--------|")
    for rtype in ["TSR", "SSR", "SWR"]:
        count = len(req_by_type[rtype])
        status = "all PASS" if req_all_pass[rtype] and count > 0 else \
                 ("FAIL" if count > 0 else "—")
        lines.append(f"| {rtype} | {count} | {status} |")
    lines.append("")

    # ASIL breakdown
    if asil_counts:
        lines.append("### ASIL Breakdown")
        lines.append("")
        for asil in sorted(asil_counts.keys(),
                           key=lambda x: {"D": 0, "C": 1, "B": 2, "A": 3, "QM": 4}.get(x, 5)):
            icon = "PASS" if asil_pass[asil] else "FAIL"
            lines.append(f"- **ASIL {asil}**: {asil_counts[asil]} test(s) — {icon}")
        lines.append("")

    # Covered requirements list
    if req_coverage:
        lines.append("<details>")
        lines.append("<summary>Covered Requirements Detail</summary>")
        lines.append("")
        lines.append("| Requirement | Scenario | Tier | Status |")
        lines.append("|-------------|----------|------|--------|")
        for req_id in sorted(req_coverage.keys()):
            for t in req_coverage[req_id]:
                lines.append(f"| {req_id} | {t['scenario']} | {t['tier']} | {t['status']} |")
        lines.append("")
        lines.append("</details>")
        lines.append("")

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def generate_coverage_json(req_coverage, sil_cases, hil_cases,
                           sil_status, hil_status, output_path):
    """Generate machine-readable coverage.json."""
    sil_counts = count_results(sil_cases)
    hil_counts = count_results(hil_cases)

    data = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "commit": os.environ.get("GITHUB_SHA", "local")[:8],
        "tiers": {
            "sil": {
                "status": sil_status,
                "passed": sil_counts["passed"],
                "failed": sil_counts["failed"] + sil_counts["error"],
                "skipped": sil_counts["skipped"],
            },
            "hil": {
                "status": hil_status,
                "passed": hil_counts["passed"],
                "failed": hil_counts["failed"] + hil_counts["error"],
                "skipped": hil_counts["skipped"],
            },
        },
        "requirements": {},
    }

    for req_id, tests in sorted(req_coverage.items()):
        data["requirements"][req_id] = {
            "type": classify_req(req_id),
            "tests": tests,
            "all_pass": all(t["status"] == "passed" for t in tests
                           if t["status"] != "unknown"),
        }

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


def main():
    args = parse_args()

    os.makedirs(args.output, exist_ok=True)

    # Parse JUnit XMLs
    sil_xmls = find_junit_xmls(args.sil_results)
    hil_xmls = find_junit_xmls(args.hil_results)

    sil_cases = []
    for xml_path in sil_xmls:
        sil_cases.extend(parse_junit_xml(xml_path))

    hil_cases = []
    for xml_path in hil_xmls:
        hil_cases.extend(parse_junit_xml(xml_path))

    print(f"SIL: {len(sil_cases)} test cases from {len(sil_xmls)} XML files")
    print(f"HIL: {len(hil_cases)} test cases from {len(hil_xmls)} XML files")

    # Parse scenario YAML for traceability
    sil_scenarios = parse_scenarios(args.sil_scenarios)
    hil_scenarios = parse_scenarios(args.hil_scenarios)
    print(f"SIL scenarios: {len(sil_scenarios)}, HIL scenarios: {len(hil_scenarios)}")

    # Build coverage matrix
    req_coverage = build_coverage_matrix(
        sil_scenarios, hil_scenarios, sil_cases, hil_cases)
    print(f"Requirements covered: {len(req_coverage)}")

    # Generate outputs
    generate_combined_junit(
        sil_cases, hil_cases,
        os.path.join(args.output, "combined-junit.xml"))

    generate_summary(
        sil_cases, hil_cases, args.sil_status, args.hil_status,
        req_coverage, os.path.join(args.output, "summary.txt"))

    generate_coverage_json(
        req_coverage, sil_cases, hil_cases, args.sil_status, args.hil_status,
        os.path.join(args.output, "coverage.json"))

    print(f"Combined results written to {args.output}")

    # Exit with error if any completed tier failed
    if args.sil_status == "failure" or args.hil_status == "failure":
        print("One or more tiers failed.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
