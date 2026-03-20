/**
 * @file    Can_Hw_STM32.c
 * @brief   STM32 FDCAN1 hardware backend for CAN MCAL driver
 * @date    2026-03-03
 *
 * @details Phase F2: Real FDCAN1 HAL implementation for STM32G474RE.
 *          Configures FDCAN1 at 500 kbps (PSC=17, Seg1=15, Seg2=4 @170MHz).
 *          Accept-all standard ID filter to FIFO0.
 *          Includes internal loopback self-test for power-on validation.
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
#include "stm32g4xx_hal.h"

/* ==================================================================
 * Static State
 * ================================================================== */

static FDCAN_HandleTypeDef hfdcan1;

/** DLC value to HAL FDCAN DLC constant lookup */
static const uint32 dlc_to_hal[9] = {
    FDCAN_DLC_BYTES_0,
    FDCAN_DLC_BYTES_1,
    FDCAN_DLC_BYTES_2,
    FDCAN_DLC_BYTES_3,
    FDCAN_DLC_BYTES_4,
    FDCAN_DLC_BYTES_5,
    FDCAN_DLC_BYTES_6,
    FDCAN_DLC_BYTES_7,
    FDCAN_DLC_BYTES_8
};

/* ==================================================================
 * Static Helpers
 * ================================================================== */

/**
 * @brief  Configure accept-all standard ID filter to FIFO0
 * @return E_OK on success, E_NOT_OK on failure
 * @note   Must be called after every HAL_FDCAN_Init() because init
 *         resets the message RAM configuration.
 */
static Std_ReturnType Can_Hw_ConfigureFilter(void)
{
    FDCAN_FilterTypeDef filter;

    filter.IdType       = FDCAN_STANDARD_ID;
    filter.FilterIndex  = 0u;
    filter.FilterType   = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = 0x000u;   /* match value  */
    filter.FilterID2    = 0x000u;   /* mask: 0 = accept all */

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK)
    {
        return E_NOT_OK;
    }

    /* Reject all extended frames and remote frames */
    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
            FDCAN_ACCEPT_IN_RX_FIFO0,  /* non-matching std → FIFO0 (accept all) */
            FDCAN_REJECT,               /* non-matching ext → reject */
            FDCAN_REJECT_REMOTE,        /* remote std → reject */
            FDCAN_REJECT_REMOTE)        /* remote ext → reject */
        != HAL_OK)
    {
        return E_NOT_OK;
    }

    return E_OK;
}

/**
 * @brief  Internal FDCAN1 init with specified mode
 * @param  mode  FDCAN_MODE_NORMAL or FDCAN_MODE_INTERNAL_LOOPBACK
 * @return E_OK on success, E_NOT_OK on failure
 */
static Std_ReturnType Can_Hw_InitMode(uint32 mode)
{
    /* Select FDCAN kernel clock: PCLK1 (170 MHz).
     * Reset default is HSE which is NOT enabled in our config. */
    {
        RCC_PeriphCLKInitTypeDef pclk = {0};
        pclk.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
        pclk.FdcanClockSelection  = RCC_FDCANCLKSOURCE_PCLK1;
        (void)HAL_RCCEx_PeriphCLKConfig(&pclk);
    }

    /* FDCAN1 configuration: classic CAN, 500 kbps @ 170 MHz PCLK1
     * Bit time = PSC * (1 + Seg1 + Seg2) = 17 * (1 + 15 + 4) = 340
     * Baudrate = 170,000,000 / 340 = 500,000 bps
     * SJW=4 to handle HSI ±1% tolerance between independent ECUs. */
    hfdcan1.Instance                  = FDCAN1;
    hfdcan1.Init.ClockDivider         = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat          = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode                 = mode;
    hfdcan1.Init.AutoRetransmission   = ENABLE;
    hfdcan1.Init.TransmitPause        = DISABLE;
    hfdcan1.Init.ProtocolException    = DISABLE;
    hfdcan1.Init.NominalPrescaler     = 17u;
    hfdcan1.Init.NominalSyncJumpWidth = 4u;
    hfdcan1.Init.NominalTimeSeg1      = 15u;
    hfdcan1.Init.NominalTimeSeg2      = 4u;
    hfdcan1.Init.DataPrescaler        = 1u;
    hfdcan1.Init.DataSyncJumpWidth    = 1u;
    hfdcan1.Init.DataTimeSeg1         = 1u;
    hfdcan1.Init.DataTimeSeg2         = 1u;
    hfdcan1.Init.StdFiltersNbr        = 4u;
    hfdcan1.Init.ExtFiltersNbr        = 0u;
    hfdcan1.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;

    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
    {
        return E_NOT_OK;
    }

    /* Re-apply filter after init (message RAM reset) */
    return Can_Hw_ConfigureFilter();
}

