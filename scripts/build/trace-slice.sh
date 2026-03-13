#!/usr/bin/env bash
set -euo pipefail

# trace-slice.sh
# Small, deterministic trace extraction to avoid large-context AI runs.
#
# Usage:
#   bash scripts/trace-slice.sh --layer stk-sys
#   bash scripts/trace-slice.sh --layer stk-sys --from STK-011 --to STK-020
#   bash scripts/trace-slice.sh --layer sys-swr --from SYS-037 --to SYS-045
#   bash scripts/trace-slice.sh --layer stk-sys --out docs/tmp/stk_sys.csv
#   bash scripts/trace-slice.sh --layer sys-swr --direction reverse
#
# Output CSV columns:
#   layer=stk-sys -> STK_ID,SYS_ID
#   layer=sys-swr -> SYS_ID,SWR_ID

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SYSREQ="$ROOT_DIR/docs/aspice/system/system-requirements.md"
SWR_DIR="$ROOT_DIR/docs/aspice/software/sw-requirements"
OUT=""
LAYER=""
FROM_ID=""
TO_ID=""
DIRECTION="both"

usage() {
  cat <<'EOF'
Usage:
  bash scripts/trace-slice.sh --layer <stk-sys|sys-swr> [--from ID] [--to ID] [--out PATH] [--direction forward|reverse|both]

Examples:
  bash scripts/trace-slice.sh --layer stk-sys
  bash scripts/trace-slice.sh --layer stk-sys --from STK-011 --to STK-020
  bash scripts/trace-slice.sh --layer sys-swr --from SYS-037 --to SYS-045
  bash scripts/trace-slice.sh --layer stk-sys --out docs/tmp/stk_sys.csv
  bash scripts/trace-slice.sh --layer sys-swr --direction reverse
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --layer)
      LAYER="${2:-}"
      shift 2
      ;;
    --from)
      FROM_ID="${2:-}"
      shift 2
      ;;
    --to)
      TO_ID="${2:-}"
      shift 2
      ;;
    --out)
      OUT="${2:-}"
      shift 2
      ;;
    --direction)
      DIRECTION="${2:-both}"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [ -z "$LAYER" ]; then
  echo "--layer is required" >&2
  usage
  exit 2
fi

if [ ! -f "$SYSREQ" ]; then
  echo "Missing file: $SYSREQ" >&2
  exit 1
fi

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

in_range() {
  # Compare by numeric suffix only (e.g., STK-011 -> 11)
  local id="$1" from="$2" to="$3"
  local n fn tn
  n="$(echo "$id" | sed -E 's/.*-([0-9]+)$/\1/')"
  if [ -z "$from" ] && [ -z "$to" ]; then
    return 0
  fi
  if [ -n "$from" ]; then
    fn="$(echo "$from" | sed -E 's/.*-([0-9]+)$/\1/')"
    if [ "$n" -lt "$fn" ]; then
      return 1
    fi
  fi
  if [ -n "$to" ]; then
    tn="$(echo "$to" | sed -E 's/.*-([0-9]+)$/\1/')"
    if [ "$n" -gt "$tn" ]; then
      return 1
    fi
  fi
  return 0
}

# -----------------------------------------------------------------------
# STK-SYS extraction: reverse (SYS "Traces up") + forward (Section 18 matrix)
# -----------------------------------------------------------------------
extract_stk_sys_reverse() {
  # Parse SYS sections for "Traces up: STK-NNN"
  awk '
    /^### SYS-[0-9]+:/ {
      match($0, /SYS-[0-9]+/, m)
      cur_sys = m[0]
      next
    }
    /- \*\*Traces up\*\*:/ {
      if (cur_sys == "") next
      line = $0
      while (match(line, /STK-[0-9]+/, m)) {
        print m[0] "," cur_sys
        line = substr(line, RSTART + RLENGTH)
      }
    }
  ' "$SYSREQ"
}

