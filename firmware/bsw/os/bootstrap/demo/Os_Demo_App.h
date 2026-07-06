/**
 * @file    Os_Demo_App.h
 * @brief   Shared OSEK bootstrap demo application
 * @date    2026-07-06
 */
#ifndef OS_DEMO_APP_H
#define OS_DEMO_APP_H

#include "Os.h"

typedef struct {
    boolean Passed;
    const char* Trace;
    const char* Failure;
    uint8 ScenarioCount;
} Os_Demo_ResultType;

void Os_Demo_Reset(void);
StatusType Os_Demo_RunAll(void);
const Os_Demo_ResultType* Os_Demo_GetResult(void);

const Os_TaskConfigType* Os_Demo_GetTaskConfig(uint8* Count);
const Os_ResourceConfigType* Os_Demo_GetResourceConfig(uint8* Count);
const Os_AlarmConfigType* Os_Demo_GetAlarmConfig(uint8* Count);

#endif /* OS_DEMO_APP_H */
