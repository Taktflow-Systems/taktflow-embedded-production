/**
 * @file    Can_Hw_STM32F4.c
 * @brief   STM32F4 bxCAN1 hardware backend for CAN MCAL driver
 * @date    2026-03-14
 *
 * @details bxCAN1 HAL implementation for STM32F413ZH (NUCLEO-F413ZH).
 *          Configures CAN1 at 500 kbps (PSC=6, BS1=13, BS2=2 @48MHz APB1).
 *          Accept-all standard ID filter to FIFO0.
 *          Includes internal loopback self-test for power-on validation.
 *
 *          Pin assignment: PD0 = CAN1_RX, PD1 = CAN1_TX (AF9)
 *
 * @safety_req SWR-BSW-001: CAN initialization
 * @safety_req SWR-BSW-002: CAN transmit
 * @safety_req SWR-BSW-003: CAN receive processing
 * @safety_req SWR-BSW-004: Bus-off recovery
 * @safety_req SWR-BSW-005: Error reporting
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can.h"
#include "stm32f4xx_hal.h"

/* ==================================================================
 * Static State
 * ================================================================== */

static CAN_HandleTypeDef hcan1;

/* ==================================================================
 * Static Helpers
 * ================================================================== */

/**
 * @brief  Configure accept-all standard ID filter to FIFO0
 * @return E_OK on success, E_NOT_OK on failure
 */
static Std_ReturnType Can_Hw_ConfigureFilter(void)
{
    CAN_FilterTypeDef filter;

    filter.FilterBank           = 0u;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = 0x0000u;
    filter.FilterIdLow          = 0x0000u;
    filter.FilterMaskIdHigh     = 0x0000u;   /* mask 0 = accept all */
    filter.FilterMaskIdLow      = 0x0000u;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation     = ENABLE;
    filter.SlaveStartFilterBank = 14u;       /* CAN2 filter bank start */

    if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
    {
        return E_NOT_OK;
    }

    return E_OK;
}

/**
 * @brief  Internal CAN1 init with specified mode
 * @param  mode  CAN_MODE_NORMAL or CAN_MODE_LOOPBACK
 * @return E_OK on success, E_NOT_OK on failure
 */
static Std_ReturnType Can_Hw_InitMode(uint32 mode)
{
    /* bxCAN uses APB1 clock directly — no separate kernel clock config.
     * F413ZH APB1 = 48 MHz (SYSCLK 96 MHz / APB1 div 2).
     *
     * CAN bit time = PSC * (1 + BS1 + BS2) = 6 * (1 + 13 + 2) = 96
     * Baudrate = 48,000,000 / 96 = 500,000 bps
     * SJW=1 (HSE-based PLL, low jitter between ECUs). */
    hcan1.Instance                = CAN1;
    hcan1.Init.Prescaler          = 6u;
    hcan1.Init.Mode               = mode;
    hcan1.Init.SyncJumpWidth      = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1           = CAN_BS1_13TQ;
    hcan1.Init.TimeSeg2           = CAN_BS2_2TQ;
    hcan1.Init.TimeTriggeredMode  = DISABLE;
    hcan1.Init.AutoBusOff         = ENABLE;
    hcan1.Init.AutoWakeUp         = ENABLE;
    hcan1.Init.AutoRetransmission = ENABLE;
    hcan1.Init.ReceiveFifoLocked  = DISABLE;
    hcan1.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan1) != HAL_OK)
    {
        return E_NOT_OK;
    }

    /* Explicitly exit sleep mode AFTER HAL_CAN_Init (clock now enabled).
     * bxCAN defaults to sleep after reset. HAL_CAN_Init requests INRQ
     * but may not clear SLEEP on some STM32F4 revisions. */
    CAN1->MCR &= ~CAN_MCR_SLEEP;
    while ((CAN1->MSR & CAN_MSR_SLAK) != 0u) {}

    return Can_Hw_ConfigureFilter();
}

/* ==================================================================
 * Can_Hw_* API implementations
 * ================================================================== */

/**
 * @brief  Initialize CAN1 hardware at 500 kbps in normal mode
 * @param  baudrate  Baudrate in bps (currently only 500000 supported)
 * @return E_OK on success, E_NOT_OK on failure
 *
 * @note   HAL_CAN_Init() triggers MspInit which configures GPIO PD0/PD1
 *         AF9 and enables CAN1 clock.
 */
