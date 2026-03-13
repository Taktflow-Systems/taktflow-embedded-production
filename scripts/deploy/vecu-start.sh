#!/usr/bin/env bash
# =============================================================================
# vecu-start.sh — Create vcan0 and start simulated ECUs via Docker Compose
#
# Usage:  ./scripts/vecu-start.sh
#
# Prerequisites:
#   - Linux host with vcan kernel module available
#   - Docker and Docker Compose installed
#   - Run with sudo or as user with NET_ADMIN capability
# =============================================================================

set -euo pipefail

echo "=== Taktflow Embedded — Starting Simulated ECUs ==="

# Load vcan kernel module
echo "Loading vcan kernel module..."
sudo modprobe vcan

# Create vcan0 interface (ignore error if already exists)
echo "Setting up vcan0..."
sudo ip link add vcan0 type vcan 2>/dev/null || true
sudo ip link set vcan0 up

echo "vcan0 is up."

# Start Docker Compose
echo "Starting simulated ECUs (BCM, ICU, TCU)..."
cd "$(dirname "$0")/../../docker"
docker compose up --build -d

echo ""
echo "=== Simulated ECUs started ==="
echo "  BCM — Body Control Module"
echo "  ICU — Instrument Cluster Unit"
echo "  TCU — Telematics Control Unit"
echo ""
echo "Monitor CAN traffic:  candump vcan0"
echo "View logs:            docker compose logs -f"
echo "Stop:                 ./scripts/vecu-stop.sh"
