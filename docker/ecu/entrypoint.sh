#!/bin/bash
set -e

# Set up virtual CAN interface if not already present
if ! ip link show vcan0 > /dev/null 2>&1; then
    ip link add dev vcan0 type vcan
    ip link set up vcan0
fi

echo "Starting ECU: ${ECU_NAME}"
exec /app/${ECU_NAME}
