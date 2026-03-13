# VPS Deployment Guide — Taktflow SIL Demo

## Overview

Deploy the full 7-ECU zonal vehicle simulation on a cloud VPS so the live demo
is accessible 24/7 at `taktflow-systems.com/embedded`.

## VPS Requirements

| Resource | Minimum | Recommended |
|----------|---------|-------------|
| Provider | Hetzner CX21 (~5/mo) | Hetzner CX31 |
| vCPU | 2 | 3 |
| RAM | 4 GB | 8 GB |
| Disk | 40 GB SSD | 80 GB SSD |
| OS | Ubuntu 22.04 LTS | Ubuntu 22.04 LTS |
| Bandwidth | 20 TB | 20 TB |

## Kernel Requirement: vcan Module

The SocketCAN virtual interface (`vcan0`) requires kernel modules:
- `CONFIG_CAN=m`
- `CONFIG_VCAN=m`
- `CONFIG_CAN_RAW=m`

### Check if Available

```bash
sudo modprobe vcan
# If this succeeds, you're good. If not, build a custom kernel.
```

### Build Custom Kernel (if needed)

```bash
# One-time setup (~30min)
sudo apt install -y build-essential libncurses-dev flex bison libssl-dev libelf-dev
cd /usr/src
sudo apt source linux
cd linux-*
cp /boot/config-$(uname -r) .config
# Enable CAN modules:
scripts/config --module CONFIG_CAN
scripts/config --module CONFIG_VCAN
scripts/config --module CONFIG_CAN_RAW
make -j$(nproc) deb-pkg
sudo dpkg -i ../linux-image-*.deb ../linux-headers-*.deb
sudo reboot
```

## DNS Setup

Create an A record:
```
sil.taktflow-systems.com  ->  <VPS_IP>
```

Caddy will auto-provision TLS via Let's Encrypt once DNS resolves.

## Deployment Steps

### 1. Provision VPS

```bash
# SSH to VPS
ssh root@<VPS_IP>

# Update system
apt update && apt upgrade -y

# Install Docker
curl -fsSL https://get.docker.com | sh
systemctl enable docker

# Install Docker Compose plugin
apt install -y docker-compose-plugin

# Verify
docker compose version
```

### 2. Clone Repository

```bash
cd /opt
git clone https://github.com/your-org/taktflow-embedded.git
cd taktflow-embedded
```

### 3. Load vcan Module

```bash
sudo modprobe vcan
# Make persistent:
echo "vcan" | sudo tee /etc/modules-load.d/vcan.conf
```

### 4. Build and Start

```bash
cd docker
docker compose up --build -d
```

### 5. Verify

```bash
# Check all services running
docker compose ps

# Check CAN bus traffic
docker exec -it can-setup ip link show vcan0

# Check MQTT
docker exec -it mqtt-broker mosquitto_sub -t 'taktflow/#' -C 5

# Check WebSocket
# From your laptop:
# wscat -c wss://sil.taktflow-systems.com/ws/telemetry

# Check SAP QM Swagger
# Browser: https://sil.taktflow-systems.com/api/sap/docs
```

## Service Architecture (14 containers)

```
can-setup        -> vcan0 creation (exits after setup)
cvc              -> Central Vehicle Computer (C, POSIX)
fzc              -> Front Zone Controller (C, POSIX)
rzc              -> Rear Zone Controller (C, POSIX)
sc               -> Safety Controller (C, POSIX)
bcm              -> Body Control Module (C, POSIX)
icu              -> Instrument Cluster Unit (C, POSIX)
tcu              -> Telematics Control Unit (C, POSIX)
plant-sim        -> Physics simulator (Python)
mqtt-broker      -> Mosquitto (internal only)
can-gateway      -> CAN -> MQTT decoder (Python)
ws-bridge        -> MQTT -> WebSocket at 10Hz (FastAPI)
caddy            -> Reverse proxy + auto-TLS
sap-qm-mock     -> SAP QM OData API (FastAPI + SQLite)
fault-inject     -> Fault injection API (FastAPI)
ml-inference     -> Anomaly detection (scikit-learn)
```

## RAM Estimate

| Service | Est. RAM |
|---------|----------|
| 7 ECU containers | ~70 MB (10 MB each) |
| Plant simulator | ~30 MB |
| Mosquitto | ~10 MB |
| CAN gateway | ~30 MB |
| WS bridge | ~30 MB |
| Caddy | ~30 MB |
| SAP QM mock | ~40 MB |
| ML inference | ~60 MB |
| Fault inject | ~30 MB |
| **Total** | **~330 MB** |

## Exposed Ports

Only Caddy is exposed to the internet:
- **443** (HTTPS/WSS) — Caddy terminates TLS
- **80** (HTTP) — redirects to HTTPS

All other services communicate on localhost via `network_mode: host`.

## Monitoring

```bash
# Service health
docker compose ps
docker compose logs -f --tail=50

# Resource usage
docker stats --no-stream

# CAN bus activity
docker exec -it can-gateway python -c "import can; bus=can.Bus('vcan0','socketcan'); [print(bus.recv(timeout=1)) for _ in range(5)]"
```

## Troubleshooting

| Issue | Fix |
|-------|-----|
| `vcan0: No such device` | `sudo modprobe vcan` then restart can-setup |
| Caddy TLS error | Check DNS A record resolves to VPS IP |
| No MQTT traffic | Check can-gateway logs: `docker compose logs can-gateway` |
| WS connection refused | Check ws-bridge is running on port 8080 |
| High CPU | Check plant-sim timing: should be ~1% CPU at 100Hz |

## Updates

```bash
cd /opt/taktflow-embedded
git pull
cd docker
docker compose up --build -d
```
