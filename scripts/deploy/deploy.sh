#!/usr/bin/env bash
# =============================================================================
# deploy.sh — Deploy the Taktflow SIL demo stack to a SIL host from a clean tree.
#
# Usage:
#   TAKTFLOW_INFRA_DIR=<path to taktflow-infra> scripts/deploy/deploy.sh \
#       [--fresh] [--profiles a,b] <ssh-target>
#
# Options:
#   --fresh           build with --no-cache (default: cached build)
#   --profiles LIST   comma-separated compose profiles to enable (default: none;
#                     "foxbms" also syncs foxbms-posix, see FOXBMS_LOCAL)
#
# Environment:
#   TAKTFLOW_INFRA_DIR  required; checkout of taktflow-infra (provides smoke.sh)
#   FOXBMS_LOCAL        foxbms-posix checkout (default: ../foxbms-posix); used
#                       only with the foxbms profile
#
# Safety: aborts on a dirty tree, on a missing host marker file, and exits
# non-zero when the final smoke test does not turn green within 5 minutes.
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
REMOTE_DIR="/opt/taktflow-embedded"
FOXBMS_DIR="/opt/foxbms-posix"
MARKER_FILE="$REMOTE_DIR/.deploy-origin"
MARKER_VALUE="taktflow-embedded-production"
SMOKE_TIMEOUT_SEC=300
SMOKE_RETRY_SEC=15

FRESH=0
PROFILES=""
VPS_HOST=""

usage() {
    echo "Usage: TAKTFLOW_INFRA_DIR=<dir> $0 [--fresh] [--profiles a,b] <ssh-target>"
    echo "  e.g. TAKTFLOW_INFRA_DIR=../taktflow-infra $0 <ssh-target>"
}

die() { echo "ERROR: $*" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --fresh) FRESH=1; shift ;;
        --profiles)
            [ $# -ge 2 ] || { usage; exit 1; }
            PROFILES="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        -*) usage; die "unknown option: $1" ;;
        *)
            [ -z "$VPS_HOST" ] || { usage; die "more than one target given"; }
            VPS_HOST="$1"; shift ;;
    esac
done
[ -n "$VPS_HOST" ] || { usage; exit 1; }

# Preflight (nothing on the host is touched before all of these pass)
[ -n "${TAKTFLOW_INFRA_DIR:-}" ] || die "TAKTFLOW_INFRA_DIR is required (smoke test lives in taktflow-infra)"
SMOKE="$TAKTFLOW_INFRA_DIR/scripts/smoke.sh"
[ -f "$SMOKE" ] || die "smoke script not found: $SMOKE"

if [ -n "$(git -C "$REPO_ROOT" status --porcelain)" ]; then
    die "working tree $REPO_ROOT is not clean; commit or stash first"
fi
GIT_SHA="$(git -C "$REPO_ROOT" rev-parse HEAD)"

PROFILE_ARGS=()
USE_FOXBMS=0
if [ -n "$PROFILES" ]; then
    IFS=',' read -ra PROFILE_LIST <<< "$PROFILES"
    for p in "${PROFILE_LIST[@]}"; do
        PROFILE_ARGS+=(--profile "$p")
        [ "$p" = "foxbms" ] && USE_FOXBMS=1
    done
fi

FOXBMS_LOCAL="${FOXBMS_LOCAL:-}"
if [ "$USE_FOXBMS" -eq 1 ]; then
    if [ -z "$FOXBMS_LOCAL" ] && [ -d "$REPO_ROOT/../foxbms-posix" ]; then
        FOXBMS_LOCAL="$(cd "$REPO_ROOT/../foxbms-posix" && pwd)"
    fi
    [ -d "$FOXBMS_LOCAL" ] || die "profile foxbms needs FOXBMS_LOCAL (foxbms-posix checkout)"
    if [ -n "$(git -C "$FOXBMS_LOCAL" status --porcelain)" ]; then
        die "foxbms-posix tree $FOXBMS_LOCAL is not clean"
    fi
fi

if [[ "$VPS_HOST" == root@* ]]; then
    echo "NOTE: deploying as root; a dedicated deploy user is planned (RF-P1-15)."
fi

echo "=== Taktflow SIL Demo Deployment ==="
echo "Target:   $VPS_HOST"
echo "Remote:   $REMOTE_DIR"
echo "Revision: $GIT_SHA"
echo "Profiles: ${PROFILES:-none}"
echo "Build:    $([ "$FRESH" -eq 1 ] && echo 'no-cache (--fresh)' || echo cached)"
echo ""

