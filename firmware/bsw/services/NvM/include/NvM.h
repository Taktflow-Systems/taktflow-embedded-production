/**
 * @file    NvM.h
 * @brief   NVRAM Manager — non-volatile memory block read/write
 * @date    2026-02-25
 *
 * @safety_req SWR-BSW-031
 * @traces_to  TSR-050
 *
 * @details  Minimal AUTOSAR NvM API stub providing NvM_ReadBlock and
 *           NvM_WriteBlock for BSW consumers (FZC NVM, CVC NVM, etc.).
 *           Block ID selects the NVM dataset; data is copied to/from
 *           the caller-provided RAM mirror.
 *
 *           SIL implementation: no-op (returns E_OK, no persistent storage).
 *
 * @standard AUTOSAR_SWS_NVRAMManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef NVM_H
#define NVM_H

#include "Std_Types.h"

/* ---- Constants ---- */

#define NVM_BLOCK_SIZE  1024u   /**< Max bytes per NvM block */

/* ---- Types ---- */

typedef uint16 NvM_BlockIdType;

/* ---- API Functions ---- */

/**
 * @brief  Read an NVM block into RAM mirror
 * @param  BlockId   NVM block identifier
 * @param  NvM_DstPtr Destination RAM buffer
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void* NvM_DstPtr);

/**
 * @brief  Write RAM mirror contents to NVM block
 * @param  BlockId   NVM block identifier
 * @param  NvM_SrcPtr Source RAM buffer
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void* NvM_SrcPtr);

#endif /* NVM_H */
