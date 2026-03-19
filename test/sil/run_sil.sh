#!/usr/bin/env bash
# =============================================================================
# @file    run_sil.sh
# @brief   SIL test orchestrator — starts Docker SIL platform, runs verdict
#          checker against all scenario YAML files, collects results
# @aspice  SWE.5 — SW Component Verification & Integration
# @iso     ISO 26262 Part 6, Section 10 — Software Integration & Testing
# @date    2026-02-24
#
# Usage:
#   ./run_sil.sh                      Run all scenarios
#   ./run_sil.sh --scenario=overcurrent  Run a single scenario
#   ./run_sil.sh --keep               Keep containers running after tests
#   ./run_sil.sh --timeout=120        Override default scenario timeout (sec)
#
# Exit codes:
#   0 — All verdicts PASS
#   1 — One or more verdicts FAIL or infrastructure error
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Color output helpers
# ---------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[PASS]${NC}  $*"; }
fail()  { echo -e "${RED}[FAIL]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
header(){ echo -e "\n${BOLD}=== $* ===${NC}"; }

# ---------------------------------------------------------------------------
# Paths (relative to this script)
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# Accept docker-compose.yml or docker-compose.dev.yml (production layout)
if [ -f "$REPO_ROOT/docker/docker-compose.yml" ]; then
    COMPOSE_FILE="$REPO_ROOT/docker/docker-compose.yml"
elif [ -f "$REPO_ROOT/docker/docker-compose.dev.yml" ]; then
    COMPOSE_FILE="$REPO_ROOT/docker/docker-compose.dev.yml"
else
    COMPOSE_FILE="$REPO_ROOT/docker/docker-compose.yml"
fi
SCENARIOS_DIR="$SCRIPT_DIR/scenarios"
RESULTS_DIR="$SCRIPT_DIR/results"
VERDICT_CHECKER="$SCRIPT_DIR/verdict_checker.py"
REQUIREMENTS="$SCRIPT_DIR/requirements.txt"

# Timestamp for this run
RUN_TS="$(date +%Y%m%d_%H%M%S)"
RUN_RESULTS_DIR="$RESULTS_DIR/$RUN_TS"

# ---------------------------------------------------------------------------
# Default configuration
# ---------------------------------------------------------------------------
KEEP_CONTAINERS=false
SINGLE_SCENARIO=""
SCENARIO_LIST=""
DEFAULT_TIMEOUT=60
HEALTH_WAIT_MAX=120     # Max seconds to wait for services to become healthy
HEALTH_POLL_INTERVAL=3  # Seconds between health checks
FAULT_API_URL="http://localhost:8091"
MQTT_PORT=1883
SIL_SCALE="${SIL_TIME_SCALE:-1}"

# ---------------------------------------------------------------------------
# Parse command-line arguments
# ---------------------------------------------------------------------------
for arg in "$@"; do
    case "$arg" in
        --keep)
            KEEP_CONTAINERS=true
            ;;
        --scenario=*)
            SINGLE_SCENARIO="${arg#*=}"
            ;;
        --scenarios=*)
            SCENARIO_LIST="${arg#*=}"
            ;;
        --timeout=*)
            DEFAULT_TIMEOUT="${arg#*=}"
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --scenario=NAME   Run a single scenario YAML file by name"
            echo "  --keep            Keep Docker containers running after tests"
            echo "  --timeout=N       Override default per-scenario timeout (seconds)"
            echo "  --help, -h        Show this help message"
            echo ""
            echo "Scenarios are YAML files in: $SCENARIOS_DIR/"
            echo "Results are saved to:        $RESULTS_DIR/<timestamp>/"
            exit 0
            ;;
        *)
            fail "Unknown argument: $arg"
            fail "Use --help for usage information."
            exit 1
            ;;
    esac
done

# ---------------------------------------------------------------------------
# Prerequisite checks
# ---------------------------------------------------------------------------
header "Prerequisite Checks"

check_command() {
    if ! command -v "$1" &>/dev/null; then
        fail "Required command not found: $1"
        echo "  Install $1 before running SIL tests."
        exit 1
    fi
    ok "$1 found: $(command -v "$1")"
}

check_command docker
check_command python3

# Verify docker compose plugin is available (v2 syntax, no hyphen)
if ! docker compose version &>/dev/null; then
    fail "Required command not found: docker compose (v2 plugin)"
    info "Install with: apt install docker-compose-plugin"
    exit 1
fi
ok "docker compose found: $(docker compose version --short)"

# Verify Docker daemon is running
if ! docker info &>/dev/null; then
    fail "Docker daemon is not running. Start Docker first."
    exit 1
fi
ok "Docker daemon is running"

# Verify compose file exists
if [ ! -f "$COMPOSE_FILE" ]; then
    fail "Docker compose file not found: $COMPOSE_FILE"
    exit 1
