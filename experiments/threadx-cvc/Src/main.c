/**
 * @file    main.c
 * @brief   ThreadX CVC on G474RE — Step 7: BSW comm stack
 * @date    2026-03-20
 *
 * Adapted from PROVEN FZC experiment (app_threadx.c).
 * Same timer structure, same init order, CVC configs.
 */

#include "stm32g4xx_hal.h"
#include "tx_api.h"

/* BSW */
#include "Std_Types.h"
#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"
#include "E2E.h"
#include "Rte.h"
#include "Det.h"
#include "Cvc_Cfg.h"

/* SWCs */
#include "Swc_Heartbeat.h"
#include "Swc_VehicleState.h"
#include "Swc_Pedal.h"
#include "Swc_EStop.h"
#include "Swc_Dashboard.h"
#include "Swc_CvcCom.h"
#include "Swc_CanMonitor.h"
#include "Swc_Watchdog.h"
#include "Swc_Nvm.h"

#include <string.h>

/* ================================================================
 * HAL handles + HW init (EXACT COPY from working FZC experiment)
 * ================================================================ */

FDCAN_HandleTypeDef hfdcan1;
UART_HandleTypeDef hlpuart1;

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = RCC_PLLM_DIV4;
    osc.PLL.PLLN = 85;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = RCC_PLLQ_DIV2;
    osc.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { for(;;); }
    clk.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) { for(;;); }
}

void MX_LPUART1_UART_Init(void)
{
    hlpuart1.Instance = LPUART1;
    hlpuart1.Init.BaudRate = 115200;
    hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
    hlpuart1.Init.StopBits = UART_STOPBITS_1;
    hlpuart1.Init.Parity = UART_PARITY_NONE;
    hlpuart1.Init.Mode = UART_MODE_TX_RX;
    hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_UART_Init(&hlpuart1);
}

/* FDCAN1 init — EXACT bit timing from working FZC: PSC=34, BS1=7, BS2=2 */
void MX_FDCAN1_Init(void)
{
    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_FDCAN_CONFIG(RCC_FDCANCLKSOURCE_PCLK1);
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(GPIOA, &gpio);

    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = ENABLE;
    hfdcan1.Init.TransmitPause = DISABLE;
    hfdcan1.Init.ProtocolException = DISABLE;
    hfdcan1.Init.NominalPrescaler = 34;
    hfdcan1.Init.NominalSyncJumpWidth = 1;
    hfdcan1.Init.NominalTimeSeg1 = 7;
    hfdcan1.Init.NominalTimeSeg2 = 2;
    hfdcan1.Init.DataPrescaler = 34;
    hfdcan1.Init.DataSyncJumpWidth = 1;
    hfdcan1.Init.DataTimeSeg1 = 7;
    hfdcan1.Init.DataTimeSeg2 = 2;
    hfdcan1.Init.StdFiltersNbr = 0;
    hfdcan1.Init.ExtFiltersNbr = 0;
    hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) { for(;;); }

    HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);
    HAL_FDCAN_Start(&hfdcan1);
}

/* ================================================================
 * UART print
 * ================================================================ */

void uart_puts(const char* str)
{
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)str, strlen(str), 100);
}

void uart_u32(uint32_t val)
{
    char buf[12]; int i = 10; buf[11] = '\0';
    if (val == 0) { uart_puts("0"); return; }
    while (val > 0 && i >= 0) { buf[i--] = '0' + (val % 10); val /= 10; }
    uart_puts(&buf[i + 1]);
}

void uart_hex(uint32_t val)
{
    static const char h[] = "0123456789ABCDEF";
    char buf[9]; int i;
    for (i = 7; i >= 0; i--) { buf[i] = h[val & 0xF]; val >>= 4; }
    buf[8] = '\0'; uart_puts("0x"); uart_puts(buf);
}

/* ================================================================
 * BSW config externs (from firmware/ecu/cvc/cfg/)
 * ================================================================ */

static const Can_ConfigType cvc_can_config = {
    .baudrate = 500000u,
    .controllerId = 0u,
};

extern const CanIf_ConfigType cvc_canif_config;
extern const PduR_ConfigType  cvc_pdur_config;
extern const Com_ConfigType   cvc_com_config;
extern const Rte_ConfigType   cvc_rte_config;

/* ================================================================
 * ThreadX resources
 * ================================================================ */

#define STACK_SIZE  2048u
#define POOL_SIZE   (STACK_SIZE * 3 + 4096)

