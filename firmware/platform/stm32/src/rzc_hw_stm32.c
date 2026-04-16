/**
 * @file    rzc_hw_stm32.c
 * @brief   STM32 hardware backend for RZC (Rear Zone Controller)
 * @date    2026-03-04
 *
 * @details Phase F2 bring-up: PLL 170 MHz, bare-metal USART2 debug TX,
 *          SysTick via HAL, CAN loopback self-test wiring. Sensor and
 *          actuator stubs still return E_OK (real implementations come
 *          in F3-F4).
 *
 *          Clock: HSI 16 MHz -> PLL (M/4 x N85 / R2) = 170 MHz
 *          UART:  Bare-metal USART2 PA2=TX AF7, 115200 baud @ 170 MHz
 *
 * @safety_req N/A — debug bring-up, not for production
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "stm32g4xx_hal.h"
#include "Rzc_Cfg.h"
#include "Can.h"
#include "Rte.h"

#define RZC_CAN_PHYSICAL_PROBE_ID         0x612u
#define RZC_CAN_PHYSICAL_PROBE_PERIOD_US  1000000u
#define RZC_CAN_PHYSICAL_PROBE_SETTLE_MS  2u

volatile uint32 g_can_hw_probe_count = 0u;
volatile uint32 g_can_hw_probe_ok_count = 0u;
volatile uint32 g_can_hw_probe_last_ret = 0u;
volatile uint32 g_can_hw_probe_last_tick_us = 0u;
volatile uint32 g_can_hw_probe_last_seq = 0u;
volatile uint32 g_can_hw_probe_last_txfqs = 0u;
volatile uint32 g_can_hw_probe_last_psr = 0u;
volatile uint32 g_can_hw_probe_last_ecr = 0u;

/* ==================================================================
 * Error Handler — required by CubeMX HAL_FDCAN_MspInit()
 * ================================================================== */

/**
 * @brief  Error handler — infinite loop, watchdog will reset
 * @note   Called by HAL init functions on failure. Declared in
 *         firmware/cvc/cfg/Core/Inc/main.h, defined here (CubeMX main.c
 *         is NOT in the build).
 */
void Error_Handler(void)
{
    __disable_irq();
    for (;;)
    {
        /* Watchdog will reset. TODO:HARDWARE — toggle LED for visual indicator */
    }
}

/* ==================================================================
 * System Clock Configuration — PLL 170 MHz
 * ================================================================== */

/**
 * @brief  Configure system clocks: HSI 16 MHz -> PLL -> 170 MHz
 * @note   Derived from CubeMX firmware/cvc/cfg/Core/Src/main.c:136-176.
 *         Voltage Scale 1 Boost, Flash latency 4 WS,
 *         APB1/APB2 divider = 1 (both 170 MHz).
 */
static void Ecu_SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Voltage scaling for 170 MHz operation */
    (void)HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    /* HSI 16 MHz -> PLL: PLLM/4 x PLLN=85 / PLLR/2 = 170 MHz */
    RCC_OscInitStruct.OscillatorType       = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState             = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue  = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState         = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource        = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM             = RCC_PLLM_DIV4;
    RCC_OscInitStruct.PLL.PLLN             = 85u;
    RCC_OscInitStruct.PLL.PLLP             = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ             = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR             = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* AHB/APB1/APB2 all at 170 MHz (divider = 1) */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ==================================================================
 * UART Debug Output — bare-metal USART2 (PA2=TX, Nucleo VCP)
 * ================================================================== */

/**
 * @brief  Initialize USART2 for debug TX at 115200 baud (PLL 170 MHz)
 * @note   Bare-metal register access — no HAL UART dependency.
 *         PA2 = USART2_TX (AF7), connected to ST-LINK VCP on Nucleo.
 */
static void Dbg_Uart_Init(void)
{
    /* Enable GPIOA and USART2 peripheral clocks */
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;

    /* Read-back for clock stabilization (silicon errata workaround) */
    (void)RCC->APB1ENR1;

    /* Configure PA2 as alternate function (MODER=0b10) */
    GPIOA->MODER  = (GPIOA->MODER  & ~(3u << (2u * 2u)))
                   | (2u << (2u * 2u));

    /* PA2 alternate function = AF7 (USART2_TX) in AFRL register */
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFu << (2u * 4u)))
                   | (7u << (2u * 4u));

    /* USART2: 115200 baud at 170 MHz APB1, 8N1, TX only
     * BRR = 170000000 / 115200 = 1475.7 ~ 1476 */
    USART2->BRR = 1476u;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;     /* Enable transmitter + USART */

    /* Wait for TE acknowledge before first transmission */
    while ((USART2->ISR & USART_ISR_TEACK) == 0u)
    {
        /* spin */
    }
}

