/**
 * @file    sc_eth.h
 * @brief   QM Ethernet bench driver API for the Safety Controller
 * @date    2026-07-06
 *
 * @details Raw EMAC/DP83630 wrapper for bench and HIL instrumentation.
 *          Implementations are compiled only when SC_ETH_ENABLE is defined.
 *
 * @note    Safety level: QM instrumentation only; excluded from safety builds.
 * @standard MISRA C:2012
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_ETH_H
#define SC_ETH_H

#ifndef __HAL_STDTYPES_H__
#include "sc_types.h"
#endif

/** Maximum Ethernet frame bytes copied by the SC Ethernet driver. */
#define SC_ETH_FRAME_MAX_LEN        1520u

/** Minimum TX buffer length used by the proven standalone ping path. */
#define SC_ETH_MIN_TX_FRAME_LEN     74u

/** Ethernet MAC address length. */
#define SC_ETH_MAC_ADDR_LEN         6u

/**
 * @brief  Initialize PHY wake/link wait and HALCoGen EMAC hardware.
 *
 * @param  mac  6-byte locally administered MAC address.
 * @return E_OK when link was observed and EMACHWInit returned OK; E_NOT_OK otherwise.
 */
Std_ReturnType Sc_Eth_Init(const uint8 *mac);

/**
 * @brief  Poll one completed RX descriptor and copy the frame into caller storage.
 *
 * @param  frame       Destination frame buffer.
 * @param  frame_size  Destination buffer capacity in bytes.
 * @param  frame_len   Output: received frame length in bytes.
 * @return TRUE when a complete frame was copied; FALSE otherwise.
 */
boolean Sc_Eth_PollRx(uint8 *frame, uint16 frame_size, uint16 *frame_len);

/**
 * @brief  Transmit one raw Ethernet frame.
 *
 * @param  frame      Mutable frame buffer; short frames are zero-padded in place.
 * @param  frame_len  Frame length in bytes before padding.
 * @return E_OK when the frame was handed to EMACTransmit; E_NOT_OK otherwise.
 */
Std_ReturnType Sc_Eth_Tx(uint8 *frame, uint16 frame_len);

/**
 * @brief  Read PHY link status.
 *
 * @return TRUE when the PHY BSR link bit is set; FALSE otherwise.
 */
boolean Sc_Eth_LinkUp(void);

#endif /* SC_ETH_H */
