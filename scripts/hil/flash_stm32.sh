#!/bin/bash
# @file       flash_stm32.sh
# @brief      Build and flash one STM32 ECU (CVC, FZC, or RZC) via st-flash
# @aspice     SWE.6 — Software Qualification Testing
#
# Usage:
#   ./scripts/hil/flash_stm32.sh cvc [--build-only] [--flash-only] [--serial SERIAL]
#   ./scripts/hil/flash_stm32.sh fzc
#   ./scripts/hil/flash_stm32.sh rzc
#
# Requires: arm-none-eabi-gcc, st-flash (stlink-tools)
#
# Board serial mapping (set via env or --serial):
#   STLINK_CVC=STLINK-CVC-PLACEHOLDER
#   STLINK_FZC=STLINK-FZC-PLACEHOLDER
#   STLINK_RZC=<third board serial>

set -euo pipefail

TARGET="${1:?Usage: $0 <cvc|fzc|rzc> [--build-only|--flash-only] [--serial SERIAL]}"
shift

FLAG=""
SERIAL=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-only) FLAG="build-only" ;;
        --flash-only) FLAG="flash-only" ;;
        --serial)     SERIAL="$2"; shift ;;
        *)            echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${PROJECT_ROOT}/build/stm32/${TARGET}.bin"
ELF="${PROJECT_ROOT}/build/stm32/${TARGET}.elf"

# Resolve serial from env if not passed
if [ -z "${SERIAL}" ]; then
    VAR="STLINK_${TARGET^^}"
    SERIAL="${!VAR:-}"
fi

# --- Verify binary matches current commit (MANDATORY) ---
verify_binary() {
    local elf="$1"
    local target="$2"
    local head_hash
    head_hash=$(cd "${PROJECT_ROOT}" && git rev-parse --short=8 HEAD 2>/dev/null || echo "unknown")

    if [ ! -f "${elf}" ]; then
        echo "  [FAIL] ${elf} not found. Build first."
        exit 1
    fi

    # Extract GIT_HASH burned into binary
    local binary_hash
    binary_hash=$(strings "${elf}" | grep -oE "${target^^} Boot.*\[([a-f0-9]{8})\]" | grep -oE '\[([a-f0-9]{8})\]' | tr -d '[]' || true)

    if [ -z "${binary_hash}" ]; then
        # Fallback: search for any 8-char hex after "Boot"
        binary_hash=$(strings "${elf}" | grep -oE '\[([a-f0-9]{8})\]' | head -1 | tr -d '[]' || true)
    fi

    if [ -z "${binary_hash}" ]; then
        echo "  [FAIL] Cannot extract GIT_HASH from ${elf}"
        echo "         Binary may be corrupt or built without GIT_HASH."
        echo "         Rebuild: make -f firmware/platform/stm32/Makefile.stm32 TARGET=${target}"
        exit 1
    fi

    if [ "${binary_hash}" != "${head_hash}" ]; then
        echo "  [FAIL] STALE BINARY — git hash mismatch!"
        echo "         Binary:  ${binary_hash}"
        echo "         HEAD:    ${head_hash}"
        echo "         Rebuild: make -f firmware/platform/stm32/Makefile.stm32 TARGET=${target}"
        exit 1
    fi

    echo "  [OK] Binary verified: ${target^^} [${binary_hash}]"
}

echo "=== STM32 ${TARGET^^} ==="

# --- Build ---
if [ "${FLAG}" != "flash-only" ]; then
    echo "  Building ${TARGET}..."
    make -f "${PROJECT_ROOT}/firmware/platform/stm32/Makefile.stm32" \
        TARGET="${TARGET}" \
        -C "${PROJECT_ROOT}" \
        -j"$(nproc)"
    echo "  [OK] Build complete"
    arm-none-eabi-size "${ELF}"
fi

# --- Flash ---
if [ "${FLAG}" != "build-only" ]; then
    # Pre-flash: verify binary is from current commit
    verify_binary "${ELF}" "${TARGET}"

    FLASH_ARGS="write ${BIN} 0x08000000"
    if [ -n "${SERIAL}" ]; then
        FLASH_ARGS="--serial ${SERIAL} ${FLASH_ARGS}"
    fi

    echo "  Flashing ${TARGET} via st-flash..."
    if ! st-flash ${FLASH_ARGS} 2>&1; then
        echo "  Retrying with --connect-under-reset..."
        st-flash --connect-under-reset ${FLASH_ARGS} 2>&1
    fi
    echo "  [OK] ${TARGET^^} flashed [$(cd "${PROJECT_ROOT}" && git rev-parse --short=8 HEAD)]"
fi