/* ==================================================================
 * Can_Hw_* API implementations
 * ================================================================== */

/**
 * @brief  Initialize FDCAN1 hardware at 500 kbps in normal mode
 * @param  baudrate  Baudrate in bps (currently only 500000 supported)
 * @return E_OK on success, E_NOT_OK on failure
 *
 * @note   HAL_FDCAN_Init() triggers MspInit (in hal_msp.c) which
 *         configures GPIO PA11/PA12 AF9 and enables FDCAN clock.
 */
Std_ReturnType Can_Hw_Init(uint32 baudrate)
{
    (void)baudrate; /* Timing hardcoded for 500 kbps @ 170 MHz */
    return Can_Hw_InitMode(FDCAN_MODE_NORMAL);
}

/**
 * @brief  Start FDCAN1 controller (enter normal/loopback mode)
 */
void Can_Hw_Start(void)
{
    (void)HAL_FDCAN_Start(&hfdcan1);
}

/**
 * @brief  Stop FDCAN1 controller (enter init mode)
 */
void Can_Hw_Stop(void)
{
    (void)HAL_FDCAN_Stop(&hfdcan1);
}

/**
 * @brief  Transmit a CAN frame via FDCAN1 TX FIFO
 * @param  id    CAN identifier (11-bit standard)
 * @param  data  Pointer to payload data
 * @param  dlc   Data length (0..8)
 * @return E_OK on success, E_NOT_OK if TX FIFO full or invalid DLC
 */
/** Maximum spin-wait iterations for TX FIFO drain (~300µs at 170 MHz).
 *  One CAN frame at 500 kbps takes ~220µs. This gives ~1.5 frames of
 *  drain time if the FIFO is full when we try to enqueue. */
#define CAN_HW_TX_RETRY_LIMIT  5000u

/* Debug: count actual HAL TX calls per CAN ID (0x200 = index for steering) */
volatile uint32 g_dbg_hw_tx_total = 0u;
volatile uint32 g_dbg_hw_tx_0x200 = 0u;

Std_ReturnType Can_Hw_Transmit(Can_IdType id, const uint8* data, uint8 dlc)
{
    FDCAN_TxHeaderTypeDef txHeader;
    uint8 txData[8];
    uint8 i;
    uint16 retry;

    g_dbg_hw_tx_total++;
    if (id == (Can_IdType)0x200u) { g_dbg_hw_tx_0x200++; }

    if (dlc > 8u)
    {
        return E_NOT_OK;
    }

    txHeader.Identifier          = (uint32)id;
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = dlc_to_hal[dlc];
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_OFF;
    txHeader.FDFormat            = FDCAN_CLASSIC_CAN;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0u;

    /* Copy data to local buffer (HAL expects non-const pointer) */
    for (i = 0u; i < dlc; i++)
    {
        txData[i] = data[i];
    }

    /* Try to enqueue; if FIFO full, spin-wait for a slot to free up.
     * At 500 kbps a frame drains in ~220µs, so 5000 iterations (~300µs
     * at 170 MHz) is enough for at least one slot to open. */
    for (retry = 0u; retry < CAN_HW_TX_RETRY_LIMIT; retry++)
    {
        if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, txData) == HAL_OK)
        {
            return E_OK;
        }
    }

    return E_NOT_OK;
}

/**
 * @brief  Non-blocking receive of a CAN frame from FDCAN1 RX FIFO0
 * @param  id    Output: received CAN identifier
 * @param  data  Output: received payload (min 8 bytes)
 * @param  dlc   Output: data length
 * @return TRUE if a frame was received, FALSE if FIFO empty
 */
boolean Can_Hw_Receive(Can_IdType* id, uint8* data, uint8* dlc)
{
    FDCAN_RxHeaderTypeDef rxHeader;
    uint32 dlcRaw;

    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) == 0u)
    {
        return FALSE;
    }

    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rxHeader, data) != HAL_OK)
    {
        return FALSE;
    }

    *id = (Can_IdType)rxHeader.Identifier;

    /* Convert HAL DLC constant back to byte count.
     * STM32G4 HAL: FDCAN_DLC_BYTES_0..8 = 0..8 (raw DLC code).
     * Classic CAN: DLC 0-8 maps 1:1 to byte count. */
    dlcRaw = rxHeader.DataLength;
    if (dlcRaw > 8u)
    {
        dlcRaw = 8u;
    }
    *dlc = (uint8)dlcRaw;

    return TRUE;
}

/**
 * @brief  Check if CAN bus is in bus-off state
 * @return TRUE if bus-off, FALSE otherwise
 * @note   Fail-closed: returns TRUE (bus-off) if status unreadable.
 */
