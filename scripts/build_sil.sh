#!/bin/bash
# build_sil.sh — Pull latest, build all SIL ECUs, report results
# Run on Ubuntu laptop: bash scripts/build_sil.sh

set -e
cd "$(dirname "$0")/.."

echo "=== Pull latest ==="
git pull --ff-only

echo ""
echo "=== Build SIL (POSIX) ==="
make -f firmware/platform/posix/Makefile.posix clean 2>/dev/null || true
make -f firmware/platform/posix/Makefile.posix build -j$(nproc) 2>&1 | tee /tmp/sil_build.log

ERRORS=$(grep -c "error:" /tmp/sil_build.log 2>/dev/null || echo 0)
WARNINGS=$(grep -c "warning:" /tmp/sil_build.log 2>/dev/null || echo 0)

echo ""
echo "============================================"
echo "BUILD RESULT"
echo "============================================"
echo "  Errors:   $ERRORS"
echo "  Warnings: $WARNINGS"

if [ "$ERRORS" -eq 0 ]; then
    echo "  STATUS:   PASS"
    echo ""
    echo "Built binaries:"
    find build/ -name "*.elf" -o -name "*_ecu" 2>/dev/null | sort
else
    echo "  STATUS:   FAIL"
    echo ""
    echo "First 20 errors:"
    grep "error:" /tmp/sil_build.log | head -20
    exit 1
fi
