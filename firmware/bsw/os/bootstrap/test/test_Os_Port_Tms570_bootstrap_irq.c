/**
 * @file    test_Os_Port_Tms570_bootstrap_irq.c
 * @brief   IRQ test registration for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_RegisterIrqSaveTests(void);
void test_Os_Port_Tms570_RegisterIrqContextByteTests(void);
void test_Os_Port_Tms570_RegisterIrqContextCpsrTests(void);
void test_Os_Port_Tms570_RegisterIrqContextScratchTests(void);
void test_Os_Port_Tms570_RegisterIrqSavedContextTests(void);
void test_Os_Port_Tms570_RegisterIrqPendingSaveContextTests(void);
void test_Os_Port_Tms570_RegisterIrqSavedTaskFrameViewTests(void);
void test_Os_Port_Tms570_RegisterIrqPendingSaveTaskFrameViewTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoredContextTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoredTaskFrameViewTests(void);
void test_Os_Port_Tms570_RegisterIrqPendingRestoreTaskFrameViewTests(void);
void test_Os_Port_Tms570_RegisterIrqIdleRestoreTests(void);
void test_Os_Port_Tms570_RegisterIrqSchedulerReturnTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoreTests(void);
void test_Os_Port_Tms570_RegisterIrqDispatchTests(void);
void test_Os_Port_Tms570_RegisterIrqDispatchContextSyncTests(void);
void test_Os_Port_Tms570_RegisterIrqSwitchTests(void);
void test_Os_Port_Tms570_RegisterIrqRuntimeFrameTests(void);
void test_Os_Port_Tms570_RegisterIrqRuntimeMetadataTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoreFrameTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoreApplyFrameTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoreBlockTests(void);
void test_Os_Port_Tms570_RegisterIrqResumeFrameTests(void);
void test_Os_Port_Tms570_RegisterIrqResumeContextSyncTests(void);
void test_Os_Port_Tms570_RegisterIrqResumeContextMetadataTests(void);
void test_Os_Port_Tms570_RegisterIrqRuntimeScratchTests(void);
void test_Os_Port_Tms570_RegisterIrqRuntimeCpsrTests(void);
void test_Os_Port_Tms570_RegisterIrqRuntimeContextSyncTests(void);
void test_Os_Port_Tms570_RegisterIrqRuntimePreservedTests(void);
void test_Os_Port_Tms570_RegisterIrqRuntimeLinkTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoreLinkTests(void);
void test_Os_Port_Tms570_RegisterIrqRuntimeLowerTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoreLowerTests(void);
void test_Os_Port_Tms570_RegisterIrqRuntimeVfpTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoreVfpTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoreSolicitedVfpTests(void);
void test_Os_Port_Tms570_RegisterIrqRestoreSolicitedTests(void);
void test_Os_Port_Tms570_RegisterIrqSolicitedReturnTests(void);
void test_Os_Port_Tms570_RegisterIrqPendingSolicitedSaveTaskFrameViewTests(void);
void test_Os_Port_Tms570_RegisterIrqSolicitedSaveSnapshotTests(void);
void test_Os_Port_Tms570_RegisterIrqFrameByteTests(void);
void test_Os_Port_Tms570_RegisterIrqSavedFrameSpTests(void);
void test_Os_Port_Tms570_RegisterIrqSaveTaskRegsTests(void);

void test_Os_Port_Tms570_RegisterIrqTests(void)
{
    test_Os_Port_Tms570_RegisterIrqSaveTests();
    test_Os_Port_Tms570_RegisterIrqContextByteTests();
    test_Os_Port_Tms570_RegisterIrqContextCpsrTests();
    test_Os_Port_Tms570_RegisterIrqContextScratchTests();
    test_Os_Port_Tms570_RegisterIrqSavedContextTests();
    test_Os_Port_Tms570_RegisterIrqPendingSaveContextTests();
    test_Os_Port_Tms570_RegisterIrqSavedTaskFrameViewTests();
    test_Os_Port_Tms570_RegisterIrqPendingSaveTaskFrameViewTests();
    test_Os_Port_Tms570_RegisterIrqRestoredContextTests();
    test_Os_Port_Tms570_RegisterIrqRestoredTaskFrameViewTests();
    test_Os_Port_Tms570_RegisterIrqPendingRestoreTaskFrameViewTests();
    test_Os_Port_Tms570_RegisterIrqIdleRestoreTests();
    test_Os_Port_Tms570_RegisterIrqSchedulerReturnTests();
    test_Os_Port_Tms570_RegisterIrqRestoreTests();
    test_Os_Port_Tms570_RegisterIrqDispatchTests();
    test_Os_Port_Tms570_RegisterIrqDispatchContextSyncTests();
    test_Os_Port_Tms570_RegisterIrqSwitchTests();
    test_Os_Port_Tms570_RegisterIrqRuntimeFrameTests();
    test_Os_Port_Tms570_RegisterIrqRuntimeMetadataTests();
    test_Os_Port_Tms570_RegisterIrqRestoreFrameTests();
    test_Os_Port_Tms570_RegisterIrqRestoreApplyFrameTests();
    test_Os_Port_Tms570_RegisterIrqRestoreBlockTests();
    test_Os_Port_Tms570_RegisterIrqResumeFrameTests();
    test_Os_Port_Tms570_RegisterIrqResumeContextSyncTests();
    test_Os_Port_Tms570_RegisterIrqResumeContextMetadataTests();
    test_Os_Port_Tms570_RegisterIrqRuntimeScratchTests();
    test_Os_Port_Tms570_RegisterIrqRuntimeCpsrTests();
    test_Os_Port_Tms570_RegisterIrqRuntimeContextSyncTests();
    test_Os_Port_Tms570_RegisterIrqRuntimePreservedTests();
    test_Os_Port_Tms570_RegisterIrqRuntimeLinkTests();
    test_Os_Port_Tms570_RegisterIrqRestoreLinkTests();
    test_Os_Port_Tms570_RegisterIrqRuntimeLowerTests();
    test_Os_Port_Tms570_RegisterIrqRestoreLowerTests();
    test_Os_Port_Tms570_RegisterIrqRuntimeVfpTests();
    test_Os_Port_Tms570_RegisterIrqRestoreVfpTests();
    test_Os_Port_Tms570_RegisterIrqRestoreSolicitedVfpTests();
    test_Os_Port_Tms570_RegisterIrqRestoreSolicitedTests();
    test_Os_Port_Tms570_RegisterIrqSolicitedReturnTests();
    test_Os_Port_Tms570_RegisterIrqPendingSolicitedSaveTaskFrameViewTests();
    test_Os_Port_Tms570_RegisterIrqSolicitedSaveSnapshotTests();
    test_Os_Port_Tms570_RegisterIrqFrameByteTests();
    test_Os_Port_Tms570_RegisterIrqSavedFrameSpTests();
    test_Os_Port_Tms570_RegisterIrqSaveTaskRegsTests();
}
