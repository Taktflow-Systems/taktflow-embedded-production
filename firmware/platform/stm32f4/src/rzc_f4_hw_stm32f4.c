/**
 * @file    rzc_f4_hw_stm32f4.c
 * @brief   STM32F4 hardware backend for RZC_F4 (RZC on NUCLEO-F413ZH)
 * @date    2026-03-14
 *
 * @details Board bring-up: HSI 16 MHz -> PLL 96 MHz, bare-metal USART3 debug TX
 *          (PD8=TX, ST-LINK VCP), SysTick via HAL, CAN loopback self-test.
 *
 *          Clock: HSI 16 MHz -> PLL (M=16, N=384, P=4) = 96 MHz
 *          UART:  USART3 PD8=TX (AF7), 115200 baud @ 48 MHz APB1
 *          CAN:   CAN1 PD0=RX, PD1=TX (AF9), 500 kbps @ 48 MHz APB1
 *
 * @safety_req N/A — debug bring-up
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "stm32f4xx_hal.h"
#include "Rzc_Cfg.h"
#include "Can.h"
#include "Rte.h"

/* ==================================================================
 * Error Handler — required by CubeMX HAL MspInit
 * ================================================================== */

void Error_Handler(void)
{
    __disable_irq();
    for (;;)
    {
    }
}

/* ==================================================================
 * System Clock Configuration — HSE 8 MHz -> PLL 96 MHz
 * ================================================================== */

/**
 * @brief  Configure system clocks: HSI 16 MHz -> PLL -> 96 MHz
 * @note   Derived from CubeMX cuberzccf4fg project, switched to HSI.
 *         HSE bypass requires ST-LINK MCO which may not be available.
 *         APB1 = 48 MHz (div 2), APB2 = 96 MHz (div 1).
 *         Flash latency 3 WS for 96 MHz @ 3.3V.
 */
static void Ecu_SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSI 16 MHz -> PLL: M=16, N=384, P=4 -> 96 MHz */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState       = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM       = 16u;
    RCC_OscInitStruct.PLL.PLLN       = 384u;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ       = 8u;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* AHB = 96 MHz, APB1 = 48 MHz (div 2), APB2 = 96 MHz (div 1) */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ==================================================================
 * UART Debug Output — bare-metal USART3 (PD8=TX, Nucleo VCP)
 * ================================================================== */

/**
 * @brief  Initialize USART3 for debug TX at 115200 baud
 * @note   PD8 = USART3_TX (AF7), connected to ST-LINK VCP on NUCLEO-F413ZH.
 *         APB1 = 48 MHz: BRR = 48000000 / 115200 = 416.67 ~ 417
 */
static void Dbg_Uart_Init(void)
{
    /* Enable GPIOD and USART3 clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    (void)RCC->APB1ENR;  /* readback for clock stabilization */

    /* PD8 as alternate function (MODER bits [17:16] = 0b10) */
    GPIOD->MODER = (GPIOD->MODER & ~(3u << (8u * 2u)))
                 | (2u << (8u * 2u));

    /* PD8 alternate function = AF7 (USART3_TX) in AFRH [3:0] for pin 8 */
    GPIOD->AFR[1] = (GPIOD->AFR[1] & ~(0xFu << (0u * 4u)))
                   | (7u << (0u * 4u));

    /* USART3: 115200 baud at 48 MHz APB1, 8N1, TX only */
    USART3->BRR = 417u;
    USART3->CR1 = USART_CR1_TE | USART_CR1_UE;

    /* Wait for TE acknowledge — F4 uses SR register, not ISR */
    while ((USART3->SR & USART_SR_TC) == 0u)
    {
        /* spin */
    }
}

/**
 * @brief  Blocking single-character transmit on USART3
 */
static void Dbg_Uart_PutChar(char c)
{
    while ((USART3->SR & USART_SR_TXE) == 0u)
    {
    }
    USART3->DR = (uint8)c;
}

/**
 * @brief  Print null-terminated string to USART3
 */
void Dbg_Uart_Print(const char *str)
{
    while (*str != '\0')
    {
        Dbg_Uart_PutChar(*str);
        str++;
    }
}

/* ==================================================================
 * Debug diagnostics
 * ================================================================== */

uint8 Main_Hw_GetCanHalState(void)
{
    extern uint8 Can_Hw_GetHalState(void);
    return Can_Hw_GetHalState();
}

static void Dbg_PrintHex8(uint8 val)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[3];
    buf[0] = hex[(val >> 4u) & 0x0Fu];
    buf[1] = hex[val & 0x0Fu];
    buf[2] = '\0';
    Dbg_Uart_Print(buf);
}

/**
 * @brief  Dump CAN GPIO + bxCAN register state for bring-up debugging
 * @note   Checks PD0/PD1 MODER (should be AF), AFR[0] (should be AF9),
 *         CAN1->MSR (INAK, SLAK bits), and TX mailbox status.
 */