boolean Can_Hw_IsBusOff(void)
{
    FDCAN_ProtocolStatusTypeDef psr;

    if (HAL_FDCAN_GetProtocolStatus(&hfdcan1, &psr) != HAL_OK)
    {
        return TRUE;  /* fail-closed: assume bus-off if unreadable */
    }

    return (psr.BusOff != 0u) ? TRUE : FALSE;
}

/**
 * @brief  Get FDCAN1 HAL state for debug diagnostics
 * @return HAL_FDCAN_StateTypeDef: 0=RESET, 1=READY, 2=LISTENING, 3=ERROR
 */
uint8 Can_Hw_GetHalState(void)
{
    return (uint8)HAL_FDCAN_GetState(&hfdcan1);
}

/**
 * @brief  Get CAN error counters (TEC and REC)
 * @param  tec  Output: transmit error counter
 * @param  rec  Output: receive error counter
 * @note   Fail-safe: returns 255/127 if counters unreadable.
 */
void Can_Hw_GetErrorCounters(uint8* tec, uint8* rec)
{
    FDCAN_ErrorCountersTypeDef counters;

    if (HAL_FDCAN_GetErrorCounters(&hfdcan1, &counters) != HAL_OK)
    {
        /* Fail-safe: max values to trigger error-passive detection */
        if (tec != NULL_PTR)
        {
            *tec = 255u;
        }
        if (rec != NULL_PTR)
        {
            *rec = 127u;
        }
        return;
    }

    if (tec != NULL_PTR)
    {
        *tec = (counters.TxErrorCnt > 255u) ? 255u : (uint8)counters.TxErrorCnt;
    }
    if (rec != NULL_PTR)
    {
        *rec = (counters.RxErrorCnt > 127u) ? 127u : (uint8)counters.RxErrorCnt;
    }
}

/* ==================================================================
 * CAN Loopback Self-Test
 * ================================================================== */

/** Loopback test frame parameters */
#define CAN_LB_TEST_ID     0x7FFu
#define CAN_LB_TEST_DLC    8u
#define CAN_LB_TIMEOUT_MS  100u

static const uint8 can_lb_test_data[8] = {
    0xCAu, 0xFEu, 0xBAu, 0xBEu, 0xDEu, 0xADu, 0x01u, 0x02u
};

/**
 * @brief  CAN internal loopback self-test
 * @return E_OK if loopback TX→RX verified, E_NOT_OK on failure
 *
 * @details Sequence:
 *   1. Stop FDCAN
 *   2. Reconfigure in INTERNAL_LOOPBACK mode
 *   3. Start, TX test frame (ID=0x7FF, 8 bytes)
 *   4. Poll RX FIFO0 with 100ms timeout
 *   5. Verify ID + data match
 *   6. Stop, reconfigure back to NORMAL mode
 *   7. Leave stopped (caller starts CAN later)
 *
 * @note   Called from Main_Hw_CanLoopbackTest() in each ECU's hw file.
 */
Std_ReturnType Can_Hw_LoopbackTest(void)
{
    Can_IdType rxId;
    uint8 rxData[8];
    uint8 rxDlc;
    uint32 start;
    uint8 i;

    /* Step 1-2: DeInit (reset HAL state machine) and switch to loopback */
    (void)HAL_FDCAN_Stop(&hfdcan1);
    (void)HAL_FDCAN_DeInit(&hfdcan1);

    if (Can_Hw_InitMode(FDCAN_MODE_INTERNAL_LOOPBACK) != E_OK)
    {
        (void)HAL_FDCAN_DeInit(&hfdcan1);
        (void)Can_Hw_InitMode(FDCAN_MODE_NORMAL);
        return E_NOT_OK;
    }

    /* Step 3: Start and transmit test frame */
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
    {
        (void)HAL_FDCAN_DeInit(&hfdcan1);
        (void)Can_Hw_InitMode(FDCAN_MODE_NORMAL);
        return E_NOT_OK;
    }

    if (Can_Hw_Transmit(CAN_LB_TEST_ID, can_lb_test_data, CAN_LB_TEST_DLC) != E_OK)
    {
        (void)HAL_FDCAN_Stop(&hfdcan1);
        (void)HAL_FDCAN_DeInit(&hfdcan1);
        (void)Can_Hw_InitMode(FDCAN_MODE_NORMAL);
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
                    (void)HAL_FDCAN_Stop(&hfdcan1);
                    (void)HAL_FDCAN_DeInit(&hfdcan1);
                    (void)Can_Hw_InitMode(FDCAN_MODE_NORMAL);
                    return E_OK;
                }
            }
        }
    }

    /* Timeout — restore normal mode */
    (void)HAL_FDCAN_Stop(&hfdcan1);
    (void)HAL_FDCAN_DeInit(&hfdcan1);
    (void)Can_Hw_InitMode(FDCAN_MODE_NORMAL);
    return E_NOT_OK;
}
