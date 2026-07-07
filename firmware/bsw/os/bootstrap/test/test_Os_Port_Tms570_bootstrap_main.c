/**
 * @file    test_Os_Port_Tms570_bootstrap_main.c
 * @brief   Test runner for split TMS570 Cortex-R5 bootstrap OS port suites
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

int main(void)
{
    UNITY_BEGIN();

    test_Os_Port_Tms570_RegisterCoreTests();
    test_Os_Port_Tms570_RegisterIrqTests();
    test_Os_Port_Tms570_RegisterFiqTests();
    test_Os_Port_Tms570_RegisterIntegrationTests();

    return UNITY_END();
}