void Main_Hw_DumpCanDiag(void)
{
    uint32 moder = GPIOD->MODER;
    uint32 afrl  = GPIOD->AFR[0];
    uint32 msr   = CAN1->MSR;

    /* PD0 MODER bits [1:0], PD1 MODER bits [3:2] — expect 0b10 (AF) */
    uint8 pd0_mode = (uint8)((moder >> 0u) & 0x03u);
    uint8 pd1_mode = (uint8)((moder >> 2u) & 0x03u);

    /* AFRL: PD0 AF at bits [3:0], PD1 AF at bits [7:4] — expect 9 */
    uint8 pd0_af = (uint8)((afrl >> 0u) & 0x0Fu);
    uint8 pd1_af = (uint8)((afrl >> 4u) & 0x0Fu);

    /* MSR: INAK(0), SLAK(1) */
    uint8 msr_lo = (uint8)(msr & 0xFFu);

    /* TX mailbox empty flags */
    uint32 tsr = CAN1->TSR;
    uint8 tme = (uint8)((tsr >> 26u) & 0x07u);  /* TME2:TME0 */

    Dbg_Uart_Print("CAN-DIAG: PD0=M");
    Dbg_PrintHex8(pd0_mode);
    Dbg_Uart_Print("/AF");
    Dbg_PrintHex8(pd0_af);
    Dbg_Uart_Print(" PD1=M");
    Dbg_PrintHex8(pd1_mode);
    Dbg_Uart_Print("/AF");
    Dbg_PrintHex8(pd1_af);
    Dbg_Uart_Print(" MSR=");
    Dbg_PrintHex8(msr_lo);
    Dbg_Uart_Print(" TME=");
    Dbg_PrintHex8(tme);
    Dbg_Uart_Print("\r\n");
}

/* ==================================================================
 * Timing — SysTick via HAL
 * ================================================================== */

void Main_Hw_SystemClockInit(void)
{
    (void)HAL_Init();
    Ecu_SystemClock_Config();
    Dbg_Uart_Init();
    Dbg_Uart_Print("\r\n=== RZC_F4 Boot (HSE PLL 96 MHz) ===\r\n");
}

void Main_Hw_MpuConfig(void)
{
}

void Main_Hw_SysTickInit(uint32 periodUs)
{
    (void)periodUs;
}

void Main_Hw_Wfi(void)
{
    __WFI();
}

uint32 Main_Hw_GetTick(void)
{
    return HAL_GetTick() * 1000u;
}

/* ==================================================================
 * Self-test stubs
 * ================================================================== */

Std_ReturnType Main_Hw_Bts7960GpioTest(void)
{
    return E_OK;
}

Std_ReturnType Main_Hw_Acs723ZeroCalTest(void)
{
    return E_OK;
}

Std_ReturnType Main_Hw_NtcRangeTest(void)
{
    return E_OK;
}

Std_ReturnType Main_Hw_EncoderStuckTest(void)
{
    return E_OK;
}

Std_ReturnType Main_Hw_CanLoopbackTest(void)
{
    extern Std_ReturnType Can_Hw_LoopbackTest(void);
    return Can_Hw_LoopbackTest();
}

Std_ReturnType Main_Hw_MpuVerifyTest(void)
{
    return E_OK;
}

Std_ReturnType Main_Hw_RamPatternTest(void)
{
    return E_OK;
}

void Main_Hw_PlantStackCanary(void)
{
}

/* ==================================================================
 * 5s Periodic Debug Status
 * ================================================================== */

static void Dbg_PrintU32(uint32 val)
{
    char buf[11];
    char *p = &buf[10];
    *p = '\0';
    if (val == 0u)
    {
        p--;
        *p = '0';
    }
    else
    {
        while (val > 0u)
        {
            p--;
            *p = (char)('0' + (char)(val % 10u));
            val /= 10u;
        }
    }
    Dbg_Uart_Print(p);
}

void Main_Hw_DebugPrintStatus(uint32 tick_us)
{
    uint8 tec = 0u;
    uint8 rec = 0u;
    uint8 err_state = 0u;
    uint32 hb_alive = 0u;

    /* Toggle LD1 (PB0) as visual heartbeat */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    GPIOB->MODER = (GPIOB->MODER & ~(3u << (0u * 2u))) | (1u << (0u * 2u));
    GPIOB->ODR  ^= (1u << 0u);

    (void)Can_GetErrorCounters(0u, &tec, &rec);
    (void)Can_GetControllerErrorState(0u, &err_state);
    (void)Rte_Read(RZC_SIG_HEARTBEAT_ALIVE, &hb_alive);

    Dbg_Uart_Print("[");
    Dbg_PrintU32(tick_us / 1000000u);
    Dbg_Uart_Print("s] RZC_F4: TEC=");
    Dbg_PrintU32((uint32)tec);
    Dbg_Uart_Print(" REC=");
    Dbg_PrintU32((uint32)rec);
    Dbg_Uart_Print(" ERR=");
    Dbg_PrintU32((uint32)err_state);
    Dbg_Uart_Print(" HB=");
    Dbg_PrintU32(hb_alive);
    Dbg_Uart_Print(" HAL=");
    Dbg_PrintU32((uint32)Main_Hw_GetCanHalState());
    Dbg_Uart_Print(" TXbusy=");
    Dbg_PrintU32(g_can_tx_busy_count);
    Dbg_Uart_Print("\r\n");
}

void Main_Hw_CanTxDiagTest(void)
{
    uint8 test_data[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x03, 0x00, 0x00, 0x00};
    Can_PduType test_pdu;
    test_pdu.id     = 0x012u;
    test_pdu.length = 8u;
    test_pdu.sdu    = test_data;
    Can_ReturnType tx_result = Can_Write(0u, &test_pdu);
    Dbg_Uart_Print("CAN TX test: ");
    Dbg_Uart_Print((tx_result == CAN_OK) ? "OK\r\n" : "FAIL\r\n");
    Main_Hw_DumpCanDiag();
}
