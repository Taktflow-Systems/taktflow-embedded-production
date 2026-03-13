#!/usr/bin/env bash
# decision-audit.sh — Daily decision compliance scanner
#
# Scans all docs/ for decisions and checks compliance with the
# decision justification rule:
#   1. Explanation/rationale
#   2. Effort (cost + time)
#   3. Two alternatives with effort comparison
#
# Usage: ./scripts/decision-audit.sh
#
# Exit codes:
#   0 = all decisions compliant
#   1 = non-compliant decisions found

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DECISION_LOG="$REPO_ROOT/docs/aspice/plans/MAN.3-project-management/decision-log.md"
FAIL_COUNT=0
PASS_COUNT=0
TOTAL=0

echo "============================================"
echo "  DECISION AUDIT — $(date +%Y-%m-%d)"
echo "============================================"
echo ""

# --- Check 1: Tagged decisions (<!-- DECISION: ADR-NNN -->) ---
echo "--- Check 1: Tagged inline decisions ---"
TAGGED=$(grep -rn "<!-- DECISION:" "$REPO_ROOT/docs/" 2>/dev/null || true)
if [ -n "$TAGGED" ]; then
    echo "$TAGGED" | while IFS= read -r line; do
        FILE=$(echo "$line" | cut -d: -f1)
        ADR_ID=$(echo "$line" | grep -oP 'ADR-\d+' || echo "UNKNOWN")
        echo "  Found: $ADR_ID in $FILE"
        # Check if this ADR exists in decision log
        if ! grep -q "$ADR_ID" "$DECISION_LOG" 2>/dev/null; then
            echo "    WARNING: $ADR_ID not in decision-log.md"
        fi
    done
else
    echo "  No tagged inline decisions found."
fi
echo ""

# --- Check 2: Decision log completeness ---
echo "--- Check 2: Decision log ADR completeness ---"
# Extract ADR IDs from quick reference table
ADR_IDS=$(grep -oP 'ADR-\d+' "$DECISION_LOG" | sort -u)
for ADR in $ADR_IDS; do
    TOTAL=$((TOTAL + 1))
    # Check if ADR has a full section (## ADR-NNN:)
    if grep -q "^## $ADR:" "$DECISION_LOG"; then
        # Check required fields
        SECTION=$(sed -n "/^## $ADR:/,/^## ADR-/p" "$DECISION_LOG" | head -60)
        HAS_TIER=$(echo "$SECTION" | grep -c "Tier" || true)
        HAS_SCORES=$(echo "$SECTION" | grep -c "Scores" || true)
        HAS_RATIONALE=$(echo "$SECTION" | grep -c "Rationale" || true)
        HAS_EFFORT=$(echo "$SECTION" | grep -c "Effort" || true)
        HAS_ALT_A=$(echo "$SECTION" | grep -c "Alternative A" || true)
        HAS_ALT_B=$(echo "$SECTION" | grep -c "Alternative B" || true)
        HAS_WHY=$(echo "$SECTION" | grep -c "Why chosen wins" || true)

        if [ "$HAS_TIER" -ge 1 ] && [ "$HAS_SCORES" -ge 1 ] && \
           [ "$HAS_RATIONALE" -ge 1 ] && [ "$HAS_EFFORT" -ge 2 ] && \
           [ "$HAS_ALT_A" -ge 1 ] && [ "$HAS_ALT_B" -ge 1 ] && [ "$HAS_WHY" -ge 1 ]; then
            echo "  PASS: $ADR"
            PASS_COUNT=$((PASS_COUNT + 1))
        else
            echo "  FAIL: $ADR — missing:"
            [ "$HAS_TIER" -lt 1 ] && echo "    - Tier (T1-T4)"
            [ "$HAS_SCORES" -lt 1 ] && echo "    - Scores (Cost/Time/Safety/Resume)"
            [ "$HAS_RATIONALE" -lt 1 ] && echo "    - Rationale"
            [ "$HAS_EFFORT" -lt 2 ] && echo "    - Effort (need chosen + alternatives)"
            [ "$HAS_ALT_A" -lt 1 ] && echo "    - Alternative A"
            [ "$HAS_ALT_B" -lt 1 ] && echo "    - Alternative B"
            [ "$HAS_WHY" -lt 1 ] && echo "    - Why chosen wins"
            FAIL_COUNT=$((FAIL_COUNT + 1))
        fi
    else
        echo "  FAIL: $ADR — no detailed section found (only in quick reference)"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done
echo ""

# --- Check 3: Untagged decisions (heuristic) ---
echo "--- Check 3: Untagged decision heuristic scan ---"
echo "  Searching for potential untagged decisions..."
HEURISTIC=$(grep -rn -i \
    -e "we chose\|we decided\|decision to use\|selected over\|rather than\|instead of\|we use .* over\|chosen because" \
    "$REPO_ROOT/docs/" \
    --include="*.md" 2>/dev/null \
    | grep -v "decision-log.md" \
    | grep -v "DECISION:" \
    | grep -v "process-playbook.md" \
    | grep -v "lessons-learned" \
    || true)
if [ -n "$HEURISTIC" ]; then
    echo "  Potential untagged decisions found:"
    echo "$HEURISTIC" | while IFS= read -r line; do
        echo "    $line"
    done
    echo ""
    echo "  ACTION: Review above lines. If they are decisions, add <!-- DECISION: ADR-NNN --> tag"
    echo "          and create entry in decision-log.md"
else
    echo "  No untagged decisions detected."
fi
echo ""

# --- Summary ---
echo "============================================"
echo "  SUMMARY"
echo "============================================"
echo "  Total ADRs:      $TOTAL"
echo "  Compliant:       $PASS_COUNT"
echo "  Non-compliant:   $FAIL_COUNT"
echo "  Compliance rate:  $(( TOTAL > 0 ? PASS_COUNT * 100 / TOTAL : 0 ))%"
echo "============================================"

if [ "$FAIL_COUNT" -gt 0 ]; then
    echo ""
    echo "  ACTION REQUIRED: Fix $FAIL_COUNT non-compliant decisions in decision-log.md"
    exit 1
else
    echo ""
    echo "  All decisions compliant."
    exit 0
fi