/**
 * @brief  Blocking single-character transmit on USART2
 * @param  c  Character to send
 */
static void Dbg_Uart_PutChar(char c)
{
    while ((USART2->ISR & USART_ISR_TXE_TXFNF) == 0u)
    {
        /* Wait for TX data register empty */
    }
    USART2->TDR = (uint8)c;
}

/**
 * @brief  Print null-terminated string to USART2 (blocking)
 * @param  str  Null-terminated string to transmit
 * @note   Non-static — called from main.c via extern declaration.
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
 * Debug: FDCAN HAL state query (for bring-up diagnostics)
 * ================================================================== */

/**
 * @brief  Get FDCAN1 HAL state for debug output
 * @return HAL state: 0=RESET, 1=READY, 2=LISTENING, 3=ERROR
 */
uint8 Main_Hw_GetCanHalState(void)
{
    return Can_Hw_GetHalState();
}

/**
 * @brief  Print hex byte for diagnostics
 * @param  val  Value to print as 2-digit hex
 */
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
 * @brief  Dump CAN GPIO + FDCAN register state for bring-up debugging
 * @note   Checks PA11/PA12 MODER (should be 0b10=AF), AFR[1] (should be AF9),
 *         FDCAN1->CCCR (INIT, CCE, MON, TEST bits), and TX FIFO status.
 */
void Main_Hw_DumpCanDiag(void)
{
    uint32 moder  = GPIOA->MODER;
    uint32 afrh   = GPIOA->AFR[1];   /* AFRH covers PA8..PA15 */
    uint32 cccr   = FDCAN1->CCCR;

    /* PA11 MODER bits [23:22], PA12 MODER bits [25:24] — expect 0b10 (AF) */
    uint8 pa11_mode = (uint8)((moder >> 22u) & 0x03u);
    uint8 pa12_mode = (uint8)((moder >> 24u) & 0x03u);

    /* AFRH: PA11 AF at bits [15:12], PA12 AF at bits [19:16] — expect 9 */
    uint8 pa11_af = (uint8)((afrh >> 12u) & 0x0Fu);
    uint8 pa12_af = (uint8)((afrh >> 16u) & 0x0Fu);

    /* CCCR bits: INIT(0), CCE(1), ASM(2), CSA(3), CSR(4), MON(5), DAR(6), TEST(7) */
    uint8 cccr_lo = (uint8)(cccr & 0xFFu);

    /* TXFQS: TX FIFO/Queue Status — TFFL field [bits 5:0] = free level */
    uint32 txfqs = FDCAN1->TXFQS;
    uint8 tx_free = (uint8)(txfqs & 0x07u);  /* 3 TX buffers on G4 */
    uint8 tx_full = (uint8)((txfqs >> 21u) & 0x01u);  /* TFQF bit */

    Dbg_Uart_Print("CAN-DIAG: PA11=M");
    Dbg_PrintHex8(pa11_mode);
    Dbg_Uart_Print("/AF");
    Dbg_PrintHex8(pa11_af);
    Dbg_Uart_Print(" PA12=M");
    Dbg_PrintHex8(pa12_mode);
    Dbg_Uart_Print("/AF");
    Dbg_PrintHex8(pa12_af);
    Dbg_Uart_Print(" CCCR=");
    Dbg_PrintHex8(cccr_lo);
    Dbg_Uart_Print(" TXfree=");
    Dbg_PrintHex8(tx_free);
    Dbg_Uart_Print(" TXfull=");
    Dbg_PrintHex8(tx_full);
    Dbg_Uart_Print("\r\n");
}

/* ==================================================================
 * Timing — SysTick via HAL (ISR in CubeMX stm32g4xx_it.c)
 * ================================================================== */

/**
 * @brief  Initialize system clocks, SysTick, and debug UART
 * @note   HAL_Init() configures SysTick at 1ms (initially at HSI 16 MHz).
 *         Ecu_SystemClock_Config() switches to PLL 170 MHz, which
 *         internally reconfigures SysTick for the new frequency.
 *         UART init + boot banner printed at earliest opportunity.
 */
