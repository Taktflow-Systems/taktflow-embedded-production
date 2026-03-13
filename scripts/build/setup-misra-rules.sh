#!/usr/bin/env bash
# =============================================================================
# setup-misra-rules.sh — Download official MISRA C rule headline texts
#
# Downloads from MISRA's public GitLab into tools/misra/.
# These texts are NOT redistributable — keep them out of version control.
#
# Usage:
#   bash scripts/setup-misra-rules.sh
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MISRA_DIR="$PROJECT_ROOT/tools/misra"

# MISRA official GitLab rule texts for cppcheck
MISRA_GITLAB_BASE="https://gitlab.com/MISRA/MISRA-C/MISRA-C-2012/tools/-/raw/main"
RULE_TEXT_FILE="misra_c_2023__headlines_for_cppcheck.txt"
TARGET_PATH="$MISRA_DIR/$RULE_TEXT_FILE"

mkdir -p "$MISRA_DIR"

echo "=== Downloading MISRA C rule texts ==="
echo "Source: $MISRA_GITLAB_BASE/$RULE_TEXT_FILE"
echo "Target: $TARGET_PATH"

if [ -f "$TARGET_PATH" ]; then
    echo "Rule texts already exist at $TARGET_PATH"
    echo "Delete the file and re-run to refresh."
    exit 0
fi

if command -v curl &>/dev/null; then
    curl -fSL "$MISRA_GITLAB_BASE/$RULE_TEXT_FILE" -o "$TARGET_PATH"
elif command -v wget &>/dev/null; then
    wget -q "$MISRA_GITLAB_BASE/$RULE_TEXT_FILE" -O "$TARGET_PATH"
else
    echo "ERROR: Neither curl nor wget found. Install one and retry."
    exit 1
fi

# Verify we got something meaningful (not an error page)
if [ ! -s "$TARGET_PATH" ]; then
    echo "ERROR: Downloaded file is empty. Check the URL or network."
    rm -f "$TARGET_PATH"
    exit 1
fi

LINE_COUNT=$(wc -l < "$TARGET_PATH")
echo "=== Downloaded $LINE_COUNT rule text lines ==="
echo ""
echo "NOTE: These rule texts are MISRA copyright. Do NOT commit to version control."
echo "      They are already in .gitignore (tools/misra/*.txt)."
