/**
 * @file    main.c
 * @brief   ThreadX on STM32L552ZE (Cortex-M33) — Step 1: LED blink + UART
 * @date    2026-03-26
 *
 * NUCLEO-L552ZE-Q: 110 MHz, FPv5-SP-D16, TrustZone capable.
 * Non-secure configuration (no TrustZone split for this experiment).
 *
 * Bare-metal register access — no HAL dependency.
 * LED: PC7 (LD1 green on NUCLEO-L552ZE-Q)
 * UART: LPUART1 PG7(TX)/PG8(RX) via ST-LINK VCP
 *
 * Prerequisites:
 *   1. Download STM32CubeL5 firmware package
 *   2. Set CMSIS_L5 path in Makefile to point to CMSIS device headers
 *   3. arm-none-eabi-gcc toolchain
 */

#include "tx_api.h"
#include <stdint.h>

/* ================================================================
 * L552ZE Register Definitions (minimal subset, no HAL)
 *
 * TODO:HARDWARE — Replace with proper CMSIS device header once
 * STM32CubeL5 package is downloaded:
 *   #include "stm32l552xx.h"
 * ================================================================ */

/* Base addresses */
#define PERIPH_BASE         0x40000000UL
#define AHB1_BASE           (PERIPH_BASE + 0x00020000UL)
#define AHB2_BASE           (PERIPH_BASE + 0x02020000UL)
#define APB1_BASE           (PERIPH_BASE + 0x00000000UL)
#define APB2_BASE           (PERIPH_BASE + 0x00012C00UL)

/* RCC */
#define RCC_BASE            (AHB1_BASE + 0x01000UL)
#define RCC_CR              (*(volatile uint32_t*)(RCC_BASE + 0x00u))
#define RCC_CFGR            (*(volatile uint32_t*)(RCC_BASE + 0x08u))
#define RCC_PLLCFGR         (*(volatile uint32_t*)(RCC_BASE + 0x0Cu))
#define RCC_AHB2ENR         (*(volatile uint32_t*)(RCC_BASE + 0x4Cu))
#define RCC_APB1ENR2        (*(volatile uint32_t*)(RCC_BASE + 0x5Cu))
#define RCC_APB2ENR         (*(volatile uint32_t*)(RCC_BASE + 0x60u))
#define RCC_CCIPR           (*(volatile uint32_t*)(RCC_BASE + 0x88u))

/* GPIO C (LED) */
#define GPIOC_BASE          (AHB2_BASE + 0x00800UL)
#define GPIOC_MODER         (*(volatile uint32_t*)(GPIOC_BASE + 0x00u))
#define GPIOC_ODR           (*(volatile uint32_t*)(GPIOC_BASE + 0x14u))
#define GPIOC_BSRR          (*(volatile uint32_t*)(GPIOC_BASE + 0x18u))

/* GPIO G (UART) */
#define GPIOG_BASE          (AHB2_BASE + 0x01800UL)
#define GPIOG_MODER         (*(volatile uint32_t*)(GPIOG_BASE + 0x00u))
#define GPIOG_AFRH          (*(volatile uint32_t*)(GPIOG_BASE + 0x24u))

/* LPUART1 */
#define LPUART1_BASE        (APB1_BASE + 0x8000UL)
#define LPUART1_CR1         (*(volatile uint32_t*)(LPUART1_BASE + 0x00u))
#define LPUART1_BRR         (*(volatile uint32_t*)(LPUART1_BASE + 0x0Cu))
#define LPUART1_ISR         (*(volatile uint32_t*)(LPUART1_BASE + 0x1Cu))
#define LPUART1_TDR         (*(volatile uint32_t*)(LPUART1_BASE + 0x28u))

/* PWR */
#define PWR_BASE            (APB1_BASE + 0x7000UL)
#define PWR_CR1             (*(volatile uint32_t*)(PWR_BASE + 0x00u))
#define PWR_SR2             (*(volatile uint32_t*)(PWR_BASE + 0x14u))
#define PWR_CR2             (*(volatile uint32_t*)(PWR_BASE + 0x04u))

