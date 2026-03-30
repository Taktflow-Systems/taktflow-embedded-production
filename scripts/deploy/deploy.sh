#!/usr/bin/env bash
# =============================================================================
# deploy.sh — Deploy Taktflow SIL Demo to VPS
#
# Usage:
#   ./scripts/deploy.sh <VPS_HOST>
#   ./scripts/deploy.sh user@203.0.113.10
#
# Prerequisites:
#   - SSH access to VPS
#   - Docker + Docker Compose installed on VPS
#   - vcan kernel module available on VPS
#   - DNS: sil.taktflow-systems.com -> VPS_IP
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
REMOTE_DIR="/opt/taktflow-embedded"
FOXBMS_DIR="/opt/foxbms-posix"
FOXBMS_LOCAL=""
if [ -d "$REPO_ROOT/../foxbms-posix" ]; then
    FOXBMS_LOCAL="$(cd "$REPO_ROOT/../foxbms-posix" && pwd)"
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 <VPS_HOST>"
    echo "  e.g. $0 root@203.0.113.10"
    exit 1
fi

VPS_HOST="$1"

# Warn if deploying as root
if [[ "$VPS_HOST" == root@* ]]; then
    echo "WARNING: Deploying as root is not recommended."
    echo "  Create a deploy user: adduser deploy && usermod -aG docker deploy"
    echo "  Then use: $0 deploy@<host>"
    echo ""
fi

echo "=== Taktflow SIL Demo Deployment ==="
echo "VPS: $VPS_HOST"
echo "Remote dir: $REMOTE_DIR"
echo "foxBMS local: $FOXBMS_LOCAL"
echo ""

# Step 1: Ensure vcan module is loaded
echo "[1/6] Checking vcan module on VPS..."
ssh "$VPS_HOST" "sudo modprobe vcan && echo 'vcan module loaded' || { echo 'ERROR: vcan module not available'; exit 1; }"

# Step 2: Sync taktflow-embedded-production to VPS
echo "[2/6] Syncing taktflow-embedded-production to VPS..."
rsync -avz --delete \
    --exclude '.git' \
    --exclude 'node_modules' \
    --exclude '__pycache__' \
    --exclude '*.pyc' \
    --exclude 'build/' \
    --exclude '.env' \
    --exclude '.env.*' \
    --exclude 'docker/certs/' \
    --exclude '*.pem' \
    --exclude '*.key' \
    --exclude 'gateway/mosquitto/passwd' \
    --exclude '.claude/' \
    --exclude 'private/' \
    "$REPO_ROOT/" "$VPS_HOST:$REMOTE_DIR/"

# Step 3: Sync foxbms-posix to VPS (for BMS demo dashboard)
echo "[3/6] Syncing foxbms-posix to VPS..."
if [ -n "$FOXBMS_LOCAL" ] && [ -d "$FOXBMS_LOCAL" ]; then
    # Ensure foxbms-2 submodule is initialized (source needed for Docker build)
    if [ ! -f "$FOXBMS_LOCAL/foxbms-2/src/app/main/main.c" ]; then
        echo "  Initializing foxbms-2 submodule..."
        (cd "$FOXBMS_LOCAL" && git submodule update --init)
    fi
    rsync -avz --delete \
        --exclude '.git' \
        --exclude '__pycache__' \
        --exclude '*.pyc' \
        --exclude 'build/' \
        --exclude '.claude/' \
        --exclude 'foxbms-2/.git' \
        "$FOXBMS_LOCAL/" "$VPS_HOST:$FOXBMS_DIR/"
else
    echo "WARNING: foxbms-posix not found at $FOXBMS_LOCAL — BMS demo will not be deployed"
fi

# Step 4: Build containers (--no-cache ensures firmware is recompiled from fresh source)
echo "[4/6] Building Docker containers on VPS..."
ssh "$VPS_HOST" "cd $REMOTE_DIR/docker && FOXBMS_DIR=$FOXBMS_DIR docker compose -f docker-compose.dev.yml build --no-cache"

# Step 5: Restart services
echo "[5/6] Restarting services..."
ssh "$VPS_HOST" "cd $REMOTE_DIR/docker && FOXBMS_DIR=$FOXBMS_DIR docker compose -f docker-compose.dev.yml down && FOXBMS_DIR=$FOXBMS_DIR docker compose -f docker-compose.dev.yml up -d"

# Step 6: Verify
echo "[6/6] Verifying deployment..."
sleep 5
ssh "$VPS_HOST" "cd $REMOTE_DIR/docker && docker compose -f docker-compose.dev.yml ps --format 'table {{.Name}}\t{{.Status}}'"

echo ""
echo "=== Deployment complete ==="
echo "Dashboard:  https://taktflow-systems.com/embedded"
echo "BMS Demo:   https://sil.taktflow-systems.com/bms/"
echo "SAP QM API: https://sil.taktflow-systems.com/api/sap/docs"
echo "WebSocket:  wss://sil.taktflow-systems.com/ws/telemetry"
