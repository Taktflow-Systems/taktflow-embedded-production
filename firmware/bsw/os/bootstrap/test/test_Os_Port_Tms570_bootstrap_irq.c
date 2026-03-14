/**
 * @file    test_Os_Port_Tms570_bootstrap_irq.c
 * @brief   IRQ test registration for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_RegisterIrqSaveTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoreTests(void);
void test_Os_Port_Tms570_RegisterIrqDispatchTests(void);
void test_Os_Port_Tms570_RegisterIrqSwitchTests(void);

void test_Os_Port_Tms570_RegisterIrqTests(void)
{
    test_Os_Port_Tms570_RegisterIrqSaveTests();
    test_Os_Port_Tms570_RegisterIrqRestoreTests();
    test_Os_Port_Tms570_RegisterIrqDispatchTests();
    test_Os_Port_Tms570_RegisterIrqSwitchTests();
}