/* FLASH */
#define FLASH_BASE_R        0x40022000UL
#define FLASH_ACR           (*(volatile uint32_t*)(FLASH_BASE_R + 0x00u))

/* SysTick */
#define SYST_CSR            (*(volatile uint32_t*)0xE000E010u)
#define SYST_RVR            (*(volatile uint32_t*)0xE000E014u)
#define SYST_CVR            (*(volatile uint32_t*)0xE000E018u)

/* NVIC */
#define SCB_VTOR            (*(volatile uint32_t*)0xE000ED08u)
#define SCB_AIRCR           (*(volatile uint32_t*)0xE000ED0Cu)

/* ================================================================
 * Clock Configuration: MSI 4MHz -> PLL -> 110MHz SYSCLK
 *
 * PLL: M=1, N=55, R=2 -> 4MHz * 55 / 2 = 110MHz
 * Flash: 5 wait states at 110MHz (range 0)
 * ================================================================ */

static void SystemClock_Config(void)
{
    /* Enable PWR clock, set voltage range 0 (boost) */
    RCC_APB1ENR2 |= (1u << 0u);  /* PWREN */
    PWR_CR1 = (PWR_CR1 & ~(3u << 9u));  /* VOS = Range 0 */
    while ((PWR_SR2 & (1u << 10u)) == 0u) {}  /* Wait VOSF */

    /* Flash: 5 wait states for 110 MHz */
    FLASH_ACR = (FLASH_ACR & ~0xFu) | 5u;
    while ((FLASH_ACR & 0xFu) != 5u) {}

    /* MSI is already on at 4 MHz (default after reset) */

    /* Configure PLL: source=MSI, M=1, N=55, R=2 */
    RCC_CR &= ~(1u << 24u);  /* PLL OFF */
    while ((RCC_CR & (1u << 25u)) != 0u) {}  /* Wait PLL unlocked */

    RCC_PLLCFGR = (0u << 27u)   /* PLLR = /2 (00) */
                | (55u << 8u)    /* PLLN = 55 */
                | (0u << 4u)     /* PLLM = /1 (0000) */
                | (1u << 0u)     /* PLLSRC = MSI */
                | (1u << 24u);   /* PLLREN = enable R output */

    RCC_CR |= (1u << 24u);  /* PLL ON */
    while ((RCC_CR & (1u << 25u)) == 0u) {}  /* Wait PLL locked */

    /* Switch SYSCLK to PLL */
    RCC_CFGR = (RCC_CFGR & ~3u) | 3u;  /* SW = PLL */
    while (((RCC_CFGR >> 2u) & 3u) != 3u) {}  /* Wait SWS = PLL */
}

/* ================================================================
 * LED: PC7 (LD1 green)
 * ================================================================ */

static void Led_Init(void)
{
    RCC_AHB2ENR |= (1u << 2u);  /* GPIOC EN */
    (void)RCC_AHB2ENR;  /* Delay after clock enable */
    GPIOC_MODER = (GPIOC_MODER & ~(3u << (7u * 2u))) | (1u << (7u * 2u));
}

static void Led_Toggle(void)
{
    GPIOC_ODR ^= (1u << 7u);
}

/* ================================================================
 * UART: LPUART1 PG7(TX) AF8, 115200 baud
 * ================================================================ */

