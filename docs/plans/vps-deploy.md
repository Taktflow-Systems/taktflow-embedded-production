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

## MQTT credential on a deployed host (RF-P4-03)

`scripts/deploy/deploy.sh` runs compose with `docker-compose.dev.yml` plus
`docker-compose.prod.yml`. The production file sets `MQTT_PASSWORD` for every
broker client (plant-sim, can-gateway, ws-bridge, sap-qm-mock, ml-inference,
fault-inject) as `${MQTT_PASSWORD:?...}`: compose stops before any build when
`docker/.env` on the host has no non-empty `MQTT_PASSWORD`. `MQTT_USER`
defaults to `taktflow`. Laptops keep the dev defaults and need no `.env`.
`test/sil/run_sil.sh` reads only `MQTT_USER` and `MQTT_PASSWORD` from
`docker/.env` on a deployed host (marker `.deploy-origin`), so the verdict
checker and the plant-inject helpers authenticate with the same credential.

Facts the rotation depends on (compose project `docker`, broker container
`docker-mqtt-broker-1`, host networking, broker is PID 1):

- Host files: `/opt/taktflow-embedded/docker/.env` (mode 600) and
  `/opt/taktflow-embedded/gateway/mosquitto/passwd`. Neither is overwritten by
  `deploy.sh` (both are rsync excludes).
- Container paths: `/mosquitto/config/passwd`, `/mosquitto/config/acl`,
  `/mosquitto/config/mosquitto.conf`, all bind-mounted read-only. The tool
  therefore cannot write the password file in place inside the container.
- The password file is a single-file bind mount, which follows the inode. Do
  not replace it with `mv` or `mosquitto_passwd -c` on the host; a new inode is
  invisible to the container. Overwrite the content in place with `cat >`.

### Rotate the broker password

Run as root on the host. The new value is never echoed.

```bash
cd /opt/taktflow-embedded
umask 077
B=".backup-mqtt-$(date -u +%Y%m%dT%H%M%SZ)"   # .backup-* survives deploy.sh rsync
mkdir "$B" && cp -p docker/.env "$B/env" && cp -p gateway/mosquitto/passwd "$B/passwd"

NEW="$(openssl rand -hex 32)"; export NEW

# 1. new hash, made by the running broker's own tool into its /tmp
docker exec -e NEW docker-mqtt-broker-1 sh -c \
  'mosquitto_passwd -b -c /tmp/passwd.new taktflow "$NEW" && cat /tmp/passwd.new && rm -f /tmp/passwd.new' \
  > "$B/passwd.new"
[ -s "$B/passwd.new" ] || { echo "no hash produced; stop"; false; }

# 2. host .env: drop the old line, append the new one, keep mode 600, owner deploy
{ grep -v '^MQTT_PASSWORD=' docker/.env || true; printf 'MQTT_PASSWORD=%s\n' "$NEW"; } > docker/.env.new
chmod 600 docker/.env.new && mv docker/.env.new docker/.env && chown deploy:deploy docker/.env

# 3. password file, content replaced in place (same inode), then reload from inside
#    the container; never `docker kill -s HUP` (see the note below)
cat "$B/passwd.new" > gateway/mosquitto/passwd
docker exec -u mosquitto docker-mqtt-broker-1 kill -HUP 1

# 4. the new password is accepted, the dev default is refused (exit 0 and non-zero)
docker exec -e NEW docker-mqtt-broker-1 sh -c 'mosquitto_pub -h 127.0.0.1 -u taktflow -P "$NEW" -t rotation-check -m ok'; echo "new=$?"
docker exec docker-mqtt-broker-1 sh -c 'mosquitto_pub -h 127.0.0.1 -u taktflow -P taktflow-dev -t rotation-check -m ok'; echo "dev=$?"
unset NEW
```

If `dev=0` after the reload, run `docker restart docker-mqtt-broker-1`
(re-resolves the bind mount) and repeat step 4.

Then deploy from the workstation so the clients restart with the new
credential (compose recreates the six services because their environment
changed):

```bash
TAKTFLOW_INFRA_DIR=<taktflow-infra checkout> scripts/deploy/deploy.sh <ssh-target>
```

Between step 3 and the container recreation the running clients still hold the
old password; established connections stay up, reconnects are refused, so keep
that window short. After the deploy: smoke green, and
`docker logs --since 10m docker-mqtt-broker-1 2>&1 | grep -c 'not authorised'`
settles at 0 once the clients have restarted.

### Roll back

```bash
cd /opt/taktflow-embedded
B=.backup-mqtt-<stamp>                 # the directory made above
cat "$B/passwd" > gateway/mosquitto/passwd
cp -p "$B/env" docker/.env && chown deploy:deploy docker/.env
docker exec -u mosquitto docker-mqtt-broker-1 kill -HUP 1
```

Why not `docker kill -s HUP`: Docker records any `docker kill`, whatever the
signal, as a manual stop, and `restart: unless-stopped` then does not start the
container at the next boot. On 2026-10-25 the broker stayed down after a host
restart until the watchdog restarted it about three minutes later. The signal
sent with `docker exec -u mosquitto` reaches PID 1 (user `mosquitto`, so no
capability is needed) without changing the container's restart state. A
`docker restart` clears the flag again.

Then redeploy the previous commit (`deploy.sh` from a checkout of that commit).
The restored `.env` carries the old values, so the old compose files (default
`taktflow-dev`) and the restored password file agree again. Keep the backup
directory until the rotation has run for a week; it holds both credentials, so
it stays on the host and out of every repository.
