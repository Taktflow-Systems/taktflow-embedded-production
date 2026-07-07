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
void test_Os_Port_Tms570_RegisterFiqDispatchContextSyncTests(void);
void test_Os_Port_Tms570_RegisterFiqRestoreTests(void);
void test_Os_Port_Tms570_RegisterFiqContextStateTests(void);
void test_Os_Port_Tms570_RegisterFiqPendingSaveContextTests(void);
void test_Os_Port_Tms570_RegisterFiqPendingSaveTaskFrameViewTests(void);
void test_Os_Port_Tms570_RegisterFiqSavedContextTests(void);
void test_Os_Port_Tms570_RegisterFiqSavedTaskFrameViewTests(void);
void test_Os_Port_Tms570_RegisterFiqRestoredContextTests(void);
void test_Os_Port_Tms570_RegisterFiqRestoredTaskFrameViewTests(void);
void test_Os_Port_Tms570_RegisterFiqPendingRestoreTaskFrameViewTests(void);
void test_Os_Port_Tms570_RegisterFiqSchedulerReturnTests(void);
void test_Os_Port_Tms570_RegisterFiqSchedulerReturnCompletionTests(void);
void test_Os_Port_Tms570_RegisterFiqResumeFrameTests(void);
void test_Os_Port_Tms570_RegisterFiqResumeContextSyncTests(void);
void test_Os_Port_Tms570_RegisterFiqResumeContextMetadataTests(void);
void test_Os_Port_Tms570_RegisterFiqRuntimeContextSyncTests(void);
void test_Os_Port_Tms570_RegisterFiqRuntimeMetadataTests(void);
void test_Os_Port_Tms570_RegisterFiqRuntimePreservedTests(void);
void test_Os_Port_Tms570_RegisterFiqRuntimeLinkTests(void);
void test_Os_Port_Tms570_RegisterFiqRuntimeLowerTests(void);
void test_Os_Port_Tms570_RegisterFiqRuntimeVfpTests(void);
void test_Os_Port_Tms570_RegisterFiqSavedFrameSpTests(void);
void test_Os_Port_Tms570_RegisterFiqSaveTaskRegsTests(void);

void test_Os_Port_Tms570_RegisterFiqTests(void)
{
    test_Os_Port_Tms570_RegisterFiqLifecycleTests();
    test_Os_Port_Tms570_RegisterFiqSaveTests();
    test_Os_Port_Tms570_RegisterFiqNestedTests();
    test_Os_Port_Tms570_RegisterFiqProcessingTests();
    test_Os_Port_Tms570_RegisterFiqDispatchTests();
    test_Os_Port_Tms570_RegisterFiqDispatchContextSyncTests();
    test_Os_Port_Tms570_RegisterFiqRestoreTests();
    test_Os_Port_Tms570_RegisterFiqContextStateTests();
    test_Os_Port_Tms570_RegisterFiqPendingSaveContextTests();
    test_Os_Port_Tms570_RegisterFiqPendingSaveTaskFrameViewTests();
    test_Os_Port_Tms570_RegisterFiqSavedContextTests();
    test_Os_Port_Tms570_RegisterFiqSavedTaskFrameViewTests();
    test_Os_Port_Tms570_RegisterFiqRestoredContextTests();
    test_Os_Port_Tms570_RegisterFiqRestoredTaskFrameViewTests();
    test_Os_Port_Tms570_RegisterFiqPendingRestoreTaskFrameViewTests();
    test_Os_Port_Tms570_RegisterFiqSchedulerReturnTests();
    test_Os_Port_Tms570_RegisterFiqSchedulerReturnCompletionTests();
    test_Os_Port_Tms570_RegisterFiqResumeFrameTests();
    test_Os_Port_Tms570_RegisterFiqResumeContextSyncTests();
    test_Os_Port_Tms570_RegisterFiqResumeContextMetadataTests();
    test_Os_Port_Tms570_RegisterFiqRuntimeContextSyncTests();
    test_Os_Port_Tms570_RegisterFiqRuntimeMetadataTests();
    test_Os_Port_Tms570_RegisterFiqRuntimePreservedTests();
    test_Os_Port_Tms570_RegisterFiqRuntimeLinkTests();
    test_Os_Port_Tms570_RegisterFiqRuntimeLowerTests();
    test_Os_Port_Tms570_RegisterFiqRuntimeVfpTests();
    test_Os_Port_Tms570_RegisterFiqSavedFrameSpTests();
    test_Os_Port_Tms570_RegisterFiqSaveTaskRegsTests();
}
