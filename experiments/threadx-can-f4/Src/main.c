/**
 * @file    main.c
 * @brief   ThreadX + bxCAN experiment for STM32F413ZH (Steps 1-6)
 * @date    2026-03-20
 *
 * Adapted from working G474RE ThreadX experiment.
 * Differences: bxCAN (not FDCAN), HSI 96MHz (not HSE 170MHz), RZC (not FZC).
 */

#include "stm32f4xx_hal.h"
#include "tx_api.h"
#include "Std_Types.h"
#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"
#include "E2E.h"
#include "Rte.h"
#include "Det.h"
#include "Rzc_Cfg.h"
#include "Swc_Heartbeat.h"
#include "Swc_Motor.h"
#include "Swc_Battery.h"
#include "Swc_CurrentMonitor.h"
#include "Swc_Encoder.h"
#include "Swc_TempMonitor.h"
#include "Swc_RzcCom.h"
#include "Swc_RzcSafety.h"
#include "Swc_RzcSensorFeeder.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * Hardware Config
 * ================================================================ */

/* CAN handled by BSW Can.c + Can_Hw_STM32F4.c */

/* UART bare-metal (USART3 PD8, 115200) */
static void Uart_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    (void)RCC->APB1ENR;

    GPIOD->MODER = (GPIOD->MODER & ~(3u << (8u * 2u))) | (2u << (8u * 2u));
    GPIOD->AFR[1] = (GPIOD->AFR[1] & ~(0xFu << (0u * 4u))) | (7u << (0u * 4u));

    USART3->BRR = 417u;  /* 48MHz / 115200 */
    USART3->CR1 = USART_CR1_TE | USART_CR1_UE;
    while ((USART3->SR & USART_SR_TC) == 0u) {}
}

static void Uart_Print(const char* str)
{
    while (*str)
    {
        while ((USART3->SR & USART_SR_TXE) == 0u) {}
        USART3->DR = (uint16_t)*str++;
    }
}

static void Uart_PrintU32(uint32_t val)
{
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    if (val == 0) { Uart_Print("0"); return; }
    while (val > 0 && i >= 0) { buf[i--] = '0' + (val % 10); val /= 10; }
    Uart_Print(&buf[i + 1]);
}

static void Uart_PrintHex32(uint32_t val)
{
    static const char h[] = "0123456789ABCDEF";
    char buf[9];
    int i;
    for (i = 7; i >= 0; i--) { buf[i] = h[val & 0xF]; val >>= 4; }
    buf[8] = '\0';
    Uart_Print("0x");
    Uart_Print(buf);
}

/* LED: PB0=LD1(green), PB7=LD2(blue), PB14=LD3(red) on NUCLEO-F413ZH */
static void Led_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    GPIOB->MODER = (GPIOB->MODER & ~(3u << (0u * 2u))) | (1u << (0u * 2u));
    GPIOB->MODER = (GPIOB->MODER & ~(3u << (7u * 2u))) | (1u << (7u * 2u));
    GPIOB->MODER = (GPIOB->MODER & ~(3u << (14u * 2u))) | (1u << (14u * 2u));
}

static void Led_Set(uint32_t pin, int on)
{
    if (on) GPIOB->BSRR = (1u << pin);
    else    GPIOB->BSRR = (1u << (pin + 16u));
}

/* System clock: HSI 16MHz -> PLL -> 96MHz */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 16u;
    osc.PLL.PLLN = 384u;
    osc.PLL.PLLP = RCC_PLLP_DIV4;
    osc.PLL.PLLQ = 8u;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { for(;;); }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK) { for(;;); }
}

/* ================================================================
 * Step 7a: BSW Can Layer (replaces raw HAL calls)
 * ================================================================ */

static const Can_ConfigType rzc_can_config = {
    .baudrate     = 500000u,
    .controllerId = 0u,
};

/* Generated BSW configs from firmware/ecu/rzc/cfg/ */
extern const CanIf_ConfigType rzc_canif_config;
extern const PduR_ConfigType  rzc_pdur_config;
extern const Com_ConfigType   rzc_com_config;
extern const Rte_ConfigType   rzc_rte_config;

/* Wrapper: BSW Can_Write uses Can_PduType, we simplify for experiment */
static int CAN_TX(uint32_t id, const uint8_t* data, uint8_t dlc)
{
    return (Can_Hw_Transmit((Can_IdType)id, data, dlc) == E_OK) ? 0 : -1;
}

/* ================================================================
 * ThreadX Resources
 * ================================================================ */

#define STACK_SIZE  2048u
#define POOL_SIZE   (STACK_SIZE * 4 + 4096)

static TX_THREAD   main_thread;
static TX_THREAD   can_rx_thread;
static TX_TIMER    bsw_10ms_timer;
static TX_TIMER    bsw_1ms_timer;
static TX_EVENT_FLAGS_GROUP can_event;
static TX_BYTE_POOL byte_pool;
static UCHAR       pool_mem[POOL_SIZE];

static volatile uint32_t can_rx_count = 0;
static volatile uint32_t bsw_tick = 0;

/* CAN RX: BSW uses polling (Can_Hw_Receive) from timer callback.
 * For Step 7a, poll from CAN RX thread instead of ISR. */

/* SINGLE 1ms timer (proven working on this board) */
static void bsw_timer_callback(ULONG param)
{
    (void)param;
    bsw_tick++;
    HAL_IncTick();

    /* CAN + Com every 10ms */
    if ((bsw_tick % 10u) == 0u)
    {
        Can_MainFunction_Read();
        Com_MainFunction_Tx();
        Com_MainFunction_Rx();
    }

    /* Rte every 1ms */
    Rte_MainFunction();
}

