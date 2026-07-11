/**
 * @file Os_Irq_Stm32F4.c
 * @brief OSEK-owned Cortex-M exception handlers for STM32F413ZH.
 */

#include "stm32f4xx_hal.h"
#include "Os.h"
#include "Os_Port_Stm32.h"
#include "Os_FaultRecord.h"

#if defined(PLATFORM_STM32) && defined(STM32F413xx) && defined(USE_OSEK)

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4                                \n"
        "ite eq                                    \n"
        "mrseq r0, msp                             \n"
        "mrsne r0, psp                             \n"
        "mov r1, lr                                \n"
        "b Os_FaultRecord_CaptureFromHandler       \n");
}

void SysTick_Handler(void)
{
    HAL_IncTick();
    Os_PortEnterIsr2();
    Os_Port_Stm32_TickIsr();
    Os_PortExitIsr2();
}

#endif
