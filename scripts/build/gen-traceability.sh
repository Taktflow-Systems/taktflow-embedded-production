#!/usr/bin/env bash
# =============================================================================
# HITL-LOCK START:gen-trace-header
# Human-in-the-Loop (HITL) Comment Lock
#
# HITL means human-reviewer-owned comment content.
# AI must never edit, reformat, move, or delete text inside any HITL-LOCK block.
# Append-only: AI may add new comments/changes only; prior HITL comments stay
# unchanged. If a locked comment needs revision, add a new note outside the
# lock or ask the human reviewer to unlock it.
# HITL-LOCK END:gen-trace-header
# =============================================================================
#
# gen-traceability.sh — Automated Bidirectional Traceability Matrix
#
# Extracts SWR-* requirement IDs from requirement docs, @verifies tags from
# unit/integration/SIL test files, and @safety_req / @traces_to / SWR-*
# references from source files.  Generates a markdown traceability matrix:
#
#     Requirement ID | Req Source | Source Files | Unit Tests |
#     Integration Tests | SIL Scenarios | Status
#
# Status logic:
#   COVERED  — at least one unit/integration/SIL test verifies the requirement
#   PARTIAL  — source code references exist but no test coverage
#   UNCOVERED — no test AND no source code reference
#
# Exit codes:
#   0 — all requirements have at least one test (COVERED)
#   1 — one or more requirements lack test coverage (PARTIAL or UNCOVERED)
#
# Standards:
#   ISO 26262 Part 6 — bidirectional traceability (SWR -> code -> test)
#   ASPICE SWE.4 / SWE.5 / SWE.6 — unit, integration, SW verification
#
# Usage:
#   bash scripts/gen-traceability.sh              Generate matrix
#   bash scripts/gen-traceability.sh --check      Check only (CI mode, exit 1 on gaps)
#   bash scripts/gen-traceability.sh --help       Show this help
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Help
# ---------------------------------------------------------------------------
if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    cat <<'USAGE'
gen-traceability.sh — Automated Bidirectional Traceability Matrix

USAGE:
  bash scripts/gen-traceability.sh              Generate matrix (write output file)
  bash scripts/gen-traceability.sh --check      Check only (CI mode, exit 1 on gaps)
  bash scripts/gen-traceability.sh --help       Show this help

OUTPUT:
  docs/aspice/verification/traceability-matrix.md

The script scans:
  1. Requirement docs   — docs/aspice/software/sw-requirements/SWR-*.md
  2. Unit tests         — firmware/ecu/*/test/test_*.c, test/unit/bsw/test_*.c
  3. Integration tests  — test/framework/test_int_*.c
  4. SIL scenarios      — test/sil/scenarios/*.yaml
  5. Source code        — firmware/ecu/*/src/*.c, firmware/bsw/**/*.c (excluding tests)

EXIT CODES:
  0 — All requirements have at least one test
  1 — One or more requirements lack test coverage (--check mode)
      In normal mode, always exits 0 but prints warnings

STANDARDS:
  ISO 26262 Part 6, ASPICE SWE.4 / SWE.5 / SWE.6
USAGE
    exit 0
fi

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DOCS_DIR="$ROOT_DIR/docs/aspice/software/sw-requirements"
FW_DIR="$ROOT_DIR/firmware"
INT_DIR="$ROOT_DIR/test/framework"
SIL_DIR="$ROOT_DIR/test/sil/scenarios"
OUTPUT="$ROOT_DIR/docs/aspice/verification/traceability-matrix.md"

CHECK_ONLY=false
if [ "${1:-}" = "--check" ]; then
    CHECK_ONLY=true
fi

# ---------------------------------------------------------------------------
# Temp files (cleaned up on exit)
# ---------------------------------------------------------------------------
REQ_FILE=$(mktemp)
REQ_SOURCE_FILE=$(mktemp)
UNIT_MAP_FILE=$(mktemp)
INT_MAP_FILE=$(mktemp)
SIL_MAP_FILE=$(mktemp)
SRC_MAP_FILE=$(mktemp)
BODY_FILE=$(mktemp)
trap 'rm -f "$REQ_FILE" "$REQ_SOURCE_FILE" "$UNIT_MAP_FILE" "$INT_MAP_FILE" "$SIL_MAP_FILE" "$SRC_MAP_FILE" "$BODY_FILE"' EXIT

echo "=== Traceability Matrix Generator ==="
echo "Root:         $ROOT_DIR"
echo "Requirements: $DOCS_DIR"
echo "Firmware:     $FW_DIR"
echo "Integration:  $INT_DIR"
echo "SIL:          $SIL_DIR"
echo ""

