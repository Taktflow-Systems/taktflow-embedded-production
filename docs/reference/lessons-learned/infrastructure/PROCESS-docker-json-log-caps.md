# Docker json-file Logs Need Explicit Caps

## 2026-10-25

**Context**: The public SIL host ran the compose stack for months with Docker's default `json-file` log driver and no `logging` settings. The disk reached 88 percent; container logs were the largest part. After the containers were recreated on 2026-10-25, `cvc` and `fzc` again wrote about 45 MB of logs per hour each.

**Mistake**: `json-file` keeps everything unless `max-size` and `max-file` are set. A log default in `/etc/docker/daemon.json` does not help the running stack either: dockerd reads it only at start, and a container keeps the log settings it was created with.

**Fix**: `docker/docker-compose.prod.yml` sets `logging` (json-file, `max-size` 10m, `max-file` 3) for every service, so each deploy creates capped containers. The host's `daemon.json` (repository `taktflow-infra`) adds the same defaults for containers started outside compose.

**Principle**: Cap logs where the container is created (compose `logging`), not only in the daemon defaults. Check with `docker inspect -f '{{json .HostConfig.LogConfig}}' <container>`.
