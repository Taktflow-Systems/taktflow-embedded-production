/**
 * @file    sc_eth_telemetry.h
 * @brief   SC Ethernet UDP telemetry producer
 * @date    2026-07-06
 *
 * @details QM bench/HIL producer for the S-UDP-01 `SCET` payload. Active only
 *          when SC_ETH_ENABLE is defined.
 *
 * @verifies S-UDP-03
 * @note    Safety level: QM instrumentation only; excluded from safety builds.
 * @standard MISRA C:2012
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_ETH_TELEMETRY_H
#define SC_ETH_TELEMETRY_H

#include "sc_types.h"

#define SC_ETH_TELEMETRY_PAYLOAD_LEN       16u
#define SC_ETH_TELEMETRY_VERSION           1u
#define SC_ETH_TELEMETRY_HEADER_LEN        12u
#define SC_ETH_TELEMETRY_STATUS_LEN        4u
#define SC_ETH_TELEMETRY_TIMESTAMP_SOURCE  1u
#define SC_ETH_TELEMETRY_TICK_MS           10u

#ifndef SC_ETH_TELEMETRY_PERIOD_TICKS
#define SC_ETH_TELEMETRY_PERIOD_TICKS      1u
#endif

#ifndef SC_ETH_TELEMETRY_SRC_PORT
#define SC_ETH_TELEMETRY_SRC_PORT          55000u
#endif

#ifndef SC_ETH_TELEMETRY_DST_PORT
#define SC_ETH_TELEMETRY_DST_PORT          55001u
#endif

#ifdef SC_ETH_ENABLE

Std_ReturnType SC_EthTelemetry_Init(void);
void SC_EthTelemetry_Update(void);
Std_ReturnType SC_EthTelemetry_BuildPayload(uint8 *payload,
                                            uint16 payload_len,
                                            uint8 sequence_mod16);

#else

static inline Std_ReturnType SC_EthTelemetry_Init(void)
{
    return E_NOT_OK;
}

static inline void SC_EthTelemetry_Update(void)
{
}

static inline Std_ReturnType SC_EthTelemetry_BuildPayload(uint8 *payload,
                                                          uint16 payload_len,
                                                          uint8 sequence_mod16)
{
    (void)payload;
    (void)payload_len;
    (void)sequence_mod16;
    return E_NOT_OK;
}

#endif /* SC_ETH_ENABLE */

#endif /* SC_ETH_TELEMETRY_H */