static void Uart_Init(void)
{
    /* Enable GPIOG + LPUART1 clocks */
    /* GPIOG needs PWR IOSV bit to be set (VDDIO2 present on Nucleo) */
    PWR_CR2 |= (1u << 9u);  /* IOSV = 1 (VDDIO2 valid) */

    RCC_AHB2ENR |= (1u << 6u);  /* GPIOG EN */
    RCC_APB1ENR2 |= (1u << 0u);  /* PWREN — already enabled */
    (void)RCC_AHB2ENR;

    /* PG7 = AF8 (LPUART1_TX) */
    GPIOG_MODER = (GPIOG_MODER & ~(3u << (7u * 2u))) | (2u << (7u * 2u));
    GPIOG_AFRH  = (GPIOG_AFRH & ~(0xFu << ((7u - 8u) * 4u)));
    /* PG7 is in AFRL (bit 7), not AFRH. Fix: use AFRL */
    {
        volatile uint32_t* GPIOG_AFRL = (volatile uint32_t*)(GPIOG_BASE + 0x20u);
        *GPIOG_AFRL = (*GPIOG_AFRL & ~(0xFu << (7u * 4u))) | (8u << (7u * 4u));
    }

    /* Enable LPUART1 clock (APB1ENR2 bit 0 = LPUART1EN) */
    RCC_APB1ENR2 |= (1u << 0u);
    (void)RCC_APB1ENR2;

    /* LPUART1: PCLK1 = 110 MHz, baud = 115200
     * BRR = 256 * PCLK1 / baud = 256 * 110000000 / 115200 = 244444 */
    LPUART1_CR1 = 0u;  /* Disable */
    LPUART1_BRR = 244444u;
    LPUART1_CR1 = (1u << 3u) | (1u << 0u);  /* TE + UE */
}

static void Uart_Print(const char* str)
{
    while (*str)
    {
        while ((LPUART1_ISR & (1u << 7u)) == 0u) {}  /* Wait TXE */
        LPUART1_TDR = (uint8_t)*str++;
    }
}

static void Uart_PrintU32(uint32_t val)
{
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    if (val == 0u) { Uart_Print("0"); return; }
    while (val > 0u && i >= 0) { buf[i--] = '0' + (char)(val % 10u); val /= 10u; }
    Uart_Print(&buf[i + 1]);
}

/* ================================================================
 * ThreadX Resources
 * ================================================================ */

#define STACK_SIZE  2048u
#define POOL_SIZE   (STACK_SIZE * 3 + 4096)

static TX_THREAD    main_thread;
static TX_TIMER     led_timer;
static TX_BYTE_POOL byte_pool;
static UCHAR        pool_mem[POOL_SIZE];

static volatile uint32_t tick_count = 0u;

static void led_timer_callback(ULONG arg)
{
    (void)arg;
    tick_count++;
}

static void main_entry(ULONG param)
{
    (void)param;

    Uart_Print("ThreadX running on Cortex-M33!\r\n");
    Uart_Print("Clock: 110 MHz, tick: 1000 Hz\r\n\r\n");

    while (1)
    {
        Led_Toggle();

        Uart_Print("tick=");
        Uart_PrintU32(tx_time_get());
        Uart_Print("\r\n");

        tx_thread_sleep(500);  /* Toggle every 500ms */
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

    tx_byte_allocate(&byte_pool, (VOID**)&ptr, STACK_SIZE, TX_NO_WAIT);
    tx_thread_create(&main_thread, "Main", main_entry, 0,
                     ptr, STACK_SIZE, 10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_timer_create(&led_timer, "LED", led_timer_callback,
                    0, 1, 1, TX_AUTO_ACTIVATE);
}

/* ================================================================
 * Handlers + main
 * ================================================================ */

void HardFault_Handler(void)
{
    Uart_Print("!!! HARDFAULT !!!\r\n");
    for (;;) {}
}

void Error_Handler(void)
{
    for (;;) {}
}

int main(void)
{
    /* NVIC priority grouping: 4 bits preemption (GROUP_4) */
    SCB_AIRCR = (SCB_AIRCR & ~(7u << 8u)) | (0x05FAu << 16u) | (3u << 8u);

    SystemClock_Config();
    Led_Init();
    Uart_Init();

    Uart_Print("\r\n=== L552ZE ThreadX (Step 1: LED + UART) ===\r\n");

    tx_kernel_enter();  /* Never returns */
    return 0;
}