extract_stk_sys_forward() {
  # Parse Section 18 forward matrix table: | STK-NNN | SYS-NNN, ... |
  awk '
    /^## 18\. Traceability Matrix/ { in_matrix = 1; next }
    /^### 18\.[1-9]/ { in_matrix = 0; next }
    /^## [^1]/ { in_matrix = 0; next }
    /^## 1[^8]/ { in_matrix = 0; next }
    in_matrix && /^\| STK-[0-9]+/ {
      match($0, /STK-[0-9]+/, stk)
      # Extract everything after first |...|
      line = $0
      sub(/^\|[^|]+\|/, "", line)
      while (match(line, /SYS-[0-9]+/, m)) {
        print stk[0] "," m[0]
        line = substr(line, RSTART + RLENGTH)
      }
    }
  ' "$SYSREQ"
}

extract_stk_sys() {
  {
    if [ "$DIRECTION" = "reverse" ] || [ "$DIRECTION" = "both" ]; then
      extract_stk_sys_reverse
    fi
    if [ "$DIRECTION" = "forward" ] || [ "$DIRECTION" = "both" ]; then
      extract_stk_sys_forward
    fi
  } | sort -u
}

# -----------------------------------------------------------------------
# SYS-SWR extraction: forward (SYS "Traces down") + reverse (SWR "Traces up")
# -----------------------------------------------------------------------
extract_sys_swr_forward() {
  # Parse system-requirements.md "Traces down: SWR-NNN"
  awk '
    /^### SYS-[0-9]+:/ {
      match($0, /SYS-[0-9]+/, m)
      cur_sys = m[0]
      next
    }
    /- \*\*Traces down\*\*:/ {
      if (cur_sys == "") next
      line = $0
      while (match(line, /SWR-[A-Z]+-[0-9]+/, m)) {
        print cur_sys "," m[0]
        line = substr(line, RSTART + RLENGTH)
      }
    }
  ' "$SYSREQ"
}

extract_sys_swr_reverse() {
  # Parse SWR-*.md files for "Traces up: SYS-NNN"
  if [ ! -d "$SWR_DIR" ]; then
    return
  fi
  for swr_file in "$SWR_DIR"/SWR-*.md; do
    [ -f "$swr_file" ] || continue
    awk '
      /^### SWR-[A-Z]+-[0-9]+:/ {
        match($0, /SWR-[A-Z]+-[0-9]+/, m)
        cur_swr = m[0]
        next
      }
      /- \*\*Traces up\*\*:/ {
        if (cur_swr == "") next
        line = $0
        while (match(line, /SYS-[0-9]+/, m)) {
          print m[0] "," cur_swr
          line = substr(line, RSTART + RLENGTH)
        }
      }
    ' "$swr_file"
  done
}

extract_sys_swr() {
  {
    if [ "$DIRECTION" = "forward" ] || [ "$DIRECTION" = "both" ]; then
      extract_sys_swr_forward
    fi
    if [ "$DIRECTION" = "reverse" ] || [ "$DIRECTION" = "both" ]; then
      extract_sys_swr_reverse
    fi
  } | sort -u
}

# -----------------------------------------------------------------------
# Main dispatch
# -----------------------------------------------------------------------
case "$LAYER" in
  stk-sys)
    {
      echo "STK_ID,SYS_ID"
      extract_stk_sys | while IFS=, read -r left right; do
        if in_range "$left" "$FROM_ID" "$TO_ID"; then
          echo "$left,$right"
        fi
      done
    } > "$tmp"
    ;;
  sys-swr)
    {
      echo "SYS_ID,SWR_ID"
      extract_sys_swr | while IFS=, read -r left right; do
        if in_range "$left" "$FROM_ID" "$TO_ID"; then
          echo "$left,$right"
        fi
      done
    } > "$tmp"
    ;;
  *)
    echo "Invalid --layer: $LAYER (expected stk-sys or sys-swr)" >&2
    exit 2
    ;;
esac

if [ -n "$OUT" ]; then
  out_abs="$ROOT_DIR/$OUT"
  mkdir -p "$(dirname "$out_abs")"
  cp "$tmp" "$out_abs"
  echo "Wrote: $out_abs"
else
  cat "$tmp"
fi
