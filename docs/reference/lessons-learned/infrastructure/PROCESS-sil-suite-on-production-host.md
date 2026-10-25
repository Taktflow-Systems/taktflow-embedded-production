# The SIL Test Script Must Use the Production Compose Files on a Deployed Host

## 2026-10-25

**Context**: The public SIL host runs the stack with `docker-compose.dev.yml` plus `docker-compose.prod.yml` (log caps, later the MQTT secret), built by `scripts/deploy/deploy.sh` with the build argument `GIT_SHA`. `test/sil/run_sil.sh` is also run on that host to prove a deploy. It called `docker compose -f docker-compose.dev.yml up --build -d` and nothing else.

**Mistake**: The test script knew only the dev file. A test run on the host would therefore recreate every container with dev settings (no log caps, default credentials) and rebuild the images without the revision label (`unknown` instead of the deployed commit). Until the next deploy the live demo would run an unreviewed configuration, and the test result would not describe the deployed stack. Found in the production refactor review before it happened.

**Fix**: `run_sil.sh` detects a deployed host by the marker `.deploy-origin` in the repository root (written only there, never synced) and by the existence of `docker/docker-compose.prod.yml`. On such a host every compose call uses both files, and the start is `build --build-arg GIT_SHA=<docker/DEPLOYED_SHA>` followed by `up -d`. A cached build reproduces the deployed images, so compose recreates nothing. Without the marker (laptop, CI) the script behaves as before. The script prints the compose files it uses. Use `--keep` on a deployed host: without it the script ends with `docker compose down`, which stops the live stack.

**Principle**: A test runner that touches a deployed environment must start it exactly as the deploy does (same compose files, same build arguments), and must say which files it used. Detect the environment by an explicit marker, not by guessing from paths.