# -----------------------------------------------------------------------
# 1. Extract requirement IDs from SWR-*.md documents
# -----------------------------------------------------------------------
echo "--- [1/5] Extracting requirements from docs ---"

# Build mapping: REQ_ID -> source file (basename)
{
    for doc in "$DOCS_DIR"/SWR-*.md; do
        [ -f "$doc" ] || continue
        basename_doc=$(basename "$doc")
        grep -oE 'SWR-[A-Z]+-[0-9]+' "$doc" 2>/dev/null | sort -u | while IFS= read -r req_id; do
            printf '%s\t%s\n' "$req_id" "$basename_doc"
        done || true
    done
} | sort -t$'\t' -k1,1 -u > "$REQ_SOURCE_FILE"

# Unique requirement IDs only
cut -f1 "$REQ_SOURCE_FILE" | sort -u > "$REQ_FILE"

REQ_COUNT=$(wc -l < "$REQ_FILE")
echo "  Found $REQ_COUNT unique SWR-* requirements"

# -----------------------------------------------------------------------
# 2. Extract @verifies tags from unit test files
# -----------------------------------------------------------------------
echo "--- [2/5] Extracting unit test verifications ---"

# Unit tests: firmware/ecu/*/test/test_*.c and test/unit/bsw/test_*.c
while IFS= read -r tfile; do
    relpath="${tfile#"$ROOT_DIR/"}"
    # Extract @verifies lines, strip the tag prefix, split on commas
    grep -oE '@verifies[[:space:]]+(SWR-[A-Z]+-[0-9]+([[:space:]]*,[[:space:]]*SWR-[A-Z]+-[0-9]+)*)' "$tfile" 2>/dev/null \
        | sed 's/@verifies[[:space:]]*//' \
        | tr ',' '\n' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//' | while IFS= read -r req_id; do
            [ -n "$req_id" ] && printf '%s\t%s\n' "$req_id" "$relpath"
        done || true
done < <(find "$FW_DIR" -path '*/test/test_*.c' -type f 2>/dev/null) \
    | sort -t$'\t' -k1,1 > "$UNIT_MAP_FILE"

UNIT_REQ_COUNT=$(cut -f1 "$UNIT_MAP_FILE" | sort -u | wc -l)
echo "  Found $UNIT_REQ_COUNT unique requirements verified by unit tests"

# -----------------------------------------------------------------------
# 3. Extract @verifies tags from integration test files
# -----------------------------------------------------------------------
echo "--- [3/5] Extracting integration test verifications ---"

if [ -d "$INT_DIR" ]; then
    while IFS= read -r tfile; do
        relpath="${tfile#"$ROOT_DIR/"}"
        grep -oE '@verifies[[:space:]]+(SWR-[A-Z]+-[0-9]+([[:space:]]*,[[:space:]]*SWR-[A-Z]+-[0-9]+)*)' "$tfile" 2>/dev/null \
            | sed 's/@verifies[[:space:]]*//' \
            | tr ',' '\n' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//' | while IFS= read -r req_id; do
                [ -n "$req_id" ] && printf '%s\t%s\n' "$req_id" "$relpath"
            done || true
    done < <(find "$INT_DIR" -name 'test_int_*.c' -type f 2>/dev/null) \
        | sort -t$'\t' -k1,1 > "$INT_MAP_FILE"
else
    touch "$INT_MAP_FILE"
fi

INT_REQ_COUNT=$(cut -f1 "$INT_MAP_FILE" | sort -u | wc -l)
echo "  Found $INT_REQ_COUNT unique requirements verified by integration tests"

# -----------------------------------------------------------------------
# 4. Extract verifies from SIL scenario YAML files
# -----------------------------------------------------------------------
echo "--- [4/5] Extracting SIL scenario verifications ---"

if [ -d "$SIL_DIR" ]; then
    for yfile in "$SIL_DIR"/*.yaml; do
        [ -f "$yfile" ] || continue
        relpath="${yfile#"$ROOT_DIR/"}"
        # Parse YAML verifies: list — extract SWR IDs from lines like:  - "SWR-XXX-NNN"
        grep -oE 'SWR-[A-Z]+-[0-9]+' "$yfile" 2>/dev/null | while IFS= read -r req_id; do
            [ -n "$req_id" ] && printf '%s\t%s\n' "$req_id" "$relpath"
        done || true
    done | sort -t$'\t' -k1,1 > "$SIL_MAP_FILE"
else
    touch "$SIL_MAP_FILE"
fi

SIL_REQ_COUNT=$(cut -f1 "$SIL_MAP_FILE" | sort -u | wc -l)
echo "  Found $SIL_REQ_COUNT unique requirements verified by SIL scenarios"

# -----------------------------------------------------------------------
# 5. Extract SWR-* references from source code
# -----------------------------------------------------------------------
echo "--- [5/5] Extracting source code references ---"

# Source files: firmware/ecu/*/src/*.c, firmware/bsw/**/*.c and .h (excluding test dirs)
while IFS= read -r sfile; do
    relpath="${sfile#"$ROOT_DIR/"}"
    grep -oE 'SWR-[A-Z]+-[0-9]+' "$sfile" 2>/dev/null | sort -u | while IFS= read -r req_id; do
        printf '%s\t%s\n' "$req_id" "$relpath"
    done || true
