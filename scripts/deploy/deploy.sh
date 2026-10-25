#!/usr/bin/env bash
# =============================================================================
# deploy.sh — Deploy the Taktflow SIL demo stack to a SIL host from a clean tree.
#
# Usage:
#   TAKTFLOW_INFRA_DIR=<path to taktflow-infra> scripts/deploy/deploy.sh \
#       [--fresh] [--profiles a,b] [--public-name NAME] <ssh-target>
#
# Options:
#   --fresh           build with --no-cache (default: cached build)
#   --profiles LIST   comma-separated compose profiles to enable (default: none;
#                     "foxbms" also syncs foxbms-posix, see FOXBMS_LOCAL)
#   --public-name N   public host name whose /healthz is awaited after start
#                     (default: sil.taktflow-systems.com)
#
# Environment:
#   TAKTFLOW_INFRA_DIR  required; checkout of taktflow-infra (provides smoke.sh)
#   FOXBMS_LOCAL        foxbms-posix checkout (default: ../foxbms-posix); used
#                       only with the foxbms profile
#
# Safety: aborts on a dirty tree, on a commit that is on no remote branch, on a
# missing host marker file or a vcan interface that is not UP. Exits non-zero
# when no fresh healthy /healthz appears or the smoke test stays red (5 minutes).
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
HEALTHZ_TIMEOUT_SEC=300
HEALTHZ_RETRY_SEC=10

FRESH=0
PROFILES=""
VPS_HOST=""
PUBLIC_NAME="sil.taktflow-systems.com"

usage() {
    echo "Usage: TAKTFLOW_INFRA_DIR=<dir> $0 [--fresh] [--profiles a,b] [--public-name NAME] <ssh-target>"
    echo "  e.g. TAKTFLOW_INFRA_DIR=../taktflow-infra $0 <ssh-target>"
}

die() { echo "ERROR: $*" >&2; exit 1; }

# Abort unless HEAD of the given tree is contained in a remote branch (after a fetch).
require_pushed() {
    local dir="$1" remote_branches
    git -C "$dir" fetch --quiet origin || die "git fetch origin failed in $dir"
    remote_branches="$(git -C "$dir" branch -r --contains HEAD)"
    [ -n "$remote_branches" ] || die "HEAD of $dir is on no remote branch; push it first"
}

# Poll https://NAME/healthz until ok is true and generated_at (UTC) is newer than
# SINCE (epoch seconds). Args: name since timeout_sec retry_sec. On timeout: last body, rc 1.
wait_fresh_healthz() {
    local name="$1" since="$2" timeout="$3" retry="$4"
    local re_ok='"ok"[[:space:]]*:[[:space:]]*true'
    local re_gen='"generated_at"[[:space:]]*:[[:space:]]*"([^"]+)"'
    local deadline body gen gen_epoch
    deadline=$(( $(date +%s) + timeout ))
    while :; do
        body="$(curl -s --connect-timeout 5 --max-time 10 "https://$name/healthz" || true)"
        gen=""
        gen_epoch=""
        if [[ "$body" =~ $re_gen ]]; then
            gen="${BASH_REMATCH[1]}"
            gen_epoch="$(date -u -d "$gen" +%s 2>/dev/null || true)"
        fi
        if [[ "$body" =~ $re_ok ]] && [ -n "$gen_epoch" ] && [ "$gen_epoch" -gt "$since" ]; then
            echo "healthz fresh and ok (generated_at=$gen)"
            return 0
        fi
        if [ "$(date +%s)" -ge "$deadline" ]; then
            echo "healthz not fresh and ok after ${timeout}s; last body:" >&2
            echo "${body:-<empty>}" >&2
            return 1
        fi
        echo "healthz not fresh and ok yet (generated_at=${gen:-none}), retrying in ${retry}s..."
        sleep "$retry"
    done
}

# Sourced by a test: stop after the function definitions.
[ "${BASH_SOURCE[0]}" = "$0" ] || return 0

while [ $# -gt 0 ]; do
    case "$1" in
        --fresh) FRESH=1; shift ;;
        --profiles)
            [ $# -ge 2 ] || { usage; exit 1; }
            PROFILES="$2"; shift 2 ;;
        --public-name)
            [ $# -ge 2 ] || { usage; exit 1; }
            PUBLIC_NAME="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        -*) usage; die "unknown option: $1" ;;
        *)
            [ -z "$VPS_HOST" ] || { usage; die "more than one target given"; }
            VPS_HOST="$1"; shift ;;
    esac
