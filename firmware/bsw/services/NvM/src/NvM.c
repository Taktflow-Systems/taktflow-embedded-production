/**
 * @file    NvM.c
 * @brief   NVRAM Manager — non-volatile memory block read/write
 * @date    2026-02-25
 *
 * @safety_req SWR-BSW-031
 * @traces_to  TSR-050
 *
 * @details  Dual-mode implementation:
 *           - POSIX builds (SIL/MIL): file-backed storage in /tmp/nvm_block_{id}.bin
 *           - Target builds: no-op stub (real flash driver to be integrated)
 *
 *           Reusable across all ECUs. Block size fixed at NVM_BLOCK_SIZE bytes.
 *
 * @standard AUTOSAR_SWS_NVRAMManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "NvM.h"
#include "Det.h"

/* ---- POSIX file-backed implementation (SIL / MIL / PIL host) ---- */
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__) || \
    defined(__POSIX__) || defined(POSIX_BUILD)

#include <unistd.h>
#include <fcntl.h>

#define NVM_PATH_PREFIX     "/tmp/nvm_block_"
#define NVM_PATH_SUFFIX     ".bin"
#define NVM_PATH_MAX        64u

/* Convert BlockId to decimal string, return number of chars written */
static uint8 NvM_U16ToStr(uint16 val, char* buf, uint8 maxLen)
{
    char tmp[6];    /* max 65535 = 5 digits + NUL */
    uint8 i = 0u;
    uint8 j;

    if (val == 0u) {
        if (maxLen > 0u) { buf[0] = '0'; }
        return 1u;
    }
    while ((val > 0u) && (i < 5u)) {
        tmp[i] = (char)((uint8)'0' + (uint8)(val % 10u));
        val /= 10u;
        i++;
    }
    /* Reverse into buf */
    for (j = 0u; j < i; j++) {
        if (j < maxLen) { buf[j] = tmp[(i - 1u) - j]; }
    }
    return i;
}

static void NvM_BuildPath(NvM_BlockIdType BlockId, char* path)
{
    uint8 pos = 0u;
    uint8 n;
    const char* prefix = NVM_PATH_PREFIX;
    const char* suffix = NVM_PATH_SUFFIX;

    while (*prefix != '\0') { path[pos] = *prefix; pos++; prefix++; }
    n = NvM_U16ToStr((uint16)BlockId, &path[pos], (uint8)(NVM_PATH_MAX - pos));
    pos += n;
    while (*suffix != '\0') { path[pos] = *suffix; pos++; suffix++; }
    path[pos] = '\0';
}

Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void* NvM_DstPtr)
{
    char path[NVM_PATH_MAX];
    int fd;
    ssize_t n;

    if (NvM_DstPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_NVM, 0u, NVM_API_READ_BLOCK, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    NvM_BuildPath(BlockId, path);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        /* File not found — first boot, no stored data */
        return E_OK;
    }

    n = read(fd, NvM_DstPtr, (size_t)NVM_BLOCK_SIZE);
    (void)close(fd);
    (void)n;

    return E_OK;
}

Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void* NvM_SrcPtr)
{
    char path[NVM_PATH_MAX];
    int fd;
    ssize_t n;

    if (NvM_SrcPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_NVM, 0u, NVM_API_WRITE_BLOCK, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    NvM_BuildPath(BlockId, path);

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 420);  /* 0644 decimal */
    if (fd < 0) {
        return E_NOT_OK;
    }

    n = write(fd, NvM_SrcPtr, (size_t)NVM_BLOCK_SIZE);
    (void)fsync(fd);
    (void)close(fd);
    (void)n;

    return E_OK;
}

/* ---- Target stub (real flash driver integration point) ---- */
#else

Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void* NvM_DstPtr)
{
    (void)BlockId;
    (void)NvM_DstPtr;
    /* TODO:HARDWARE — integrate with flash/EEPROM driver */
    return E_OK;
}

Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void* NvM_SrcPtr)
{
    (void)BlockId;
    (void)NvM_SrcPtr;
    /* TODO:HARDWARE — integrate with flash/EEPROM driver */
    return E_OK;
}

#endif