/* ================================================================
 * Threads
 * ================================================================ */

/* Main thread: init CAN, send periodic heartbeat */
static void main_thread_entry(ULONG param)
{
    (void)param;
    uint8_t counter = 0;
    uint8_t txData[8] = {0};

    Uart_Print("ThreadX running, starting BSW stack...\r\n");

    /* Step 7a: MCAL */
    Det_Init();
    Can_Init(&rzc_can_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);
    Uart_Print("  7a: Can OK\r\n");

    /* Step 7b: CanIf */
    CanIf_Init(&rzc_canif_config);
    Uart_Print("  7b: CanIf OK\r\n");

    /* Step 7c: PduR + Com + E2E */
    PduR_Init(&rzc_pdur_config);
    Com_Init(&rzc_com_config);
    E2E_Init();
    Uart_Print("  7c: PduR+Com+E2E OK\r\n");

    /* Step 7d: Rte */
    Rte_Init(&rzc_rte_config);
    Uart_Print("  7d: Rte OK\r\n");

    /* SWC init */
    Swc_Heartbeat_Init();
    Swc_Motor_Init();
    Swc_Battery_Init();
    Swc_CurrentMonitor_Init();
    Swc_Encoder_Init();
    Swc_TempMonitor_Init();
    Swc_RzcCom_Init();
    Swc_RzcSafety_Init();
    Swc_RzcSensorFeeder_Init();
    Uart_Print("  SWC init OK\r\n");

    tx_thread_sleep(100);
    Uart_Print("BSW+SWC init complete, ESR=");
    Uart_PrintHex32(CAN1->ESR);
    Uart_Print("\r\n");

    /* Wait for BSW to start processing */
    tx_thread_sleep(100);

    /* Debug: manually call Com_SendSignal to test TX path */
    {
        uint8_t val = 3u;
        Com_SendSignal(RZC_COM_SIG_RZC_HEARTBEAT_ECU_ID, &val);
        Uart_Print("Forced Com_SendSignal(ECU_ID=3)\r\n");
    }

    tx_thread_sleep(100); /* let Com_MainFunction_Tx process it */

    Uart_Print("BSW running. Monitoring CAN...\r\n");
    while (1)
    {
        counter++;
        if ((counter % 20) == 0)
        {
            uint32_t tsr = CAN1->TSR;
            uint8_t tme = (uint8_t)((tsr >> 26u) & 0x07u);

            Uart_Print("[");
            Uart_PrintU32(bsw_tick / 1000);
            Uart_Print("s] tick=");
            Uart_PrintU32(bsw_tick);
            Uart_Print(" rx=");
            Uart_PrintU32(can_rx_count);
            Uart_Print(" ESR=");
            Uart_PrintHex32(CAN1->ESR);
            Uart_Print(" TME=");
            Uart_PrintU32(tme);
            Uart_Print("\r\n");
        }

        Led_Set(0, (counter & 1));
        tx_thread_sleep(50);
    }
}

/* CAN RX thread: polls BSW Can_Hw_Receive every 1ms */
static void can_rx_thread_entry(ULONG param)
{
    (void)param;
    Can_IdType rxId;
    uint8_t rxD[8];
    uint8_t rxDlc;

    while (1)
    {
        while (Can_Hw_Receive(&rxId, rxD, &rxDlc) == TRUE)
        {
            can_rx_count++;
            if (can_rx_count <= 5)
            {
                Uart_Print("  RX: ID=");
                Uart_PrintHex32(rxId);
                Uart_Print(" DLC=");
                Uart_PrintU32(rxDlc);
                Uart_Print("\r\n");
            }
        }
        tx_thread_sleep(1); /* Poll every 1ms */
    }
}

/* ================================================================
 * ThreadX Entry
 * ================================================================ */

void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    CHAR *ptr;

    tx_byte_pool_create(&byte_pool, "pool", pool_mem, POOL_SIZE);

    /* Main thread */
    tx_byte_allocate(&byte_pool, (VOID**)&ptr, STACK_SIZE, TX_NO_WAIT);
    tx_thread_create(&main_thread, "Main", main_thread_entry, 0,
                     ptr, STACK_SIZE, 10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);

    /* CAN RX thread */
    tx_byte_allocate(&byte_pool, (VOID**)&ptr, STACK_SIZE, TX_NO_WAIT);
    tx_thread_create(&can_rx_thread, "CAN_RX", can_rx_thread_entry, 0,
                     ptr, STACK_SIZE, 8, 8, TX_NO_TIME_SLICE, TX_AUTO_START);

    /* Event flags */
    tx_event_flags_create(&can_event, "CAN_EVT");

    /* Single 1ms timer (two-timer crashes on F413 — investigate later) */
    tx_timer_create(&bsw_10ms_timer, "BSW_1ms", bsw_timer_callback, 0,
                    1, 1, TX_AUTO_ACTIVATE);
}

/* ================================================================
 * Main (pre-ThreadX)
 * ================================================================ */

/* _tx_initialize_low_level and SysTick_Handler provided by
 * Src/tx_initialize_low_level.s (ST ThreadX integration assembly).
 * PendSV_Handler provided by ThreadX port tx_thread_schedule.S.
 *
 * HAL_IncTick is driven by the BSW 1ms timer callback (thread context).
 */

void HardFault_Handler(void) { Uart_Print("!!! HARDFAULT !!!\r\n"); for(;;); }

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    Uart_Init();
    Led_Init();

    Uart_Print("\r\n=== F413ZH ThreadX + bxCAN (Steps 1-6) ===\r\n");
    Uart_Print("Clock: 96 MHz, ThreadX tick: 1000 Hz\r\n\r\n");

    tx_kernel_enter();  /* Never returns */
    return 0;
}
