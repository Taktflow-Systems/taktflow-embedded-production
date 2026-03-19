/**
 * @file    stm32g4xx_it_threadx.c
 * @brief   Interrupt handlers for ThreadX builds.
 *
 * PendSV_Handler and SysTick_Handler are provided by ThreadX port assembly.
 * This file provides the remaining handlers from stm32g4xx_it.c.
 */

#ifdef USE_THREADX

#include "stm32g4xx_hal.h"

void NMI_Handler(void)
{
    while (1) {}
}

void HardFault_Handler(void)
{
    while (1) {}
}

void MemManage_Handler(void)
{
    while (1) {}
}

void BusFault_Handler(void)
{
    while (1) {}
}

void UsageFault_Handler(void)
{
    while (1) {}
}

void DebugMon_Handler(void)
{
}

#endif /* USE_THREADX */