done
[ -n "$VPS_HOST" ] || { usage; exit 1; }
[[ "$PUBLIC_NAME" =~ ^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$ ]] || die "invalid --public-name: $PUBLIC_NAME"

# Preflight (nothing on the host is touched before all of these pass)
[ -n "${TAKTFLOW_INFRA_DIR:-}" ] || die "TAKTFLOW_INFRA_DIR is required (smoke test lives in taktflow-infra)"
SMOKE="$TAKTFLOW_INFRA_DIR/scripts/smoke.sh"
[ -f "$SMOKE" ] || die "smoke script not found: $SMOKE"

if [ -n "$(git -C "$REPO_ROOT" status --porcelain)" ]; then
    die "working tree $REPO_ROOT is not clean; commit or stash first"
fi
require_pushed "$REPO_ROOT"
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
    require_pushed "$FOXBMS_LOCAL"
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
echo "[1/9] Checking remote marker..."
REMOTE_MARKER="$(ssh -o BatchMode=yes "$VPS_HOST" "cat '$MARKER_FILE' 2>/dev/null || true" | tr -d '\r')"
[ "$REMOTE_MARKER" = "$MARKER_VALUE" ] || die "marker $MARKER_FILE on $VPS_HOST is '$REMOTE_MARKER', expected '$MARKER_VALUE'"

# Step 2: read-only check; the vcan@ units own the interfaces (RF-P1-04), no sudo here
echo "[2/9] Checking vcan0 and vcan1 are UP on VPS..."
VCAN_UP_RE='[<,]UP[,>]'
for ifn in vcan0 vcan1; do
    link="$(ssh -o BatchMode=yes "$VPS_HOST" "ip -br link show $ifn" 2>&1)" \
        || die "interface $ifn not found on $VPS_HOST ($link); check systemctl status vcan@$ifn"
    [[ "$link" =~ $VCAN_UP_RE ]] || die "interface $ifn is not UP on $VPS_HOST ($link); check systemctl status vcan@$ifn"
    echo "  $link"
done

# Step 3: sync the repository (host-only state is excluded from --delete)
echo "[3/9] Syncing taktflow-embedded-production to VPS..."
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
    --exclude '/.backup-*/' \
    "$REPO_ROOT/" "$VPS_HOST:$REMOTE_DIR/"

# Step 4: foxBMS sources, only when the foxbms profile is requested
echo "[4/9] foxbms-posix..."
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
echo "[5/9] Building images on VPS..."
BUILD_FLAGS="--build-arg GIT_SHA=$GIT_SHA"
[ "$FRESH" -eq 1 ] && BUILD_FLAGS="$BUILD_FLAGS --no-cache"
ssh "$VPS_HOST" "$REMOTE_ENV $COMPOSE build $BUILD_FLAGS"

# Step 6: start; orphans of removed services go, the standalone-ingress caddy stays off
echo "[6/9] Starting services..."
ssh "$VPS_HOST" "$REMOTE_ENV $COMPOSE up -d --remove-orphans"
# Host clock, because generated_at in /healthz is written by the host
UP_DONE_EPOCH="$(ssh "$VPS_HOST" "date -u +%s")"
[[ "$UP_DONE_EPOCH" =~ ^[0-9]+$ ]] || die "could not read the host clock after start: '$UP_DONE_EPOCH'"

# Step 7: the health file must be rewritten after the start, otherwise smoke could pass on the old one
echo "[7/9] Waiting for a fresh healthy /healthz of $PUBLIC_NAME (up to ${HEALTHZ_TIMEOUT_SEC}s)..."
wait_fresh_healthz "$PUBLIC_NAME" "$UP_DONE_EPOCH" "$HEALTHZ_TIMEOUT_SEC" "$HEALTHZ_RETRY_SEC" \
    || die "no fresh healthy /healthz within ${HEALTHZ_TIMEOUT_SEC}s; smoke not run, roll back (see runbook)"

# Step 8: smoke test from taktflow-infra, retried while containers settle
echo "[8/9] Smoke test (up to ${SMOKE_TIMEOUT_SEC}s)..."
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

# Step 9: only after a green smoke: prune dangling images (until now they are the
# rollback images) and record the deployed revision
echo "[9/9] Pruning dangling images, recording revision..."
ssh "$VPS_HOST" "docker image prune -f && printf '%s\n' '$GIT_SHA' > $REMOTE_DIR/docker/DEPLOYED_SHA"
ssh "$VPS_HOST" "cd $REMOTE_DIR/docker && $COMPOSE ps --format 'table {{.Name}}\t{{.Status}}'"

echo ""
echo "=== Deployment complete: $GIT_SHA ==="