fi
ok "Compose file found: $COMPOSE_FILE"

# Verify verdict checker exists
if [ ! -f "$VERDICT_CHECKER" ]; then
    fail "Verdict checker not found: $VERDICT_CHECKER"
    exit 1
fi
ok "Verdict checker found"

# Verify scenarios directory exists and has YAML files
if [ ! -d "$SCENARIOS_DIR" ]; then
    fail "Scenarios directory not found: $SCENARIOS_DIR"
    exit 1
fi

if [ -n "$SINGLE_SCENARIO" ]; then
    SCENARIO_FILES=("$SCENARIOS_DIR/${SINGLE_SCENARIO}.yaml")
    if [ ! -f "${SCENARIO_FILES[0]}" ]; then
        SCENARIO_FILES=("$SCENARIOS_DIR/${SINGLE_SCENARIO}.yml")
        if [ ! -f "${SCENARIO_FILES[0]}" ]; then
            fail "Scenario file not found: ${SINGLE_SCENARIO}.yaml or ${SINGLE_SCENARIO}.yml"
            exit 1
        fi
    fi
elif [ -n "$SCENARIO_LIST" ]; then
    # Comma-separated list of scenario names (without extension)
    SCENARIO_FILES=()
    IFS=',' read -ra NAMES <<< "$SCENARIO_LIST"
    for name in "${NAMES[@]}"; do
        name="$(echo "$name" | xargs)"  # trim whitespace
        f="$SCENARIOS_DIR/${name}.yaml"
        [ ! -f "$f" ] && f="$SCENARIOS_DIR/${name}.yml"
        if [ ! -f "$f" ]; then
            fail "Scenario file not found: ${name}.yaml or ${name}.yml"
            exit 1
        fi
        SCENARIO_FILES+=("$f")
    done
