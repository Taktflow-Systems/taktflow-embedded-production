/**
 * @file    CanSM.h
 * @brief   CAN State Manager — controller state machine and bus-off recovery
 * @date    2026-03-21
 *
 * @details Manages CAN controller states (UNINIT, STOPPED, STARTED, BUS_OFF)
 *          with configurable L1 (fast) and L2 (slow) bus-off recovery.
 *          AUTOSAR-aligned but simplified for single-controller ECUs.
 *
 * @safety_req SWR-BSW-026
 * @standard AUTOSAR_SWS_CANStateManager
 * @copyright Taktflow Systems 2026
 */
#ifndef CANSM_H
#define CANSM_H

#include "Std_Types.h"

/* ---- CAN Controller States ---- */

typedef enum {
    CANSM_UNINIT   = 0u,  /**< Not initialized                         */
    CANSM_STOPPED  = 1u,  /**< Initialized but not communicating        */
    CANSM_STARTED  = 2u,  /**< Normal communication                    */
    CANSM_BUS_OFF  = 3u   /**< Bus-off detected, recovery in progress  */
} CanSM_StateType;

/* ---- Bus-Off Recovery Configuration ---- */

typedef struct {
    uint16  L1_RecoveryTimeMs;   /**< Fast recovery attempt period (typ 10ms)   */
    uint8   L1_MaxAttempts;      /**< Max fast recovery attempts (typ 5)        */
    uint16  L2_RecoveryTimeMs;   /**< Slow recovery attempt period (typ 1000ms) */
    uint8   L2_MaxAttempts;      /**< Max slow attempts before permanent off (typ 10) */
} CanSM_ConfigType;

/* ---- API Functions ---- */

/**
 * @brief  Initialize CanSM with recovery configuration
 * @param  ConfigPtr  Bus-off recovery parameters
 */
void CanSM_Init(const CanSM_ConfigType* ConfigPtr);

/**
 * @brief  Request transition to STARTED (enable communication)
 * @return E_OK if transition accepted
 */
Std_ReturnType CanSM_RequestComMode(void);

/**
 * @brief  Notify CanSM of bus-off event (called from CAN driver ISR)
 */
void CanSM_ControllerBusOff(void);

/**
 * @brief  Periodic function — drives bus-off recovery state machine
 *         Call from 10ms scheduler task.
 */
void CanSM_MainFunction(void);

/**
 * @brief  Get current CAN controller state
 * @return Current state (UNINIT/STOPPED/STARTED/BUS_OFF)
 */
CanSM_StateType CanSM_GetState(void);

/**
 * @brief  Check if communication is allowed
 * @return TRUE if state == STARTED, FALSE otherwise
 */
boolean CanSM_IsCommunicationAllowed(void);

#endif /* CANSM_H */
