#!/usr/bin/env bash
# =============================================================================
# run_hil_pi.sh — Run HIL tests on Pi (vECU station)
#
# Assumes vECU station is already running (deploy-pi.sh was run first).
# Can be invoked locally on Pi or remotely via SSH from dev PC.
#
# Usage (on Pi):
#   ./scripts/hil/run_hil_pi.sh
#   ./scripts/hil/run_hil_pi.sh --tests-only
#   ./scripts/hil/run_hil_pi.sh --scenarios-only
#   ./scripts/hil/run_hil_pi.sh --soak 120
#
# Usage (from PC):
#   ssh bench-pi@192.0.2.10 'cd taktflow-embedded-production && ./scripts/hil/run_hil_pi.sh'
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
RESULTS_DIR="${PROJECT_ROOT}/test/hil/results"

SCENARIOS_ONLY=false
TESTS_ONLY=false
SOAK_SEC=60

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scenarios-only) SCENARIOS_ONLY=true ;;
        --tests-only)     TESTS_ONLY=true ;;
        --soak)           SOAK_SEC="$2"; shift ;;
        *)                echo "Unknown: $1"; exit 1 ;;
    esac
    shift
done

echo "============================================"
echo "  Taktflow HIL Test Suite — Pi"
echo "  $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
echo "  Host: $(hostname) ($(uname -m))"
echo "============================================"
echo ""

mkdir -p "${RESULTS_DIR}"
export CAN_INTERFACE=can0
export MQTT_HOST=localhost
export PYTHONPATH="${PROJECT_ROOT}/test/hil:${PYTHONPATH:-}"
cd "${PROJECT_ROOT}"

# --- Preflight: can0 up? ---
if ! ip link show can0 up 2>/dev/null | grep -q "UP"; then
    echo "[WARN] can0 not up — setting up..."
    sudo ip link set can0 type can bitrate 500000 2>/dev/null || true
    sudo ip link set can0 up
fi

# --- Preflight: Docker vECUs running? ---
RUNNING=$(docker compose -f docker/docker-compose.hil-pi.yml ps --status running -q 2>/dev/null | wc -l)
if [ "$RUNNING" -lt 3 ]; then
    echo "[WARN] Only ${RUNNING} containers running (need ≥3: bcm, icu, tcu)"
    echo "       Start with: docker compose -f docker/docker-compose.hil-pi.yml up -d"
    echo ""
fi

# --- Preflight: Physical ECUs on bus? ---
echo "Checking CAN bus for physical ECU heartbeats (5s)..."
SEEN=$(timeout 5 candump -t z can0 2>/dev/null | awk '{print $3}' | sort -u || true)
for ID_NAME in "010:CVC" "011:FZC" "012:RZC" "013:SC"; do
    ID="${ID_NAME%%:*}"
    NAME="${ID_NAME##*:}"
    if echo "${SEEN}" | grep -qi "${ID}"; then
        echo "  [OK] ${NAME} (0x${ID})"
    else
        echo "  [--] ${NAME} (0x${ID}) not seen"
    fi
done
echo ""

FAIL_COUNT=0
PASS_COUNT=0
TOTAL=0

# --- Python hop-by-hop tests ---
if [ "${SCENARIOS_ONLY}" = false ]; then
    echo "=== Python Hop Tests ==="
    for test_file in test/hil/test_hil_*.py; do
        test_name=$(basename "${test_file}" .py)
        TOTAL=$((TOTAL + 1))
        echo ""
        echo ">>> ${test_name}"
        if python3 "${test_file}" 2>&1 | tee "${RESULTS_DIR}/${test_name}.log"; then
            PASS_COUNT=$((PASS_COUNT + 1))
        else
            FAIL_COUNT=$((FAIL_COUNT + 1))
        fi
    done
    echo ""
fi

# --- YAML scenario tests ---
if [ "${TESTS_ONLY}" = false ]; then
    echo "=== YAML Scenario Tests ==="
    TOTAL=$((TOTAL + 1))
    if python3 test/hil/hil_runner.py \
        --channel can0 \
        --interface socketcan \
        --mqtt-host localhost \
        2>&1 | tee "${RESULTS_DIR}/hil_scenarios.log"; then
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
    echo ""
fi

# --- Summary ---
echo "============================================"
echo "  HIL Results: ${PASS_COUNT}/${TOTAL} passed, ${FAIL_COUNT} failed"
echo "  Reports: ${RESULTS_DIR}/"
echo "============================================"

exit ${FAIL_COUNT}