else
    SCENARIO_FILES=()
    for f in "$SCENARIOS_DIR"/*.yaml "$SCENARIOS_DIR"/*.yml; do
        [ -f "$f" ] && SCENARIO_FILES+=("$f")
    done
fi

if [ ${#SCENARIO_FILES[@]} -eq 0 ]; then
    warn "No scenario YAML files found in $SCENARIOS_DIR/"
    warn "Create scenario files to run SIL tests."
    exit 1
fi
ok "Found ${#SCENARIO_FILES[@]} scenario file(s)"

# Install Python dependencies if needed
header "Python Dependencies"
if ! python3 -c "import yaml; import requests; import can; import junit_xml" 2>/dev/null; then
    info "Installing Python dependencies from $REQUIREMENTS"
    python3 -m pip install -r "$REQUIREMENTS" --quiet
    ok "Dependencies installed"
else
    ok "All Python dependencies already installed"
fi

# ---------------------------------------------------------------------------
# Create results directory
# ---------------------------------------------------------------------------
mkdir -p "$RUN_RESULTS_DIR"
info "Results will be saved to: $RUN_RESULTS_DIR"

# ---------------------------------------------------------------------------
# Start SIL platform
# ---------------------------------------------------------------------------
header "Starting SIL Platform"

info "Bringing up Docker services from $COMPOSE_FILE"
docker compose -f "$COMPOSE_FILE" up --build -d 2>&1 | tee "$RUN_RESULTS_DIR/docker_startup.log"

if [ $? -ne 0 ]; then
    fail "Docker compose up failed. Check $RUN_RESULTS_DIR/docker_startup.log"
    exit 1
fi
ok "Docker services started"

# ---------------------------------------------------------------------------
# Wait for services to become healthy
# ---------------------------------------------------------------------------
header "Waiting for Services"

wait_for_health() {
    local url="$1"
    local name="$2"
    local elapsed=0

    while [ $elapsed -lt $HEALTH_WAIT_MAX ]; do
        if curl -sf "$url" >/dev/null 2>&1; then
            ok "$name is healthy ($url)"
            return 0
        fi
        sleep "$HEALTH_POLL_INTERVAL"
        elapsed=$((elapsed + HEALTH_POLL_INTERVAL))
        info "Waiting for $name... (${elapsed}s / ${HEALTH_WAIT_MAX}s)"
    done

    fail "$name did not become healthy within ${HEALTH_WAIT_MAX}s"
    return 1
}

wait_for_port() {
    local port="$1"
    local name="$2"
    local elapsed=0

    while [ $elapsed -lt $HEALTH_WAIT_MAX ]; do
        if python3 -c "import socket; s=socket.socket(); s.settimeout(1); s.connect(('localhost',$port)); s.close()" 2>/dev/null; then
            ok "$name is reachable on port $port"
            return 0
        fi
        sleep "$HEALTH_POLL_INTERVAL"
        elapsed=$((elapsed + HEALTH_POLL_INTERVAL))
        info "Waiting for $name on port $port... (${elapsed}s / ${HEALTH_WAIT_MAX}s)"
    done

    fail "$name did not become reachable on port $port within ${HEALTH_WAIT_MAX}s"
    return 1
}

# Check fault injection API
HEALTH_OK=true
if ! wait_for_health "$FAULT_API_URL/api/fault/health" "Fault Injection API"; then
    HEALTH_OK=false
fi

# Check MQTT broker
if ! wait_for_port "$MQTT_PORT" "MQTT Broker"; then
    HEALTH_OK=false
fi

if [ "$HEALTH_OK" = false ]; then
    fail "One or more services failed health checks"
    info "Saving container logs before exit..."
    docker compose -f "$COMPOSE_FILE" logs --no-color > "$RUN_RESULTS_DIR/docker_logs.txt" 2>&1
    if [ "$KEEP_CONTAINERS" = false ]; then
        info "Tearing down containers..."
        docker compose -f "$COMPOSE_FILE" down 2>/dev/null
    fi
    exit 1
fi

# Brief stabilization pause — let all ECUs complete init and reach RUN state
STAB_SEC=$(( 5 / SIL_SCALE ))
[ "$STAB_SEC" -lt 1 ] && STAB_SEC=1
info "Stabilization pause (${STAB_SEC}s, scale=${SIL_SCALE}) — waiting for ECUs to initialize..."
sleep "$STAB_SEC"
ok "All services healthy and stabilized"

# ---------------------------------------------------------------------------
# Run verdict checker
# ---------------------------------------------------------------------------
header "Running SIL Test Scenarios"

VERDICT_ARGS=()
VERDICT_ARGS+=("--results-dir" "$RUN_RESULTS_DIR")
VERDICT_ARGS+=("--timeout" "$DEFAULT_TIMEOUT")
VERDICT_ARGS+=("--fault-api-url" "$FAULT_API_URL")
VERDICT_ARGS+=("--mqtt-port" "$MQTT_PORT")

for scenario_file in "${SCENARIO_FILES[@]}"; do
    VERDICT_ARGS+=("--scenario" "$scenario_file")
done

info "Executing verdict checker with ${#SCENARIO_FILES[@]} scenario(s)..."
echo ""

# Run the verdict checker, tee output to both console and log file
set +e
python3 "$VERDICT_CHECKER" "${VERDICT_ARGS[@]}" 2>&1 | tee "$RUN_RESULTS_DIR/verdict_output.log"
VERDICT_EXIT=${PIPESTATUS[0]}
set -e

echo ""

# ---------------------------------------------------------------------------
# Collect container logs
# ---------------------------------------------------------------------------
header "Collecting Logs"

info "Saving Docker container logs..."
docker compose -f "$COMPOSE_FILE" logs --no-color > "$RUN_RESULTS_DIR/docker_logs.txt" 2>&1
ok "Container logs saved to $RUN_RESULTS_DIR/docker_logs.txt"

# Save individual container logs
for container in cvc fzc rzc sc bcm icu tcu plant-sim can-gateway fault-inject mqtt-broker; do
    docker compose -f "$COMPOSE_FILE" logs --no-color "$container" \
        > "$RUN_RESULTS_DIR/${container}.log" 2>/dev/null || true
done
ok "Individual container logs saved"

# ---------------------------------------------------------------------------
# Teardown (unless --keep)
# ---------------------------------------------------------------------------
if [ "$KEEP_CONTAINERS" = true ]; then
    warn "Containers kept running (--keep flag). Tear down manually:"
    warn "  docker compose -f $COMPOSE_FILE down"
else
    header "Tearing Down SIL Platform"
    docker compose -f "$COMPOSE_FILE" down 2>&1 | tee -a "$RUN_RESULTS_DIR/docker_teardown.log"
    ok "All containers stopped and removed"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
header "SIL Test Summary"

info "Results directory: $RUN_RESULTS_DIR"

# List result files
if ls "$RUN_RESULTS_DIR"/*.xml 1>/dev/null 2>&1; then
    info "JUnit XML report(s):"
    for xml in "$RUN_RESULTS_DIR"/*.xml; do
        info "  $(basename "$xml")"
    done
fi

if [ -f "$RUN_RESULTS_DIR/summary.txt" ]; then
    echo ""
    cat "$RUN_RESULTS_DIR/summary.txt"
    echo ""
fi

# Final exit status
if [ $VERDICT_EXIT -eq 0 ]; then
    echo ""
    ok "============================================"
    ok "  ALL SIL TESTS PASSED"
    ok "============================================"
    exit 0
else
    echo ""
    fail "============================================"
    fail "  ONE OR MORE SIL TESTS FAILED"
    fail "============================================"
    fail "Review details in: $RUN_RESULTS_DIR/verdict_output.log"
    exit 1
fi
