/**
 * @file    test_Os_Port_Tms570_bootstrap_integration.c
 * @brief   Integration test registration for the TMS570 bootstrap port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_RegisterIntegrationSchedulerTests(void);
void test_Os_Port_Tms570_RegisterIntegrationIsrTests(void);
void test_Os_Port_Tms570_RegisterIntegrationTickTests(void);
void test_Os_Port_Tms570_RegisterIntegrationTimeSliceTests(void);
void test_Os_Port_Tms570_RegisterIntegrationTimeSliceServiceTests(void);
void test_Os_Port_Tms570_RegisterIntegrationTimeSlicePolicyTests(void);

void test_Os_Port_Tms570_RegisterIntegrationTests(void)
{
    test_Os_Port_Tms570_RegisterIntegrationSchedulerTests();
    test_Os_Port_Tms570_RegisterIntegrationIsrTests();
    test_Os_Port_Tms570_RegisterIntegrationTickTests();
    test_Os_Port_Tms570_RegisterIntegrationTimeSliceTests();
    test_Os_Port_Tms570_RegisterIntegrationTimeSliceServiceTests();
    test_Os_Port_Tms570_RegisterIntegrationTimeSlicePolicyTests();
}