done < <(find "$FW_DIR" \( -path '*/src/*.c' -o -path '*/bsw/*/*.c' -o -path '*/bsw/*/*.h' \) \
    ! -path '*/test/*' ! -path '*/unity/*' -type f 2>/dev/null) \
    | sort -t$'\t' -k1,1 > "$SRC_MAP_FILE"

SRC_REQ_COUNT=$(cut -f1 "$SRC_MAP_FILE" | sort -u | wc -l)
echo "  Found $SRC_REQ_COUNT unique requirements referenced in source code"

# -----------------------------------------------------------------------
# Helper: lookup tab-separated map file by req_id
# -----------------------------------------------------------------------
lookup() {
    # Usage: lookup "$req_id" "$MAP_FILE"
    # Returns tab-separated values from column 2 for matching column 1
    awk -F'\t' -v id="$1" '$1==id{print $2}' "$2"
}

# -----------------------------------------------------------------------
# 6. Build traceability matrix — first pass: generate rows and count
# -----------------------------------------------------------------------
echo ""
echo "--- Building traceability matrix ---"

COVERED=0
PARTIAL=0
UNCOVERED=0
UNCOVERED_LIST=""
PARTIAL_LIST=""

# Generate table rows to BODY_FILE while accumulating counts
{
    echo "| Requirement ID | Requirement Source | Source Files | Unit Tests | Integration Tests | SIL Scenarios | Status |"
    echo "|---|---|---|---|---|---|---|"

    while IFS= read -r req_id; do
        # Requirement source document
        req_src=$(lookup "$req_id" "$REQ_SOURCE_FILE" | head -1 || true)
        req_src="${req_src:-—}"

        # Source files referencing this requirement
        src_files=$(lookup "$req_id" "$SRC_MAP_FILE" | sort -u \
            | sed "s|firmware/bsw/||g; s|firmware/ecu/||g; s|firmware/||g" | tr '\n' ', ' | sed 's/,$//' || true)
        src_files="${src_files:-—}"

        # Unit test files
        unit_files=$(lookup "$req_id" "$UNIT_MAP_FILE" | sort -u \
            | sed "s|firmware/bsw/||g; s|firmware/ecu/||g; s|firmware/||g" | tr '\n' ', ' | sed 's/,$//' || true)
        unit_files="${unit_files:-—}"

        # Integration test files
        int_files=$(lookup "$req_id" "$INT_MAP_FILE" | sort -u \
            | sed "s|test/integration/||g" | tr '\n' ', ' | sed 's/,$//' || true)
        int_files="${int_files:-—}"

        # SIL scenario files
        sil_files=$(lookup "$req_id" "$SIL_MAP_FILE" | sort -u \
            | sed "s|test/sil/scenarios/||g" | tr '\n' ', ' | sed 's/,$//' || true)
        sil_files="${sil_files:-—}"

        # Determine status
        has_test=false
        has_source=false
        [ "$unit_files" != "—" ] && has_test=true
        [ "$int_files" != "—" ] && has_test=true
        [ "$sil_files" != "—" ] && has_test=true
        [ "$src_files" != "—" ] && has_source=true

        if $has_test; then
            status="COVERED"
            COVERED=$((COVERED + 1))
        elif $has_source; then
            status="**PARTIAL**"
            PARTIAL=$((PARTIAL + 1))
            PARTIAL_LIST="$PARTIAL_LIST $req_id"
        else
            status="**UNCOVERED**"
            UNCOVERED=$((UNCOVERED + 1))
            UNCOVERED_LIST="$UNCOVERED_LIST $req_id"
        fi

        # Truncate long file lists for table readability
        src_short=$(echo "$src_files" | cut -c1-55)
        [ ${#src_files} -gt 55 ] && src_short="${src_short}..."
        unit_short=$(echo "$unit_files" | cut -c1-55)
        [ ${#unit_files} -gt 55 ] && unit_short="${unit_short}..."
        int_short=$(echo "$int_files" | cut -c1-55)
        [ ${#int_files} -gt 55 ] && int_short="${int_short}..."
        sil_short=$(echo "$sil_files" | cut -c1-55)
        [ ${#sil_files} -gt 55 ] && sil_short="${sil_short}..."

        echo "| $req_id | $req_src | $src_short | $unit_short | $int_short | $sil_short | $status |"
    done < "$REQ_FILE"
} > "$BODY_FILE"

# -----------------------------------------------------------------------
# 7. Compute summary values
# -----------------------------------------------------------------------
TOTAL=$((COVERED + PARTIAL + UNCOVERED))
if [ "$TOTAL" -gt 0 ]; then
    PCT=$((COVERED * 100 / TOTAL))
else
    PCT=0
fi

# -----------------------------------------------------------------------
# 8. Assemble final output file (no sed -i needed)
# -----------------------------------------------------------------------

# Ensure output directory exists
mkdir -p "$(dirname "$OUTPUT")"

{
    # --- Document header ---
    cat <<'HEADER'
---
document_id: TM
title: "Bidirectional Traceability Matrix"
aspice_process: "SWE.4, SWE.5, SWE.6"
iso_reference: "ISO 26262 Part 6"
---

<!-- HITL-LOCK START:tm-header -->
## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.
<!-- HITL-LOCK END:tm-header -->


# Bidirectional Traceability Matrix

> **Auto-generated** by `scripts/gen-traceability.sh`
> Do not edit manually — regenerate after requirement or test changes.

**Standard references:**
- ISO 26262:2018 Part 6 — Software level product development
- Automotive SPICE 4.0 SWE.4 — Software unit verification
- Automotive SPICE 4.0 SWE.5 — Software component verification & integration
- Automotive SPICE 4.0 SWE.6 — Software verification

## Coverage Summary

HEADER

    # Summary table with actual computed values
    echo "| Metric | Count |"
    echo "|--------|-------|"
    echo "| Total requirements | $TOTAL |"
    echo "| Covered (has test) | $COVERED |"
    echo "| Partial (code only) | $PARTIAL |"
    echo "| Uncovered | $UNCOVERED |"
    echo "| **Coverage** | **${PCT}%** |"
    echo ""
    echo "## Traceability Matrix"
    echo ""

    # Append pre-generated table body
    cat "$BODY_FILE"

    # --- Summary section ---
    echo ""
    echo "## Summary"
    echo ""

    echo "- **Covered**: $COVERED / $TOTAL requirements have test coverage"
    echo "- **Partial**: $PARTIAL requirements have source code but no test"
    echo "- **Uncovered**: $UNCOVERED requirements have no test and no source reference"
    echo "- **Coverage**: ${PCT}%"

    if [ $PARTIAL -gt 0 ]; then
        echo ""
        echo "### Partial Requirements (code but no test)"
        echo ""
        for req in $PARTIAL_LIST; do
            echo "- \`$req\`"
        done
    fi

    if [ $UNCOVERED -gt 0 ]; then
        echo ""
        echo "### Uncovered Requirements"
        echo ""
        for req in $UNCOVERED_LIST; do
            echo "- \`$req\`"
        done
    fi

    echo ""
    TIMESTAMP=$(date -u '+%Y-%m-%d %H:%M UTC')
    echo "---"
    echo ""
    echo "*Generated: $TIMESTAMP*"

} > "$OUTPUT"

# -----------------------------------------------------------------------
# 9. Print summary to stdout
# -----------------------------------------------------------------------
echo ""
echo "=== Traceability Matrix Generated ==="
echo "Output: $OUTPUT"
echo ""
echo "  Total requirements:  $TOTAL"
echo "  Covered (tested):    $COVERED"
echo "  Partial (code only): $PARTIAL"
echo "  Uncovered:           $UNCOVERED"
echo "  Coverage:            ${PCT}%"

if [ $PARTIAL -gt 0 ]; then
    echo ""
    echo "PARTIAL requirements (code exists, no test):"
    for req in $PARTIAL_LIST; do
        echo "  - $req"
    done
fi

if [ $UNCOVERED -gt 0 ]; then
    echo ""
    echo "WARNING: UNCOVERED requirements (no test, no code):"
    for req in $UNCOVERED_LIST; do
        echo "  - $req"
    done
fi

# -----------------------------------------------------------------------
# 10. Exit code
# -----------------------------------------------------------------------
GAPS=$((PARTIAL + UNCOVERED))

if $CHECK_ONLY && [ $GAPS -gt 0 ]; then
    echo ""
    echo "FAIL: Traceability check failed — $GAPS requirements without test coverage"
    exit 1
elif [ $GAPS -gt 0 ]; then
    echo ""
    echo "NOTE: $GAPS requirements without full test coverage (run with --check to enforce)"
    exit 0
fi

echo ""
echo "=== All $TOTAL requirements have test coverage ==="
exit 0