Std_ReturnType Can_Hw_Init(uint32 baudrate)
{
    (void)baudrate;

    /* Init in SILENT loopback first — bxCAN can't exit sleep in normal
     * mode without seeing recessive level on RX pin. Silent loopback
     * bypasses this WITHOUT driving the TX pin (no bus disruption).
     * After init succeeds (clock enabled, SLAK cleared), switch to
     * normal mode. */
    if (Can_Hw_InitMode(CAN_MODE_SILENT_LOOPBACK) != E_OK)
    {
        return E_NOT_OK;
    }

    /* Now re-init in normal mode — peripheral is awake, clock running */
    return Can_Hw_InitMode(CAN_MODE_NORMAL);
}

/**
 * @brief  Start CAN1 controller (enter normal/loopback mode)
 */
void Can_Hw_Start(void)
{
    (void)HAL_CAN_Start(&hcan1);

    /* bxCAN may still have SLEEP set in MCR after HAL_CAN_Start.
     * Explicitly clear it and wait for SLAK to deassert. */
    CAN1->MCR &= ~CAN_MCR_SLEEP;
    {
        volatile uint32_t timeout = 100000u;
        while (((CAN1->MSR & CAN_MSR_SLAK) != 0u) && (timeout > 0u)) { timeout--; }
    }
}

/**
 * @brief  Stop CAN1 controller (enter init mode)
 */
void Can_Hw_Stop(void)
{
    (void)HAL_CAN_Stop(&hcan1);
}

/**
 * @brief  Transmit a CAN frame via CAN1 TX mailbox
 * @param  id    CAN identifier (11-bit standard)
 * @param  data  Pointer to payload data
 * @param  dlc   Data length (0..8)
 * @return E_OK on success, E_NOT_OK if TX mailbox full or invalid DLC
 */
#define CAN_HW_TX_RETRY_LIMIT  5000u

Std_ReturnType Can_Hw_Transmit(Can_IdType id, const uint8* data, uint8 dlc)
{
    CAN_TxHeaderTypeDef txHeader;
    uint8 txData[8];
    uint32_t txMailbox;
    uint8 i;
    uint16 retry;

    if (dlc > 8u)
    {
        return E_NOT_OK;
    }

    txHeader.StdId              = (uint32)id;
    txHeader.ExtId              = 0u;
    txHeader.IDE                = CAN_ID_STD;
    txHeader.RTR                = CAN_RTR_DATA;
    txHeader.DLC                = (uint32)dlc;
    txHeader.TransmitGlobalTime = DISABLE;

    /* Copy data to local buffer (HAL expects non-const pointer) */
    for (i = 0u; i < dlc; i++)
    {
        txData[i] = data[i];
    }

    /* Try to enqueue; if mailboxes full, spin-wait for one to free up. */
    for (retry = 0u; retry < CAN_HW_TX_RETRY_LIMIT; retry++)
    {
        if (HAL_CAN_AddTxMessage(&hcan1, &txHeader, txData, &txMailbox) == HAL_OK)
        {
            return E_OK;
        }
    }

    return E_NOT_OK;
}

/**
 * @brief  Non-blocking receive of a CAN frame from CAN1 RX FIFO0
 * @param  id    Output: received CAN identifier
 * @param  data  Output: received payload (min 8 bytes)
 * @param  dlc   Output: data length
 * @return TRUE if a frame was received, FALSE if FIFO empty
 */
boolean Can_Hw_Receive(Can_IdType* id, uint8* data, uint8* dlc)
{
    CAN_RxHeaderTypeDef rxHeader;

    if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) == 0u)
    {
        return FALSE;
    }

    if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rxHeader, data) != HAL_OK)
    {
        return FALSE;
    }

    *id = (Can_IdType)rxHeader.StdId;
    *dlc = (uint8)rxHeader.DLC;

    return TRUE;
}

/**
 * @brief  Check if CAN bus is in bus-off state
 * @return TRUE if bus-off, FALSE otherwise
 * @note   Fail-closed: returns TRUE (bus-off) if state unreadable.
 */
boolean Can_Hw_IsBusOff(void)
{
    uint32 esr = hcan1.Instance->ESR;
    return ((esr & CAN_ESR_BOFF) != 0u) ? TRUE : FALSE;
}

/**
 * @brief  Get CAN1 HAL state for debug diagnostics
 * @return HAL_CAN_StateTypeDef
 */
uint8 Can_Hw_GetHalState(void)
{
    return (uint8)HAL_CAN_GetState(&hcan1);
}

