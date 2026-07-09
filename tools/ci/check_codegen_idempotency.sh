#!/usr/bin/env bash
# check_codegen_idempotency.sh — S-AG-04 (docs/plans/memo-arxmlgen-idempotency.md)
#
# Proves the full DBC -> ARXML -> C pipeline is idempotent:
#   1. run the pipeline (CI-equivalent invocation, see .github/workflows/ci.yml)
#   2. snapshot every generated path
#   3. run the pipeline again
#   4. FAIL (exit 1) if run 2 changed any file relative to run 1
#
# With --against-head, additionally FAIL if run 1 differs from the committed
# baseline (usable as a CI gate once the regenerated baseline has landed,
# step S-AG-05).
#
# The script only reads/writes paths the pipeline itself generates; it never
# reverts or cleans unrelated working-tree changes.

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

GEN_PATHS=(
    arxml/TaktflowSystem.arxml
    arxml_v2/TaktflowSystem.arxml
    firmware/ecu
)

run_pipeline() {
    python tools/arxml/dbc2arxml.py gateway/taktflow_vehicle.dbc arxml_v2/ arxml_v2/swc_model.json > /dev/null
    cp arxml_v2/TaktflowSystem.arxml arxml/TaktflowSystem.arxml
    python -m tools.arxmlgen --config project.yaml --quiet > /dev/null
}

echo "[idempotency] pipeline run 1..."
run_pipeline

if [[ "${1:-}" == "--against-head" ]]; then
    if ! git diff --quiet -- "${GEN_PATHS[@]}"; then
        echo "FAIL: pipeline output differs from committed baseline (HEAD):" >&2
        git --no-pager diff --stat -- "${GEN_PATHS[@]}" >&2
        exit 1
    fi
    echo "[idempotency] run 1 matches committed baseline"
fi

SNAP="$(mktemp -d)"
trap 'rm -rf "$SNAP"' EXIT

for p in "${GEN_PATHS[@]}"; do
    mkdir -p "$SNAP/$(dirname "$p")"
    cp -r "$p" "$SNAP/$p"
done

echo "[idempotency] pipeline run 2..."
run_pipeline

fail=0
for p in "${GEN_PATHS[@]}"; do
    if ! diff -r -q "$SNAP/$p" "$p" > /dev/null 2>&1; then
        echo "FAIL: non-idempotent output under: $p" >&2
        # || true: head's early exit sends diff a SIGPIPE, which pipefail
        # would otherwise escalate past our fail=1 accounting
        { diff -r "$SNAP/$p" "$p" 2>&1 || true; } | head -40 >&2 || true
        fail=1
    fi
done

if [[ $fail -eq 0 ]]; then
    echo "OK: pipeline is idempotent (run 2 == run 1)"
fi
exit $fail
