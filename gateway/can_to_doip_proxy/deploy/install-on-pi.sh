#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
#
# Deploy the can-to-doip-proxy to the Raspberry Pi bench. Assumes SSH is
# configured per the HIL-bench README (bench-pi@192.0.2.10, no
# jump host, key-based auth). Builds the binary on the current host
# (relying on cargo-zigbuild or a native linux/aarch64 toolchain), rsyncs
# it across, and enables the systemd unit.
#
# Usage:
#   cd gateway/can_to_doip_proxy
#   ./deploy/install-on-pi.sh [pi_user@pi_host]
#
# Defaults: bench-pi@192.0.2.10

set -euo pipefail

TARGET="${1:-bench-pi@192.0.2.10}"
PROXY_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROXY_ROOT"

echo "[install-on-pi] building release binary for aarch64-unknown-linux-gnu"
if ! cargo zigbuild --release --target aarch64-unknown-linux-gnu \
       --bin opensovd-can-to-doip-proxy 2>/dev/null; then
    echo "[install-on-pi] cargo zigbuild not available; falling back to host target"
    echo "[install-on-pi] WARNING: binary will only run on the build host's arch"
    cargo build --release --bin opensovd-can-to-doip-proxy
    BIN=target/release/opensovd-can-to-doip-proxy
else
    BIN=target/aarch64-unknown-linux-gnu/release/opensovd-can-to-doip-proxy
fi

echo "[install-on-pi] copying binary -> $TARGET:/usr/local/bin/"
rsync -avz "$BIN" "$TARGET:/tmp/opensovd-can-to-doip-proxy"
ssh "$TARGET" "sudo install -m 0755 /tmp/opensovd-can-to-doip-proxy /usr/local/bin/opensovd-can-to-doip-proxy && rm /tmp/opensovd-can-to-doip-proxy"

echo "[install-on-pi] copying config -> $TARGET:/etc/opensovd/proxy.toml"
rsync -avz deploy/opensovd-proxy.toml "$TARGET:/tmp/opensovd-proxy.toml"
ssh "$TARGET" "sudo mkdir -p /etc/opensovd && sudo install -m 0644 /tmp/opensovd-proxy.toml /etc/opensovd/proxy.toml && rm /tmp/opensovd-proxy.toml"

echo "[install-on-pi] copying unit -> $TARGET:/etc/systemd/system/"
rsync -avz deploy/can-to-doip-proxy.service "$TARGET:/tmp/can-to-doip-proxy.service"
ssh "$TARGET" "sudo install -m 0644 /tmp/can-to-doip-proxy.service /etc/systemd/system/can-to-doip-proxy.service && rm /tmp/can-to-doip-proxy.service"

echo "[install-on-pi] reloading systemd and enabling service"
ssh "$TARGET" "sudo systemctl daemon-reload && sudo systemctl enable can-to-doip-proxy.service"

echo "[install-on-pi] Coordinating ecu-sim port conflict (stop it before starting proxy)"
ssh "$TARGET" "sudo systemctl stop ecu-sim.service 2>/dev/null || true"

echo "[install-on-pi] starting proxy"
ssh "$TARGET" "sudo systemctl restart can-to-doip-proxy.service"
sleep 1
ssh "$TARGET" "sudo systemctl status can-to-doip-proxy.service --no-pager || true"

echo "[install-on-pi] done. Tail logs with:"
echo "  ssh $TARGET 'sudo journalctl -u can-to-doip-proxy.service -f'"
