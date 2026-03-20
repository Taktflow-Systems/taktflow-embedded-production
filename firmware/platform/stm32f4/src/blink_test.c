/**
 * @file    blink_test.c
 * @brief   Minimal blink test for NUCLEO-F413ZH — HSI only, no PLL, no HAL
 *
 * Toggles LD1 (PB0), LD2 (PB7), LD3 (PB14) using direct register access.
 * Uses HSI (16 MHz, default after reset) — no external clock dependency.
 * Standalone: link with startup_stm32f413xx.s and linker script only.
 */

#include <stdint.h>

/* STM32F413 register addresses */
#define RCC_BASE        0x40023800u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30u))

#define GPIOB_BASE      0x40020400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14u))

/* LED pins on Port B */
#define LD1_PIN  0u   /* PB0  — green */
#define LD2_PIN  7u   /* PB7  — green */
#define LD3_PIN  14u  /* PB14 — red   */

static void delay(volatile uint32_t count)
{
    while (count > 0u) { count--; }
}

int main(void)
{
    /* Enable GPIOB clock */
    RCC_AHB1ENR |= (1u << 1u);

    /* Brief delay for clock to stabilize */
    delay(10u);

    /* Configure PB0, PB7, PB14 as output (MODER = 01 for each) */
    GPIOB_MODER &= ~((3u << (LD1_PIN * 2u)) |
                      (3u << (LD2_PIN * 2u)) |
                      (3u << (LD3_PIN * 2u)));
    GPIOB_MODER |=  ((1u << (LD1_PIN * 2u)) |
                      (1u << (LD2_PIN * 2u)) |
                      (1u << (LD3_PIN * 2u)));

    while (1)
    {
        /* All LEDs on */
        GPIOB_ODR |= (1u << LD1_PIN) | (1u << LD2_PIN) | (1u << LD3_PIN);
        delay(800000u);   /* ~200ms at 16 MHz HSI */

        /* All LEDs off */
        GPIOB_ODR &= ~((1u << LD1_PIN) | (1u << LD2_PIN) | (1u << LD3_PIN));
        delay(800000u);
    }
}

/* Stubs required by startup .s */
void NMI_Handler(void)        { while(1); }
void HardFault_Handler(void)  { while(1); }
void MemManage_Handler(void)  { while(1); }
void BusFault_Handler(void)   { while(1); }
void UsageFault_Handler(void) { while(1); }
void SVC_Handler(void)        { while(1); }
void DebugMon_Handler(void)   { while(1); }
void PendSV_Handler(void)     { while(1); }
void SysTick_Handler(void)    {}
void SystemInit(void)         {} /* startup.s calls this before main */