void Main_Hw_SystemClockInit(void)
{
    /* HAL_Init: flash prefetch, SysTick 1ms, NVIC priority grouping */
    (void)HAL_Init();

    /* Switch to PLL 170 MHz — SysTick auto-reconfigured */
    Ecu_SystemClock_Config();

    /* Initialize debug UART at 170 MHz clock rate */
    Dbg_Uart_Init();
    Dbg_Uart_Print("\r\n=== RZC Boot (PLL 170 MHz) [" GIT_HASH "] ===\r\n");
}

/**
 * @brief  Configure MPU regions — no-op stub
 * @note   TODO:HARDWARE — configure MPU via HAL_MPU_ConfigRegion()
 */
void Main_Hw_MpuConfig(void)
{
    /* TODO:HARDWARE — configure MPU via HAL_MPU_ConfigRegion() */
}

/**
 * @brief  Initialize SysTick timer
 * @param  periodUs  Tick period in microseconds (expected: 1000 = 1ms)
 * @note   SysTick already configured by HAL_Init() and reconfigured by
 *         HAL_RCC_ClockConfig() for 170 MHz. This call serves as a
 *         synchronization point before entering the main loop.
 */
void Main_Hw_SysTickInit(uint32 periodUs)
{
    (void)periodUs;
    /* SysTick running at 1ms from HAL_Init() + PLL 170 MHz.
     * HAL_IncTick() ISR in stm32g4xx_it.c increments the HAL tick. */
}

/**
 * @brief  Wait for interrupt — saves power between ticks
 */
void Main_Hw_Wfi(void)
{
    __WFI();
}

/**
 * @brief  Get elapsed time since boot in microseconds
 * @return Elapsed microseconds (HAL tick * 1000)
 * @note   Resolution is 1ms (1000us steps). Overflows after ~49 days.
 */
uint32 Main_Hw_GetTick(void)
{
    return HAL_GetTick() * 1000u;
}

/* ==================================================================
 * Startup self-test stubs (Main_Hw_* from main.c)
 * ================================================================== */

/**
 * @brief  BTS7960 motor driver GPIO self-test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — toggle BTS7960 INH/IS pins and read back
 */
Std_ReturnType Main_Hw_Bts7960GpioTest(void)
{
    /* TODO:HARDWARE — real BTS7960 GPIO test */
    return E_OK;
}

/**
 * @brief  ACS723 current sensor zero calibration self-test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — read ADC at zero current and verify offset
 */
Std_ReturnType Main_Hw_Acs723ZeroCalTest(void)
{
    /* TODO:HARDWARE — real ACS723 zero-cal test */
    return E_OK;
}

/**
 * @brief  NTC thermistor range self-test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — read NTC ADC and verify within expected range
 */
Std_ReturnType Main_Hw_NtcRangeTest(void)
{
    /* TODO:HARDWARE — real NTC range test */
    return E_OK;
}

/**
 * @brief  Encoder stuck self-test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — check encoder pulse count after brief motor drive
 */
Std_ReturnType Main_Hw_EncoderStuckTest(void)
{
    /* TODO:HARDWARE — real encoder stuck test */
    return E_OK;
}

/**
 * @brief  CAN loopback self-test — delegates to Can_Hw_LoopbackTest()
 * @return E_OK if loopback verified, E_NOT_OK on failure
 */
Std_ReturnType Main_Hw_CanLoopbackTest(void)
{
    return Can_Hw_LoopbackTest();
}

/**
 * @brief  MPU verify self-test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — verify MPU region configuration registers
 */
Std_ReturnType Main_Hw_MpuVerifyTest(void)
{
    /* TODO:HARDWARE — real MPU verify test */
    return E_OK;
}

/**
 * @brief  RAM pattern self-test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — write/read march-C pattern to SRAM
 */
Std_ReturnType Main_Hw_RamPatternTest(void)
{
    /* TODO:HARDWARE — real RAM pattern test */
    return E_OK;
}

/**
 * @brief  Plant stack canary — no-op stub
 * @note   TODO:HARDWARE — write canary word at end of stack region
 */
void Main_Hw_PlantStackCanary(void)
{
    /* TODO:HARDWARE — plant canary at stack boundary */
}

/* ==================================================================
 * 5s Periodic Debug Status (moved from main.c — STM32 UART only)
 * ================================================================== */

/**
 * @brief  Print decimal uint32 to debug UART
 * @param  val  Value to print
 */
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

/**
 * @brief  5s periodic debug status print to UART
 * @param  tick_us  Current tick in microseconds
 */