static TX_THREAD    main_thread;
static TX_TIMER     bsw_10ms_timer;
static TX_TIMER     bsw_1ms_timer;
static TX_BYTE_POOL byte_pool;
static UCHAR        pool_mem[POOL_SIZE];

static volatile uint32_t can_tx_count = 0;

/* 10ms: CAN RX/TX + Com + bus-off recovery */
static void BSW_10ms_Callback(ULONG arg)
{
    (void)arg;
    can_tx_count++;

    /* Bus-off auto-recovery: re-init FDCAN if bus-off detected */
    if ((FDCAN1->PSR & 0x80u) != 0u) /* BO bit */
    {
        HAL_FDCAN_Stop(&hfdcan1);
        HAL_FDCAN_DeInit(&hfdcan1);
        MX_FDCAN1_Init();  /* re-inits + starts */
    }

    Can_MainFunction_Read();
    Com_MainFunction_Tx();
    Com_MainFunction_Rx();
}

/* 1ms: Rte scheduler (SAME as FZC experiment) */
static void BSW_1ms_Callback(ULONG arg)
{
    (void)arg;
    Rte_MainFunction();
}

/* ================================================================
 * Main thread
 * ================================================================ */

static void main_entry(ULONG param)
{
    (void)param;
    uint32_t counter = 0;

    uart_puts("CVC ThreadX: BSW init...\r\n");

    /* SAME init order as FZC experiment */
    Det_Init();
    Can_Init(&cvc_can_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);
    uart_puts("  Can OK\r\n");

    CanIf_Init(&cvc_canif_config);
    uart_puts("  CanIf OK\r\n");

    PduR_Init(&cvc_pdur_config);
    Com_Init(&cvc_com_config);
    E2E_Init();
    uart_puts("  PduR+Com+E2E OK\r\n");

    Rte_Init(&cvc_rte_config);
    uart_puts("  Rte OK\r\n");

    /* SWC init */
    Swc_Heartbeat_Init();
    Swc_VehicleState_Init();
    Swc_Pedal_Init(NULL_PTR);
    Swc_EStop_Init();
    Swc_Dashboard_Init();
    Swc_CvcCom_Init();
    Swc_CanMonitor_Init();
    Swc_Watchdog_Init(NULL_PTR);
    Swc_Nvm_Init();
    uart_puts("  SWC init OK\r\n");

    tx_thread_sleep(200);
    uart_puts("CVC running.\r\n");

    /* Monitor loop */
    while (1)
    {
        counter++;
        if ((counter % 20) == 0)
        {
            extern volatile uint32 g_can_rx_count;
            uint8 vstate = 0;
            Rte_Read(CVC_SIG_VEHICLE_STATE_MODE, &vstate);

            uart_puts("[");
            uart_u32(tx_time_get() / 1000);
            uart_puts("s] CVC S=");
            uart_u32(vstate);
            uart_puts(" tx10ms=");
            uart_u32(can_tx_count);
            uart_puts(" rx=");
            uart_u32(g_can_rx_count);
            uart_puts(" PSR=");
            uart_hex(FDCAN1->PSR);
            uart_puts("\r\n");
        }
        tx_thread_sleep(50);
    }
}

/* ================================================================
 * ThreadX setup
 * ================================================================ */

void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    CHAR *ptr;

    tx_byte_pool_create(&byte_pool, "pool", pool_mem, POOL_SIZE);

    tx_byte_allocate(&byte_pool, (VOID**)&ptr, STACK_SIZE, TX_NO_WAIT);
    tx_thread_create(&main_thread, "CVC", main_entry, 0,
                     ptr, STACK_SIZE, 10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);

    /* SAME two-timer pattern as FZC experiment */
    tx_timer_create(&bsw_10ms_timer, "BSW_10MS", BSW_10ms_Callback,
                    0, 10, 10, TX_AUTO_ACTIVATE);
    tx_timer_create(&bsw_1ms_timer, "BSW_1MS", BSW_1ms_Callback,
                    0, 1, 1, TX_AUTO_ACTIVATE);
}

/* ================================================================
 * Handlers + main
 * ================================================================ */

void HardFault_Handler(void) { uart_puts("!!! HARDFAULT !!!\r\n"); for(;;); }
void Error_Handler(void) { for(;;); }

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_LPUART1_UART_Init();

    uart_puts("\r\n=== G474RE ThreadX CVC (Step 7) ===\r\n");

    /* FDCAN init BEFORE ThreadX kernel — same as FZC */
    MX_FDCAN1_Init();
    uart_puts("FDCAN OK\r\n");

    tx_kernel_enter();
    return 0;
}