# Step 1: remote marker, proves the target dir belongs to this repository
echo "[1/8] Checking remote marker..."
REMOTE_MARKER="$(ssh -o BatchMode=yes "$VPS_HOST" "cat '$MARKER_FILE' 2>/dev/null || true" | tr -d '\r')"
[ "$REMOTE_MARKER" = "$MARKER_VALUE" ] || die "marker $MARKER_FILE on $VPS_HOST is '$REMOTE_MARKER', expected '$MARKER_VALUE'"

# Step 2: vcan module (stays until the vcan@ units of RF-P1-04 exist)
echo "[2/8] Checking vcan module on VPS..."
ssh "$VPS_HOST" "sudo modprobe vcan && echo 'vcan module loaded' || { echo 'ERROR: vcan module not available'; exit 1; }"

# Step 3: sync the repository (host-only state is excluded from --delete)
echo "[3/8] Syncing taktflow-embedded-production to VPS..."
rsync -avz --delete \
    --exclude '.git' \
    --exclude 'node_modules' \
    --exclude '__pycache__' \
    --exclude '*.pyc' \
    --exclude 'build/' \
    --exclude '.env' \
    --exclude '.env.*' \
    --exclude 'docker/certs/' \
    --exclude '*.pem' \
    --exclude '*.key' \
    --exclude 'gateway/mosquitto/passwd' \
    --exclude '.claude/' \
    --exclude 'private/' \
    --exclude '/.deploy-origin' \
    --exclude '/docker/DEPLOYED_SHA' \
    --exclude '/test/sil/results/' \
    "$REPO_ROOT/" "$VPS_HOST:$REMOTE_DIR/"

# Step 4: foxBMS sources, only when the foxbms profile is requested
echo "[4/8] foxbms-posix..."
if [ "$USE_FOXBMS" -eq 1 ]; then
    if [ ! -f "$FOXBMS_LOCAL/foxbms-2/src/app/main/main.c" ]; then
        echo "  Initializing foxbms-2 submodule..."
        (cd "$FOXBMS_LOCAL" && git submodule update --init)
    fi
    rsync -avz --delete \
        --exclude '.git' \
        --exclude '__pycache__' \
        --exclude '*.pyc' \
        --exclude 'build/' \
        --exclude '.claude/' \
        --exclude 'foxbms-2/.git' \
        "$FOXBMS_LOCAL/" "$VPS_HOST:$FOXBMS_DIR/"
else
    echo "  skipped (profile foxbms not requested)"
fi

COMPOSE="docker compose ${PROFILE_ARGS[*]:-} -f docker-compose.dev.yml -f docker-compose.prod.yml"
REMOTE_ENV="cd $REMOTE_DIR/docker && FOXBMS_DIR=$FOXBMS_DIR GIT_SHA=$GIT_SHA"

# Step 5: build first (running containers stay up), revision label via build arg
echo "[5/8] Building images on VPS..."
BUILD_FLAGS="--build-arg GIT_SHA=$GIT_SHA"
[ "$FRESH" -eq 1 ] && BUILD_FLAGS="$BUILD_FLAGS --no-cache"
ssh "$VPS_HOST" "$REMOTE_ENV $COMPOSE build $BUILD_FLAGS"

# Step 6: start; orphans of removed services go, the standalone-ingress caddy stays off
echo "[6/8] Starting services..."
ssh "$VPS_HOST" "$REMOTE_ENV $COMPOSE up -d --remove-orphans"

# Step 7: smoke test from taktflow-infra, retried while containers settle
echo "[7/8] Smoke test (up to ${SMOKE_TIMEOUT_SEC}s)..."
deadline=$(( $(date +%s) + SMOKE_TIMEOUT_SEC ))
while :; do
    if bash "$SMOKE"; then
        break
    fi
    if [ "$(date +%s)" -ge "$deadline" ]; then
        die "smoke test still red after ${SMOKE_TIMEOUT_SEC}s; roll back (see runbook)"
    fi
    echo "smoke red, retrying in ${SMOKE_RETRY_SEC}s..."
    sleep "$SMOKE_RETRY_SEC"
done

# Step 8: only after a green smoke: prune dangling images (until now they are the
# rollback images) and record the deployed revision
echo "[8/8] Pruning dangling images, recording revision..."
ssh "$VPS_HOST" "docker image prune -f && printf '%s\n' '$GIT_SHA' > $REMOTE_DIR/docker/DEPLOYED_SHA"
ssh "$VPS_HOST" "cd $REMOTE_DIR/docker && $COMPOSE ps --format 'table {{.Name}}\t{{.Status}}'"

echo ""
echo "=== Deployment complete: $GIT_SHA ==="
