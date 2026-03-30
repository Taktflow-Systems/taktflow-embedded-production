/**
 * @file    Xcp.h
 * @brief   XCP slave — minimal measurement and calibration over CAN
 * @date    2026-03-21
 *
 * @details Implements ASAM MCD-1 XCP on CAN (subset):
 *          CONNECT, DISCONNECT, GET_STATUS, SHORT_UPLOAD, SHORT_DOWNLOAD.
 *          No DAQ/STIM (polling mode only). ~3 KB Flash, ~256 bytes RAM.
 *
 * @standard ASAM MCD-1 XCP V1.5
 * @copyright Taktflow Systems 2026
 */
#ifndef XCP_H
#define XCP_H

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ---- XCP Command Codes (request PID) ---- */

#define XCP_CMD_CONNECT              0xFFu
#define XCP_CMD_DISCONNECT           0xFEu
#define XCP_CMD_GET_STATUS           0xFDu
#define XCP_CMD_GET_COMM_MODE_INFO   0xFBu
#define XCP_CMD_SHORT_UPLOAD         0xF4u
#define XCP_CMD_SHORT_DOWNLOAD       0xEDu
#define XCP_CMD_SET_MTA              0xF6u
#define XCP_CMD_UPLOAD               0xF5u
#define XCP_CMD_GET_SEED             0xF8u
#define XCP_CMD_UNLOCK               0xF7u

/* ---- XCP Response Codes ---- */

#define XCP_RES_OK                   0xFFu
#define XCP_RES_ERR                  0xFEu

/* ---- XCP Error Codes ---- */

#define XCP_ERR_CMD_UNKNOWN          0x20u
#define XCP_ERR_OUT_OF_RANGE         0x22u
#define XCP_ERR_ACCESS_DENIED        0x24u
#define XCP_ERR_CMD_SYNTAX           0x21u
#define XCP_ERR_SEQUENCE             0x29u  /**< Seed/Key sequence error */
#define XCP_ERR_KEY_REJECTED         0x25u  /**< Invalid key */

/* ---- XCP Resource Bits (GET_STATUS) ---- */

#define XCP_RESOURCE_CAL_PAG         0x01u  /**< Calibration/Paging */
#define XCP_RESOURCE_DAQ             0x04u  /**< DAQ (not supported) */
#define XCP_RESOURCE_STIM            0x08u  /**< STIM (not supported) */
#define XCP_RESOURCE_PGM             0x10u  /**< Programming (not supported) */

/* ---- Configuration ---- */

/** Maximum XCP packet size (CAN 2.0B = 8 bytes) */
#define XCP_MAX_CTO                  8u
#define XCP_MAX_DTO                  8u

/** XCP protocol version */
#define XCP_PROTOCOL_VERSION_MAJOR   1u
#define XCP_PROTOCOL_VERSION_MINOR   0u
#define XCP_TRANSPORT_VERSION_MAJOR  1u
#define XCP_TRANSPORT_VERSION_MINOR  0u

/* ---- Per-ECU Configuration (set in Xcp_Cfg.h or main) ---- */

typedef struct {
    PduIdType  RxPduId;     /**< CanIf PDU ID for XCP request */
    PduIdType  TxPduId;     /**< CanIf PDU ID for XCP response */
} Xcp_ConfigType;

/* ---- API Functions ---- */

/**
 * @brief  Initialize XCP slave module
 * @param  ConfigPtr  Pointer to XCP configuration (CAN IDs)
 */
void Xcp_Init(const Xcp_ConfigType* ConfigPtr);

/**
 * @brief  Process incoming XCP request (called from PduR/CanIf RX path)
 * @param  RxPduId   PDU ID of received request
 * @param  PduInfoPtr  Pointer to received data
 */
void Xcp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/**
 * @brief  Check if XCP session is active (connected)
 * @return TRUE if connected, FALSE otherwise
 */
boolean Xcp_IsConnected(void);

/* Debug counters (bring-up diagnostics — volatile, accessed from main.c) */
extern volatile uint32 g_dbg_xcp_rx_count;
extern volatile uint32 g_dbg_xcp_tx_count;
extern volatile uint32 g_dbg_xcp_err_count;

#endif /* XCP_H */
