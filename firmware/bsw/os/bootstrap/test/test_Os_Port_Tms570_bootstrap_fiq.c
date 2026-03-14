/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq.c
 * @brief   FIQ test registration for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_RegisterFiqLifecycleTests(void);
void test_Os_Port_Tms570_RegisterFiqSaveTests(void);
void test_Os_Port_Tms570_RegisterFiqNestedTests(void);
void test_Os_Port_Tms570_RegisterFiqProcessingTests(void);
void test_Os_Port_Tms570_RegisterFiqDispatchTests(void);
void test_Os_Port_Tms570_RegisterFiqRestoreTests(void);

void test_Os_Port_Tms570_RegisterFiqTests(void)
{
    test_Os_Port_Tms570_RegisterFiqLifecycleTests();
    test_Os_Port_Tms570_RegisterFiqSaveTests();
    test_Os_Port_Tms570_RegisterFiqNestedTests();
    test_Os_Port_Tms570_RegisterFiqProcessingTests();
    test_Os_Port_Tms570_RegisterFiqDispatchTests();
    test_Os_Port_Tms570_RegisterFiqRestoreTests();
}