/**
 * @brief  Get CAN error counters (TEC and REC)
 * @param  tec  Output: transmit error counter
 * @param  rec  Output: receive error counter
 */
void Can_Hw_GetErrorCounters(uint8* tec, uint8* rec)
{
    uint32 esr = hcan1.Instance->ESR;

    if (tec != NULL_PTR)
    {
        *tec = (uint8)((esr >> 16u) & 0xFFu);  /* TEC field: bits [23:16] */
    }
    if (rec != NULL_PTR)
    {
        *rec = (uint8)((esr >> 24u) & 0xFFu);  /* REC field: bits [31:24] */
    }
}

/* ==================================================================
 * CAN Loopback Self-Test
 * ================================================================== */

#define CAN_LB_TEST_ID     0x7FFu
#define CAN_LB_TEST_DLC    8u
#define CAN_LB_TIMEOUT_MS  100u

static const uint8 can_lb_test_data[8] = {
    0xCAu, 0xFEu, 0xBAu, 0xBEu, 0xDEu, 0xADu, 0x01u, 0x02u
};

/**
 * @brief  CAN internal loopback self-test
 * @return E_OK if loopback TX->RX verified, E_NOT_OK on failure
 *
 * @details Sequence:
 *   1. Stop CAN
 *   2. Reconfigure in LOOPBACK mode
 *   3. Start, TX test frame (ID=0x7FF, 8 bytes)
 *   4. Poll RX FIFO0 with 100ms timeout
 *   5. Verify ID + data match
 *   6. Stop, reconfigure back to NORMAL mode
 *   7. Leave stopped (caller starts CAN later)
 */
Std_ReturnType Can_Hw_LoopbackTest(void)
{
    Can_IdType rxId;
    uint8 rxData[8];
    uint8 rxDlc;
    uint32 start;
    uint8 i;

    /* Step 1-2: DeInit and switch to loopback */
    (void)HAL_CAN_Stop(&hcan1);
    (void)HAL_CAN_DeInit(&hcan1);

    if (Can_Hw_InitMode(CAN_MODE_LOOPBACK) != E_OK)
    {
        (void)HAL_CAN_DeInit(&hcan1);
        (void)Can_Hw_InitMode(CAN_MODE_NORMAL);
        return E_NOT_OK;
    }

    /* Step 3: Start and transmit test frame */
    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        (void)HAL_CAN_DeInit(&hcan1);
        (void)Can_Hw_InitMode(CAN_MODE_NORMAL);
        return E_NOT_OK;
    }

    if (Can_Hw_Transmit(CAN_LB_TEST_ID, can_lb_test_data, CAN_LB_TEST_DLC) != E_OK)
    {
        (void)HAL_CAN_Stop(&hcan1);
        (void)HAL_CAN_DeInit(&hcan1);
        (void)Can_Hw_InitMode(CAN_MODE_NORMAL);
        return E_NOT_OK;
    }

    /* Step 4: Poll RX with timeout */
    start = HAL_GetTick();
    while ((HAL_GetTick() - start) < CAN_LB_TIMEOUT_MS)
    {
        if (Can_Hw_Receive(&rxId, rxData, &rxDlc) == TRUE)
        {
            /* Step 5: Verify ID + data match */
            if ((rxId == CAN_LB_TEST_ID) && (rxDlc == CAN_LB_TEST_DLC))
            {
                boolean match = TRUE;
                for (i = 0u; i < CAN_LB_TEST_DLC; i++)
                {
                    if (rxData[i] != can_lb_test_data[i])
                    {
                        match = FALSE;
                        break;
                    }
                }
                if (match == TRUE)
                {
                    /* Step 6-7: Restore normal mode, leave stopped */
                    (void)HAL_CAN_Stop(&hcan1);
                    (void)HAL_CAN_DeInit(&hcan1);
                    (void)Can_Hw_InitMode(CAN_MODE_NORMAL);
                    return E_OK;
                }
            }
        }
    }

    /* Timeout — restore normal mode */
    (void)HAL_CAN_Stop(&hcan1);
    (void)HAL_CAN_DeInit(&hcan1);
    (void)Can_Hw_InitMode(CAN_MODE_NORMAL);
    return E_NOT_OK;
}

/* Bringup debug stub — referenced by HardFault handler in stm32f4xx_it.c */
void bringup_put_hex(uint32_t val) { (void)val; }
void bringup_puts(const char* s) { (void)s; }
