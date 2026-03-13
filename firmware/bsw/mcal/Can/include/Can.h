/**
 * @file    Can.h
 * @brief   CAN MCAL driver — AUTOSAR-like CAN 2.0B driver interface
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-001: CAN initialization
 * @safety_req SWR-BSW-002: CAN transmit
 * @safety_req SWR-BSW-003: CAN receive processing
 * @safety_req SWR-BSW-004: Bus-off recovery
 * @safety_req SWR-BSW-005: Error reporting
 * @traces_to  TSR-022, TSR-023, TSR-024, TSR-038, TSR-039
 *
 * @standard AUTOSAR_SWS_CANDriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef CAN_H
#define CAN_H

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ---- Constants ---- */

#define CAN_MAX_DLC         8u    /**< CAN 2.0B max data length      */
#define CAN_MAX_RX_PER_CALL 32u   /**< Max RX messages per MainFunc   */

/* ---- Types ---- */

/** CAN controller state (AUTOSAR Can_StateType) */
typedef enum {
    CAN_CS_UNINIT  = 0u,
    CAN_CS_STOPPED = 1u,
    CAN_CS_STARTED = 2u,
    CAN_CS_SLEEP   = 3u
} Can_StateType;

/** CAN write return type (AUTOSAR Can_ReturnType) */
typedef enum {
    CAN_OK      = 0u,    /**< Message queued successfully    */
    CAN_NOT_OK  = 1u,    /**< Error (bad param, not init)    */
    CAN_BUSY    = 2u     /**< TX mailbox full, retry later   */
} Can_ReturnType;

/** CAN PDU for transmission */
typedef struct {
    Can_IdType  id;       /**< CAN identifier (11-bit std)   */
    uint8       length;   /**< Data length 0..8              */
    uint8*      sdu;      /**< Pointer to data bytes         */
} Can_PduType;

/** CAN driver configuration */
typedef struct {
    uint32  baudrate;     /**< Baudrate in bps (e.g. 500000) */
    uint8   controllerId; /**< Controller index (0)          */
} Can_ConfigType;

/* ---- Hardware Abstraction (implemented per platform) ----
 * STM32: mcal/Can_Hw_STM32.c
 * Test:  Mocked in test/test_Can.c
 */
extern Std_ReturnType Can_Hw_Init(uint32 baudrate);
extern void           Can_Hw_Start(void);
extern void           Can_Hw_Stop(void);
extern Std_ReturnType Can_Hw_Transmit(Can_IdType id, const uint8* data, uint8 dlc);
extern boolean        Can_Hw_Receive(Can_IdType* id, uint8* data, uint8* dlc);
extern boolean        Can_Hw_IsBusOff(void);
extern void           Can_Hw_GetErrorCounters(uint8* tec, uint8* rec);

/* ---- CanIf Callbacks (provided by upper layer) ---- */
extern void CanIf_RxIndication(Can_IdType canId, const uint8* sduPtr, uint8 dlc);
extern void CanIf_ControllerBusOff(uint8 controllerId);

/* ---- API Functions ---- */

/**
 * @brief Initialize CAN driver and hardware
 * @param ConfigPtr  Configuration (must not be NULL)
 */
void Can_Init(const Can_ConfigType* ConfigPtr);

/**
 * @brief De-initialize CAN driver
 */
void Can_DeInit(void);

/**
 * @brief Set controller mode (STOPPED <-> STARTED)
 * @param Controller  Controller ID (0)
 * @param Mode        Target mode
 * @return E_OK on success, E_NOT_OK if transition invalid
 */
Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateType Mode);

/**
 * @brief Get current controller mode
 * @param Controller  Controller ID (0)
 * @return Current Can_StateType
 */
Can_StateType Can_GetControllerMode(uint8 Controller);

/**
 * @brief Queue a CAN frame for transmission
 * @param Hth      Hardware transmit handle (controller ID)
 * @param PduInfo  Pointer to PDU (must not be NULL, DLC 0..8)
 * @return CAN_OK, CAN_BUSY, or CAN_NOT_OK
 */
Can_ReturnType Can_Write(uint8 Hth, const Can_PduType* PduInfo);

/**
 * @brief Process received CAN frames (called cyclically, 5 ms)
 */
void Can_MainFunction_Read(void);

/**
 * @brief Check and handle bus-off condition (called cyclically)
 */
void Can_MainFunction_BusOff(void);

/**
 * @brief Read hardware error counters
 * @param Controller  Controller ID
 * @param tec         Output: transmit error counter
 * @param rec         Output: receive error counter
 * @return E_OK on success, E_NOT_OK on invalid params
 */
Std_ReturnType Can_GetErrorCounters(uint8 Controller, uint8* tec, uint8* rec);

/**
 * @brief Get controller error state (AUTOSAR Can_GetControllerErrorState)
 * @param ControllerId  Controller ID (0)
 * @param ErrorStatePtr Output: 0 = ACTIVE, 1 = PASSIVE, 2 = BUS_OFF
 * @return E_OK on success, E_NOT_OK on null pointer
 */
Std_ReturnType Can_GetControllerErrorState(uint8 ControllerId, uint8* ErrorStatePtr);

/* Debug counters (bring-up diagnostics — volatile, accessed from main.c) */
extern volatile uint32 g_can_rx_count;
extern volatile uint32 g_can_rx_last_id;
extern volatile uint32 g_can_tx_busy_count;
extern volatile uint32 g_can_rx_012_count;
extern volatile uint32 g_can_rx_011_count;

#endif /* CAN_H */