void Main_Hw_DebugPrintStatus(uint32 tick_us)
{
    uint8 tec = 0u;
    uint8 rec = 0u;
    uint8 err_state = 0u;
    uint32 hb_alive = 0u;

    (void)Can_GetErrorCounters(0u, &tec, &rec);
    (void)Can_GetControllerErrorState(0u, &err_state);
    (void)Rte_Read(RZC_SIG_HEARTBEAT_ALIVE, &hb_alive);

    Dbg_Uart_Print("[");
    Dbg_PrintU32(tick_us / 1000000u);
    Dbg_Uart_Print("s] RZC: TEC=");
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
    Dbg_Uart_Print(" HWprobe=");
    Dbg_PrintU32(g_can_hw_probe_ok_count);
    Dbg_Uart_Print("/");
    Dbg_PrintU32(g_can_hw_probe_count);
    Dbg_Uart_Print(" ProbeRet=");
    Dbg_PrintU32(g_can_hw_probe_last_ret);
    Dbg_Uart_Print("\r\n");
}

/**
 * @brief  Init-time CAN TX diagnostic test — sends test frame and dumps diag
 */
/**
 * @brief  Send one direct FDCAN probe frame and snapshot controller state
 * @param  tick_us     Current system tick in microseconds
 * @param  wait_settle TRUE to wait briefly so TX FIFO drain can be observed
 * @return E_OK if the frame was enqueued, E_NOT_OK if the TX path rejected it
 */
static Std_ReturnType Main_Hw_SendCanPhysicalProbe(uint32 tick_us, boolean wait_settle)
{
    static uint8 probe_seq = 0u;
    uint8 tec = 0u;
    uint8 rec = 0u;
    uint8 probe_data[8];
    Std_ReturnType tx_result;

    (void)Can_GetErrorCounters(0u, &tec, &rec);

    probe_data[0] = (uint8)'R';
    probe_data[1] = (uint8)'Z';
    probe_data[2] = (uint8)'C';
    probe_data[3] = (uint8)0xA5u;
    probe_data[4] = probe_seq;
    probe_data[5] = Main_Hw_GetCanHalState();
    probe_data[6] = tec;
    probe_data[7] = rec;

    tx_result = Can_Hw_Transmit((Can_IdType)RZC_CAN_PHYSICAL_PROBE_ID, probe_data, 8u);

    g_can_hw_probe_count++;
    g_can_hw_probe_last_ret = (uint32)tx_result;
    g_can_hw_probe_last_tick_us = tick_us;
    g_can_hw_probe_last_seq = probe_seq;

    if (wait_settle == TRUE)
    {
        uint32 start_ms = HAL_GetTick();
        while ((HAL_GetTick() - start_ms) < RZC_CAN_PHYSICAL_PROBE_SETTLE_MS)
        {
            /* Allow the controller to attempt transmission before snapshot. */
        }
    }

    g_can_hw_probe_last_txfqs = FDCAN1->TXFQS;
    g_can_hw_probe_last_psr = FDCAN1->PSR;
    g_can_hw_probe_last_ecr = FDCAN1->ECR;

    if (tx_result == E_OK)
    {
        g_can_hw_probe_ok_count++;
    }

    probe_seq++;
    return tx_result;
}

/**
 * @brief  Init-time CAN TX diagnostic test â€” bypasses Can_Write()
 */
void Main_Hw_CanTxDiagTest(void)
{
    Std_ReturnType tx_result = Main_Hw_SendCanPhysicalProbe(Main_Hw_GetTick(), TRUE);
    Dbg_Uart_Print("CAN HW probe: ");
    Dbg_Uart_Print((tx_result == E_OK) ? "OK\r\n" : "FAIL\r\n");
    Main_Hw_DumpCanDiag();
}

/**
 * @brief  Periodic direct FDCAN probe for HIL physical-path triage
 * @param  tick_us  Current system tick in microseconds
 */
void Main_Hw_RunCanPhysicalProbe(uint32 tick_us)
{
#ifdef PLATFORM_HIL
    static uint32 last_probe_us = 0u;

    if ((tick_us - last_probe_us) < RZC_CAN_PHYSICAL_PROBE_PERIOD_US)
    {
        return;
    }

    last_probe_us = tick_us;
    (void)Main_Hw_SendCanPhysicalProbe(tick_us, FALSE);
#else
    (void)tick_us;
#endif
}
