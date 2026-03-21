#!/usr/bin/env python3
"""
Traceability Matrix Generator

Scans DBC (Satisfies), generated configs (@satisfies), SWC source (@safety_req),
and test files (@verifies) to produce a requirement → implementation → test matrix.

Output: docs/safety/verification/traceability-matrix.md

Usage: python tools/trace/gen_traceability_matrix.py
"""

import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Patterns to extract requirement IDs
REQ_PATTERNS = [
    (re.compile(r'@satisfies\s+(.+?)(?:\s*\*/|\s*$)', re.MULTILINE), 'satisfies'),
    (re.compile(r'@safety_req\s+(.+?)(?:\s*$)', re.MULTILINE), 'safety_req'),
    (re.compile(r'@traces_to\s+(.+?)(?:\s*$)', re.MULTILINE), 'traces_to'),
    (re.compile(r'@verifies\s+(.+?)(?:\s*$)', re.MULTILINE), 'verifies'),
]

# Requirement ID pattern
REQ_ID = re.compile(r'(SG-\d+|TSR-\d+|SSR-\w+-\d+|FSR-\d+|SWR-\w+-\d+|HE-\d+)')


def scan_files(patterns, root_dir):
    """Scan files for requirement references."""
    results = {}  # req_id → { 'implementations': set, 'tests': set, 'configs': set }

    for pattern_glob in patterns:
        for filepath in glob.glob(os.path.join(root_dir, pattern_glob), recursive=True):
            relpath = os.path.relpath(filepath, root_dir).replace('\\', '/')
            try:
                with open(filepath, 'r', errors='replace') as f:
                    content = f.read()
            except OSError:
                continue

            is_test = '/test/' in relpath or relpath.startswith('test/')
            is_config = '/cfg/' in relpath
            is_source = '/src/' in relpath or '/include/' in relpath

            for regex, tag in REQ_PATTERNS:
                for match in regex.finditer(content):
                    refs = REQ_ID.findall(match.group(1))
                    for req_id in refs:
                        if req_id not in results:
                            results[req_id] = {'implementations': set(), 'tests': set(), 'configs': set()}
                        if is_test:
                            results[req_id]['tests'].add(relpath)
                        elif is_config:
                            results[req_id]['configs'].add(relpath)
                        elif is_source:
                            results[req_id]['implementations'].add(relpath)
                        else:
                            results[req_id]['implementations'].add(relpath)

    return results


def main():
    file_patterns = [
        'firmware/ecu/*/src/*.c',
        'firmware/ecu/*/include/*.h',
        'firmware/ecu/*/cfg/*.c',
        'firmware/ecu/*/test/*.c',
        'firmware/bsw/**/src/*.c',
        'firmware/bsw/**/include/*.h',
        'firmware/bsw/test/*.c',
        'test/unit/**/*.c',
        'test/sil/**/*.py',
        'test/sil/**/*.yaml',
        'gateway/*.dbc',
        'tools/pipeline/*.py',
    ]

    print("Scanning for requirement references...")
    results = scan_files(file_patterns, ROOT)

    # Sort by requirement ID
    sorted_reqs = sorted(results.keys(), key=lambda r: (r.split('-')[0], int(r.split('-')[-1]) if r.split('-')[-1].isdigit() else 0))

    # Count stats
    total = len(sorted_reqs)
    with_impl = sum(1 for r in sorted_reqs if results[r]['implementations'] or results[r]['configs'])
    with_test = sum(1 for r in sorted_reqs if results[r]['tests'])
    full_trace = sum(1 for r in sorted_reqs if (results[r]['implementations'] or results[r]['configs']) and results[r]['tests'])

    # Generate markdown
    lines = []
    lines.append('# Traceability Matrix')
    lines.append('')
    lines.append(f'**Generated:** {__import__("datetime").datetime.now().strftime("%Y-%m-%d %H:%M")}')
    lines.append(f'**Requirements found:** {total}')
    lines.append(f'**With implementation:** {with_impl} ({100*with_impl//max(total,1)}%)')
    lines.append(f'**With test:** {with_test} ({100*with_test//max(total,1)}%)')
    lines.append(f'**Full trace (impl+test):** {full_trace} ({100*full_trace//max(total,1)}%)')
    lines.append('')
    lines.append('| Requirement | Implementation | Config | Test | Status |')
    lines.append('|-------------|---------------|--------|------|--------|')

    for req_id in sorted_reqs:
        data = results[req_id]
        impl = ', '.join(sorted(data['implementations']))[:80] or '—'
        cfg = ', '.join(sorted(data['configs']))[:60] or '—'
        test = ', '.join(sorted(data['tests']))[:60] or '—'

        has_impl = bool(data['implementations'] or data['configs'])
        has_test = bool(data['tests'])
        if has_impl and has_test:
            status = 'TRACED'
        elif has_impl:
            status = 'IMPL ONLY'
        elif has_test:
            status = 'TEST ONLY'
        else:
            status = 'ORPHAN'

        lines.append(f'| {req_id} | {impl} | {cfg} | {test} | **{status}** |')

    lines.append('')

    output_dir = os.path.join(ROOT, 'docs', 'safety', 'verification')
    os.makedirs(output_dir, exist_ok=True)
    output_path = os.path.join(output_dir, 'traceability-matrix.md')

    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))

    print(f"Written: {output_path}")
    print(f"  {total} requirements, {full_trace} fully traced, {total - full_trace} gaps")
    return 0


if __name__ == '__main__':
    sys.exit(main())
