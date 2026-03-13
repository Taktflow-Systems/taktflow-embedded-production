/**
 * @file    Can_Posix.h
 * @brief   POSIX SocketCAN backend for CAN MCAL driver
 * @date    2026-02-23
 *
 * @details Implements Can_Hw_* extern functions from Can.h using Linux
 *          SocketCAN. Used by simulated ECUs (BCM, ICU, TCU) running
 *          in Docker containers on vcan0.
 *
 * @safety_req SWR-BSW-001, SWR-BSW-002, SWR-BSW-003
 * @traces_to  TSR-022, TSR-023, TSR-024
 *
 * @copyright Taktflow Systems 2026
 */
#ifndef CAN_POSIX_H
#define CAN_POSIX_H

#include "Std_Types.h"
#include "ComStack_Types.h"

/**
 * @brief  Get the SocketCAN file descriptor (for advanced usage/diagnostics)
 * @return Socket fd, or -1 if not initialized
 */
int Can_Posix_GetFd(void);

#endif /* CAN_POSIX_H */
