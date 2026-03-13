#!/usr/bin/env bash
# baseline-tag.sh — Create a baseline (tagged release) on main
#
# Usage: ./scripts/baseline-tag.sh <version> <baseline-id> "<description>"
# Example: ./scripts/baseline-tag.sh v0.1.0 BL-001 "Phase 0 complete"
#
# Stub — implementation deferred until first baseline.

set -euo pipefail

VERSION="${1:-}"
BASELINE="${2:-}"
DESCRIPTION="${3:-}"

if [ -z "$VERSION" ] || [ -z "$BASELINE" ] || [ -z "$DESCRIPTION" ]; then
    echo "Usage: $0 <version> <baseline-id> \"<description>\""
    echo "Example: $0 v0.1.0 BL-001 \"Phase 0 complete\""
    exit 1
fi

echo "baseline-tag.sh: Would create baseline:"
echo "  Version: $VERSION"
echo "  Baseline: $BASELINE"
echo "  Description: $DESCRIPTION"
echo ""
echo "Steps (to be automated):"
echo "  1. Verify on main branch"
echo "  2. Verify all tests pass"
echo "  3. Create annotated tag: git tag -a $VERSION -m \"$BASELINE: $DESCRIPTION\""
echo "  4. Push tag: git push origin $VERSION"
echo "  5. Create baseline record in docs/aspice/cm/baselines/"
echo ""
echo "Stub — implement when ready for first baseline."
