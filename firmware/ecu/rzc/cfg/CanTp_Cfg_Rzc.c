/**
 * @file    CanTp_Cfg_Rzc.c
 * @brief   CanTp configuration for RZC — ISO-TP for UDS diagnostics
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 */
#include "CanTp.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * CanTp Channel Configuration — ISO-TP for UDS diagnostics
 * ================================================================== */

const CanTp_ConfigType rzc_cantp_config = {
    .rxPduId      = RZC_COM_RX_UDS_PHYS_REQ_RZC,   /* UDS physical request RX */
    .txPduId      = RZC_COM_TX_UDS_RESP_RZC,        /* UDS response TX */
    .fcTxPduId    = RZC_COM_TX_UDS_RESP_RZC,        /* Flow control TX (same as response) */
    .upperRxPduId = 0u,                              /* DCM RX PDU ID */
};
