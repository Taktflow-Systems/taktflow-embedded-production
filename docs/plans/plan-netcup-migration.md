# Plan: Migrate SIL Demo to Netcup VPS

## Context

- **VPS**: Netcup VPS 1000 G11 (4 vCPU, 8GB RAM, 256GB SSD)
- **OS**: Debian 12 (pre-installed)
- **Scope**: SIL demo only (`sil.taktflow-systems.com`)
- **Domain**: `taktflow-systems.com`
- **IP**: `152.53.245.209`
- **Completed**: 2026-02-24

## Status Table

| Phase | Name | Status |
|-------|------|--------|
| 1 | VPS Initial Setup | DONE |
| 2 | DNS Configuration | DONE |
| 3 | Deploy SIL Demo | DONE |
| 4 | Verify | DONE |

---

## Phase 1: VPS Initial Setup

SSH into your Netcup VPS and run these commands.

### 1.1 System update

```bash
ssh root@<NETCUP_IP>
apt update && apt upgrade -y
```

### 1.2 Check vcan kernel module

```bash
modprobe vcan
```

If this succeeds, make it persistent:

```bash
echo "vcan" > /etc/modules-load.d/vcan.conf
```

If it fails (`FATAL: Module vcan not found`), you need the `can-utils` package and possibly extra kernel modules:

```bash
apt install -y can-utils linux-modules-extra-$(uname -r) 2>/dev/null || true
modprobe vcan
```

If it still fails, install the full kernel headers and rebuild (unlikely on Debian 12 — vcan is included by default):

```bash
apt install -y linux-headers-$(uname -r)
modprobe vcan
```

### 1.3 Install Docker

```bash
curl -fsSL https://get.docker.com | sh
systemctl enable docker
systemctl start docker

# Verify
docker compose version
```

### 1.4 Firewall

```bash
apt install -y ufw
ufw default deny incoming
ufw default allow outgoing
ufw allow ssh
ufw allow 80/tcp    # HTTP (Caddy redirect)
ufw allow 443/tcp   # HTTPS (Caddy TLS)
ufw --force enable
ufw status
```

### 1.5 Create deploy user (optional but recommended)

```bash
adduser deploy
usermod -aG docker deploy
# Copy SSH key
mkdir -p /home/deploy/.ssh
cp ~/.ssh/authorized_keys /home/deploy/.ssh/
chown -R deploy:deploy /home/deploy/.ssh
```

---

## Phase 2: DNS Configuration

In your domain registrar (wherever `taktflow-systems.com` is managed):

### 2.1 Add A record

```
sil.taktflow-systems.com  ->  <NETCUP_IP>   (A record, TTL 300)
```

### 2.2 Verify DNS propagation

```bash
# From your local machine:
nslookup sil.taktflow-systems.com
# Should return <NETCUP_IP>
```

Caddy will auto-provision TLS via Let's Encrypt once DNS resolves. No manual cert setup needed.

---

## Phase 3: Deploy SIL Demo

### 3.1 Clone the repo on VPS

```bash
ssh root@<NETCUP_IP>    # or deploy@<NETCUP_IP>
mkdir -p /opt
cd /opt
git clone https://github.com/nhuvaoanh123/taktflow-embedded.git
cd taktflow-embedded
```

### 3.2 Build and start

```bash
cd docker
docker compose up --build -d
```

First build takes ~5-10 min (compiling 7 ECU C binaries + pulling Python images).

### 3.3 Check services

```bash
docker compose ps
```

All 15 services should show `Up` (except `can-setup` which exits after creating vcan0).

---

## Phase 4: Verify

### 4.1 CAN bus

```bash
docker exec -it can-setup ip link show vcan0
# Should show: vcan0: <NOARP,RUNNING,NOQUEUE,UP,LOWER_UP>
```

### 4.2 MQTT traffic

```bash
docker exec -it mqtt-broker mosquitto_sub -t 'taktflow/#' -C 5
# Should print 5 CAN-decoded messages
```

### 4.3 WebSocket (from your laptop)

```bash
# Install wscat if needed: npm i -g wscat
wscat -c wss://sil.taktflow-systems.com/ws/telemetry
# Should stream JSON telemetry at 10Hz
```

### 4.4 SAP QM Swagger

Open in browser: `https://sil.taktflow-systems.com/api/sap/docs`

### 4.5 Dashboard

Open in browser: `https://taktflow-systems.com/embedded`
- Should connect to live WebSocket on `sil.taktflow-systems.com`

---

## Future Deploys

Use the existing deploy script:

```bash
./scripts/deploy.sh root@<NETCUP_IP>
```

Or manually:

```bash
ssh root@<NETCUP_IP>
cd /opt/taktflow-embedded
git pull
cd docker
docker compose up --build -d
```

---

## Troubleshooting

| Issue | Fix |
|-------|-----|
| `vcan0: No such device` | `modprobe vcan` then `docker compose restart can-setup` |
| Caddy TLS error | DNS not propagated yet — wait or check `nslookup` |
| Port 443 refused | Check `ufw status` allows 443 |
| High memory | `docker stats --no-stream` — total should be ~330MB |
| Container crash loop | `docker compose logs <service> --tail=50` |
