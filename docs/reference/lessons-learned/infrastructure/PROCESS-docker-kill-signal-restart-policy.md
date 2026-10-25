# `docker kill -s <signal>` Disables `unless-stopped` at the Next Boot

## 2026-10-25

**Context**: The MQTT password of the public SIL host was rotated; the broker re-reads its password file on SIGHUP, so the runbook sent `docker kill -s HUP docker-mqtt-broker-1`. The reload worked. A planned host restart 30 minutes later brought every container back except the broker; `can-gateway` and `plant-sim` turned unhealthy without it, and the watchdog restarted all three about three minutes after the boot.

**Mistake**: Docker treats every `docker kill`, also with a harmless signal such as HUP, as a manual stop of the container (`HasBeenManuallyStopped` in its stored state). With `restart: unless-stopped` the daemon then skips the container when it starts after a reboot, although the container kept running and looked normal until then.

**Fix**: Send the signal from inside the container, as the user that runs PID 1: `docker exec -u mosquitto docker-mqtt-broker-1 kill -HUP 1`. This leaves the restart state alone (checked: the stored flag stays false). A `docker restart` clears the flag if it was set. The rotation runbook in `docs/plans/vps-deploy.md` now uses the in-container signal.

**Principle**: Do not use `docker kill` for reload signals on containers that rely on a restart policy; signal PID 1 from inside the container, or check `HasBeenManuallyStopped` in `/var/lib/docker/containers/<id>/config.v2.json` afterwards.
