/**
 * @file    test_Os_Port_Tms570_bootstrap_core.c
 * @brief   Core test registration for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_RegisterCoreInitTests(void);
void test_Os_Port_Tms570_RegisterCoreTargetHwTests(void);
void test_Os_Port_Tms570_RegisterCoreRtiServiceCoreTests(void);
void test_Os_Port_Tms570_RegisterCoreRtiNotificationTests(void);
void test_Os_Port_Tms570_RegisterCoreRtiCounterTests(void);
void test_Os_Port_Tms570_RegisterCoreRtiCounterProgressTests(void);
void test_Os_Port_Tms570_RegisterCoreRtiPendingTests(void);
void test_Os_Port_Tms570_RegisterCoreRtiRequestTests(void);
void test_Os_Port_Tms570_RegisterCoreVimTests(void);
void test_Os_Port_Tms570_RegisterCoreVimSyncTests(void);
void test_Os_Port_Tms570_RegisterCoreVimSelectTests(void);
void test_Os_Port_Tms570_RegisterCoreVimMapTests(void);
void test_Os_Port_Tms570_RegisterCoreVimReadChannelTests(void);
void test_Os_Port_Tms570_RegisterCoreVimReadVectorTests(void);
void test_Os_Port_Tms570_RegisterCoreVimPulseMaskTests(void);
void test_Os_Port_Tms570_RegisterCoreVimServiceChannelTests(void);
void test_Os_Port_Tms570_RegisterCoreVimInvokeVectorTests(void);
void test_Os_Port_Tms570_RegisterCoreVimEntryCoreTests(void);
void test_Os_Port_Tms570_RegisterCoreVimEntryTests(void);
void test_Os_Port_Tms570_RegisterCoreVimServiceCoreTests(void);
void test_Os_Port_Tms570_RegisterCoreVimServiceTests(void);
void test_Os_Port_Tms570_RegisterCoreVimDispatchTests(void);
void test_Os_Port_Tms570_RegisterCoreVimDispatchSelectedTests(void);
void test_Os_Port_Tms570_RegisterCoreDispatchTests(void);
void test_Os_Port_Tms570_RegisterCoreFirstTaskViewTests(void);
void test_Os_Port_Tms570_RegisterCoreHalBridgeTests(void);

void test_Os_Port_Tms570_RegisterCoreTests(void)
{
    test_Os_Port_Tms570_RegisterCoreInitTests();
    test_Os_Port_Tms570_RegisterCoreTargetHwTests();
    test_Os_Port_Tms570_RegisterCoreRtiServiceCoreTests();
    test_Os_Port_Tms570_RegisterCoreRtiNotificationTests();
    test_Os_Port_Tms570_RegisterCoreRtiCounterTests();
    test_Os_Port_Tms570_RegisterCoreRtiCounterProgressTests();
    test_Os_Port_Tms570_RegisterCoreRtiPendingTests();
    test_Os_Port_Tms570_RegisterCoreRtiRequestTests();
    test_Os_Port_Tms570_RegisterCoreVimTests();
    test_Os_Port_Tms570_RegisterCoreVimSyncTests();
    test_Os_Port_Tms570_RegisterCoreVimSelectTests();
    test_Os_Port_Tms570_RegisterCoreVimMapTests();
    test_Os_Port_Tms570_RegisterCoreVimReadChannelTests();
    test_Os_Port_Tms570_RegisterCoreVimReadVectorTests();
    test_Os_Port_Tms570_RegisterCoreVimPulseMaskTests();
    test_Os_Port_Tms570_RegisterCoreVimServiceChannelTests();
    test_Os_Port_Tms570_RegisterCoreVimInvokeVectorTests();
    test_Os_Port_Tms570_RegisterCoreVimEntryCoreTests();
    test_Os_Port_Tms570_RegisterCoreVimEntryTests();
    test_Os_Port_Tms570_RegisterCoreVimServiceCoreTests();
    test_Os_Port_Tms570_RegisterCoreVimServiceTests();
    test_Os_Port_Tms570_RegisterCoreVimDispatchTests();
    test_Os_Port_Tms570_RegisterCoreVimDispatchSelectedTests();
    test_Os_Port_Tms570_RegisterCoreDispatchTests();
    test_Os_Port_Tms570_RegisterCoreFirstTaskViewTests();
    test_Os_Port_Tms570_RegisterCoreHalBridgeTests();
}
