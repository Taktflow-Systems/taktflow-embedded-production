#!/usr/bin/env bash
# =============================================================================
# deploy-pi.sh — Deploy taktflow-embedded to Pi for HIL testing
#
# Syncs source to Pi, builds Docker images (vECUs + plant-sim + MQTT),
# sets up can0, and starts the vECU station.
#
# Usage:
#   ./scripts/deploy/deploy-pi.sh                 # deploy + start
#   ./scripts/deploy/deploy-pi.sh --build-only     # sync + build, don't start
#   ./scripts/deploy/deploy-pi.sh --no-build       # sync only
#
# Prerequisites:
#   - SSH access: ssh bench-pi@192.0.2.10
#   - Docker + Docker Compose on Pi
#   - USB-CAN adapter connected to Pi (shows up as can0)
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

PI_HOST="${PI_HOST:-bench-pi@192.0.2.10}"
PI_DIR="/home/taktflow-pi/taktflow-embedded-production"
COMPOSE_FILE="docker/docker-compose.hil-pi.yml"
BITRATE=500000

BUILD_ONLY=false
NO_BUILD=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-only) BUILD_ONLY=true ;;
        --no-build)   NO_BUILD=true ;;
        --host)       PI_HOST="$2"; shift ;;
        *)            echo "Unknown: $1"; exit 1 ;;
    esac
    shift
done

echo "============================================"
echo "  Taktflow HIL — Deploy to Pi"
echo "  $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
echo "  Pi: ${PI_HOST}"
echo "============================================"
echo ""

# --- Step 1: Sync repo to Pi ---
echo "[1/5] Syncing repository to Pi..."
rsync -avz --delete \
    --exclude '.git' \
    --exclude 'node_modules' \
    --exclude '__pycache__' \
    --exclude '*.pyc' \
    --exclude 'build/' \
    --exclude 'test/hil/reports/' \
    --exclude 'test/hil/__pycache__/' \
    --exclude '.claude/' \
    --exclude '.env' \
    --exclude '.env.*' \
    --exclude 'docker/certs/' \
    --exclude '*.pem' \
    --exclude '*.key' \
    --exclude 'gateway/mosquitto/passwd' \
    --exclude 'private/' \
    "$REPO_ROOT/" "${PI_HOST}:${PI_DIR}/"
echo "  [OK] Sync complete"
echo ""

if [ "$NO_BUILD" = true ]; then
    echo "  --no-build: skipping Docker build and startup"
    exit 0
fi

# --- Step 2: Install Python deps for test runner ---
echo "[2/5] Installing Python dependencies on Pi..."
ssh "$PI_HOST" "pip3 install --user python-can cantools paho-mqtt pyyaml 2>/dev/null || true"
echo "  [OK] Python deps"
echo ""

# --- Step 3: Set up can0 ---
echo "[3/5] Setting up can0 on Pi..."
ssh "$PI_HOST" "sudo ip link set can0 down 2>/dev/null || true; \
    sudo ip link set can0 type can bitrate ${BITRATE}; \
    sudo ip link set can0 up; \
    echo '  [OK] can0 up @ ${BITRATE} bit/s'"
echo ""

# --- Step 4: Build Docker images ---
echo "[4/5] Building Docker images on Pi (this may take a few minutes)..."
ssh "$PI_HOST" "cd ${PI_DIR} && docker compose -f ${COMPOSE_FILE} build"
echo "  [OK] Build complete"
echo ""

if [ "$BUILD_ONLY" = true ]; then
    echo "  --build-only: skipping startup"
    exit 0
fi

# --- Step 5: Start vECU station ---
echo "[5/5] Starting vECU station on Pi..."
ssh "$PI_HOST" "cd ${PI_DIR} && \
    docker compose -f ${COMPOSE_FILE} down 2>/dev/null || true; \
    docker compose -f ${COMPOSE_FILE} up -d"
echo ""

# Verify
sleep 5
echo "=== Container Status ==="
ssh "$PI_HOST" "cd ${PI_DIR} && docker compose -f ${COMPOSE_FILE} ps --format 'table {{.Name}}\t{{.Status}}'"
echo ""

echo "=== CAN Bus Check (3s) ==="
ssh "$PI_HOST" "timeout 3 candump can0 -t z 2>/dev/null | head -15 || echo '  (no traffic yet — physical ECUs may not be powered)'"
echo ""

echo "============================================"
echo "  Pi vECU station running"
echo "  SSH:   ssh ${PI_HOST}"
echo "  Logs:  docker compose -f ${COMPOSE_FILE} logs -f"
echo "  Stop:  docker compose -f ${COMPOSE_FILE} down"
echo "  Tests: python3 test/hil/test_hil_heartbeat.py"
echo "============================================"
