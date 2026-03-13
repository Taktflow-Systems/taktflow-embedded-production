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

if [ $# -lt 1 ]; then
    echo "Usage: $0 <VPS_HOST>"
    echo "  e.g. $0 root@203.0.113.10"
    exit 1
fi

VPS_HOST="$1"

echo "=== Taktflow SIL Demo Deployment ==="
echo "VPS: $VPS_HOST"
echo "Remote dir: $REMOTE_DIR"
echo ""

# Step 1: Ensure vcan module is loaded
echo "[1/5] Checking vcan module on VPS..."
ssh "$VPS_HOST" "sudo modprobe vcan && echo 'vcan module loaded' || { echo 'ERROR: vcan module not available'; exit 1; }"

# Step 2: Sync repository to VPS
echo "[2/5] Syncing repository to VPS..."
rsync -avz --delete \
    --exclude '.git' \
    --exclude 'node_modules' \
    --exclude '__pycache__' \
    --exclude '*.pyc' \
    --exclude 'build/' \
    "$REPO_ROOT/" "$VPS_HOST:$REMOTE_DIR/"

# Step 3: Build containers (--no-cache ensures firmware is recompiled from fresh source)
echo "[3/5] Building Docker containers on VPS..."
ssh "$VPS_HOST" "cd $REMOTE_DIR/docker && docker compose build --no-cache"

# Step 4: Restart services
echo "[4/5] Restarting services..."
ssh "$VPS_HOST" "cd $REMOTE_DIR/docker && docker compose down && docker compose up -d"

# Step 5: Verify
echo "[5/5] Verifying deployment..."
sleep 5
ssh "$VPS_HOST" "cd $REMOTE_DIR/docker && docker compose ps --format 'table {{.Name}}\t{{.Status}}'"

echo ""
echo "=== Deployment complete ==="
echo "Dashboard: https://taktflow-systems.com/embedded"
echo "SAP QM API: https://sil.taktflow-systems.com/api/sap/docs"
echo "WebSocket:  wss://sil.taktflow-systems.com/ws/telemetry"
