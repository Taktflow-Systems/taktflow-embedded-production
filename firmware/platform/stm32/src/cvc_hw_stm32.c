/**
 * @file    cvc_hw_stm32.c
 * @brief   STM32 hardware backend for CVC (Central Vehicle Computer)
 * @date    2026-03-03
 *
 * @details Phase F2: PLL 170 MHz, bare-metal USART2 debug TX, SysTick via
 *          HAL, CAN loopback self-test wiring. All other self-test and
 *          peripheral stubs still return E_OK (real implementations F3-F4).
 *
 *          Clock: HSI 16 MHz → PLL (M/4 × N85 / R2) = 170 MHz
 *          UART:  Bare-metal USART2 PA2=TX AF7, 115200 baud @ 170 MHz
 *
 * @safety_req N/A — debug bring-up, not for production
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "stm32g4xx_hal.h"
#include "Cvc_Cfg.h"
#include "Can.h"
#include "Com.h"
#include "Rte.h"

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
 * @brief  Configure system clocks: HSI 16 MHz → PLL → 170 MHz
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

    /* HSI 16 MHz → PLL: PLLM/4 × PLLN=85 / PLLR/2 = 170 MHz */
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
     * BRR = 170000000 / 115200 = 1475.7 ≈ 1476 */
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
    Dbg_Uart_Print("\r\n=== CVC Boot (PLL 170 MHz) ===\r\n");
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
 * @brief  SPI loopback self-test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — perform real SPI loopback via HAL_SPI_TransmitReceive()
 */
Std_ReturnType Main_Hw_SpiLoopbackTest(void)
{
    /* TODO:HARDWARE — real SPI loopback test */
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
 * @brief  OLED I2C ACK self-test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — probe SSD1306 via HAL_I2C_IsDeviceReady()
 */
Std_ReturnType Main_Hw_OledAckTest(void)
{
    /* TODO:HARDWARE — real OLED ACK probe */
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
 * Periodic self-test stubs (SelfTest_Hw_* from Swc_SelfTest.c)
 * ================================================================== */

/**
 * @brief  Periodic SPI loopback self-test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — periodic SPI loopback via HAL
 */
Std_ReturnType SelfTest_Hw_SpiLoopback(void)
{
    /* TODO:HARDWARE — real periodic SPI loopback */
    return E_OK;
}

/**
 * @brief  Periodic CAN loopback self-test — stub returns E_OK
 * @return E_OK
 * @note   Too disruptive for periodic use (stops CAN bus).
 *         Phase F4 uses error counter monitoring instead.
 */
Std_ReturnType SelfTest_Hw_CanLoopback(void)
{
    /* Stub — periodic CAN loopback too disruptive. F4 uses error counters. */
    return E_OK;
}

/**
 * @brief  NVM integrity check — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — read NVM CRC and verify
 */
Std_ReturnType SelfTest_Hw_NvmCheck(void)
{
    /* TODO:HARDWARE — real NVM integrity check */
    return E_OK;
}

/**
 * @brief  Periodic OLED I2C ACK self-test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — periodic SSD1306 ACK probe
 */
Std_ReturnType SelfTest_Hw_OledAck(void)
{
    /* TODO:HARDWARE — real periodic OLED ACK */
    return E_OK;
}

/**
 * @brief  MPU region verify — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — verify MPU region configuration registers
 */
Std_ReturnType SelfTest_Hw_MpuVerify(void)
{
    /* TODO:HARDWARE — real MPU region verify */
    return E_OK;
}

/**
 * @brief  Stack canary check — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — compare canary word at stack boundary
 */
Std_ReturnType SelfTest_Hw_CanaryCheck(void)
{
    /* TODO:HARDWARE — real canary check */
    return E_OK;
}

/**
 * @brief  Periodic RAM pattern test — stub returns E_OK
 * @return E_OK
 * @note   TODO:HARDWARE — periodic march-C on reserved SRAM block
 */
Std_ReturnType SelfTest_Hw_RamPattern(void)
{
    /* TODO:HARDWARE — real periodic RAM pattern test */
    return E_OK;
}

/* ==================================================================
 * SSD1306 OLED I2C stub
 * ================================================================== */

/**
 * @brief  Write data to I2C bus — stub returns E_OK
 * @param  addr  7-bit I2C slave address
 * @param  data  Pointer to data buffer
 * @param  len   Number of bytes to write
 * @return E_OK always (stub — no hardware)
 * @note   TODO:HARDWARE — call HAL_I2C_Master_Transmit()
 */
Std_ReturnType Ssd1306_Hw_I2cWrite(uint8 addr, const uint8* data, uint8 len)
{
    (void)addr;
    (void)data;
    (void)len;
    /* TODO:HARDWARE — real I2C write via HAL_I2C_Master_Transmit() */
    return E_OK;
}

/* ==================================================================
 * CvcCom E-Stop Injection — no-op on target (GPIO reads real hardware)
 * ================================================================== */

/**
 * @brief  E-Stop injection — no-op on STM32 (real DIO hardware)
 * @param  Level  Unused on target
 */
void CvcCom_Hw_InjectEstop(uint8 Level)
{
    (void)Level;
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
    char buf[11]; /* max "4294967295\0" */
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
    uint32 fzc_comm = 0u;
    uint32 rzc_comm = 0u;
    uint32 veh_state = 0u;
    uint8  tec = 0u;
    uint8  rec = 0u;

    (void)Rte_Read(CVC_SIG_FZC_COMM_STATUS, &fzc_comm);
    (void)Rte_Read(CVC_SIG_RZC_COMM_STATUS, &rzc_comm);
    (void)Rte_Read(CVC_SIG_VEHICLE_STATE, &veh_state);
    (void)Can_GetErrorCounters(0u, &tec, &rec);

    Dbg_Uart_Print("[");
    Dbg_PrintU32(tick_us / 1000000u);
    Dbg_Uart_Print("s] CVC S=");
    Dbg_PrintU32(veh_state);
    Dbg_Uart_Print(" FZC=");
    Dbg_Uart_Print((fzc_comm == CVC_COMM_OK) ? "OK" : "TIMEOUT");
    Dbg_Uart_Print(" RZC=");
    Dbg_Uart_Print((rzc_comm == CVC_COMM_OK) ? "OK" : "TIMEOUT");
    Dbg_Uart_Print(" TEC=");
    Dbg_PrintU32((uint32)tec);
    Dbg_Uart_Print(" REC=");
    Dbg_PrintU32((uint32)rec);

    {
        uint8 fzc_alive = 0xFFu;
        uint8 rzc_alive = 0xFFu;
        (void)Com_ReceiveSignal(CVC_COM_SIG_FZC_HB_ALIVE, &fzc_alive);
        (void)Com_ReceiveSignal(CVC_COM_SIG_RZC_HB_ALIVE, &rzc_alive);
        Dbg_Uart_Print(" Fa=");
        Dbg_PrintU32((uint32)fzc_alive);
        Dbg_Uart_Print(" Ra=");
        Dbg_PrintU32((uint32)rzc_alive);
    }

    Dbg_Uart_Print(" rx=");
    Dbg_PrintU32(g_can_rx_count);
    Dbg_Uart_Print(" h11=");
    Dbg_PrintU32(g_can_rx_011_count);
    Dbg_Uart_Print(" h12=");
    Dbg_PrintU32(g_can_rx_012_count);
    Dbg_Uart_Print("\r\n");
}
