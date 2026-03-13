#!/usr/bin/env bash
# =============================================================================
# vecu-stop.sh — Stop simulated ECUs and clean up vcan0
#
# Usage:  ./scripts/vecu-stop.sh
# =============================================================================

set -euo pipefail

echo "=== Taktflow Embedded — Stopping Simulated ECUs ==="

# Stop Docker Compose
echo "Stopping simulated ECUs..."
cd "$(dirname "$0")/../../docker"
docker compose down

# Remove vcan0 interface (ignore error if already removed)
echo "Removing vcan0..."
sudo ip link delete vcan0 2>/dev/null || true

echo ""
echo "=== Done. All simulated ECUs stopped, vcan0 removed. ==="
