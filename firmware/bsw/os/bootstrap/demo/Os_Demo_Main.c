/**
 * @file    Os_Demo_Main.c
 * @brief   CLI entry point for the OSEK bootstrap demo application
 * @date    2026-07-06
 */
#include <stdio.h>

#include "Os_Demo_App.h"

int main(void)
{
    const Os_Demo_ResultType* result;
    StatusType status = Os_Demo_RunAll();

    result = Os_Demo_GetResult();

    if ((status == E_OK) && (result->Passed != FALSE)) {
        printf("OSEK_DEMO PASS scenarios=%u trace=%s\n",
               (unsigned int)result->ScenarioCount,
               result->Trace);
        return 0;
    }

    printf("OSEK_DEMO FAIL scenarios=%u failure=%s trace=%s\n",
           (unsigned int)result->ScenarioCount,
           result->Failure,
           result->Trace);
    return 1;
}
