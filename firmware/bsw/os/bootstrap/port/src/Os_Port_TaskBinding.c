/**
 * @file    Os_Port_TaskBinding.c
 * @brief   Bootstrap binding from configured OS tasks into target port contexts
 * @date    2026-03-13
 */
#include "Os_Internal.h"
#include "Os_Port_TaskBinding.h"

#if defined(PLATFORM_STM32)
#include "Os_Port_Stm32.h"
#elif defined(PLATFORM_STM32L5)
#include "Os_Port_Stm32L5.h"
#elif defined(PLATFORM_TMS570)
#include "Os_Port_Tms570.h"
#endif

StatusType Os_Port_PrepareConfiguredTask(TaskType TaskID, uintptr_t StackTop)
{
    if ((os_is_valid_task(TaskID) == FALSE) || (StackTop == (uintptr_t)0u)) {
        return E_OS_VALUE;
    }

#if defined(PLATFORM_STM32)
    return Os_Port_Stm32_PrepareTaskContext(TaskID, os_task_cfg[TaskID].Entry, StackTop);
#elif defined(PLATFORM_STM32L5)
    return Os_Port_Stm32L5_PrepareTaskContext(TaskID, os_task_cfg[TaskID].Entry, StackTop, (uintptr_t)0u);
#elif defined(PLATFORM_TMS570)
    return Os_Port_Tms570_PrepareTaskContext(TaskID, os_task_cfg[TaskID].Entry, StackTop);
#else
    (void)TaskID;
    (void)StackTop;
    return E_OS_STATE;
#endif
}

StatusType Os_Port_PrepareConfiguredFirstTask(TaskType TaskID, uintptr_t StackTop)
{
    if ((os_is_valid_task(TaskID) == FALSE) || (StackTop == (uintptr_t)0u)) {
        return E_OS_VALUE;
    }

#if defined(PLATFORM_STM32)
    return Os_Port_Stm32_PrepareFirstTask(TaskID, os_task_cfg[TaskID].Entry, StackTop);
#elif defined(PLATFORM_STM32L5)
    return Os_Port_Stm32L5_PrepareFirstTask(TaskID, os_task_cfg[TaskID].Entry, StackTop, (uintptr_t)0u);
#elif defined(PLATFORM_TMS570)
    return Os_Port_Tms570_PrepareFirstTask(TaskID, os_task_cfg[TaskID].Entry, StackTop);
#else
    (void)TaskID;
    (void)StackTop;
    return E_OS_STATE;
#endif
}

StatusType Os_Port_SelectConfiguredTask(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return E_OS_VALUE;
    }

#if defined(PLATFORM_STM32)
    return Os_Port_Stm32_SelectNextTask(TaskID);
#elif defined(PLATFORM_STM32L5)
    return Os_Port_Stm32L5_SelectNextTask(TaskID);
#elif defined(PLATFORM_TMS570)
    return Os_Port_Tms570_SelectNextTask(TaskID);
#else
    (void)TaskID;
    return E_OS_STATE;
#endif
}

void Os_Port_SynchronizeConfiguredTask(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return;
    }

#if defined(PLATFORM_STM32)
    Os_Port_Stm32_SynchronizeCurrentTask(TaskID);
#elif defined(PLATFORM_STM32L5)
    Os_Port_Stm32L5_SynchronizeCurrentTask(TaskID);
#elif defined(PLATFORM_TMS570)
    Os_Port_Tms570_SynchronizeCurrentTask(TaskID);
#else
    (void)TaskID;
#endif
}

StatusType Os_Port_RequestConfiguredDispatch(TaskType TaskID)
{
    StatusType status = Os_Port_SelectConfiguredTask(TaskID);

    if (status != E_OK) {
        return status;
    }

    Os_PortRequestContextSwitch();
    return E_OK;
}

StatusType Os_Port_CompleteConfiguredDispatch(void)
{
#if defined(PLATFORM_STM32)
    const Os_Port_Stm32_StateType* state = Os_Port_Stm32_GetBootstrapState();

    if (state->PendSvPending == FALSE) {
        return E_OS_NOFUNC;
    }

    Os_Port_Stm32_PendSvHandler();
    return E_OK;
#elif defined(PLATFORM_STM32L5)
    const Os_Port_Stm32L5_StateType* state = Os_Port_Stm32L5_GetBootstrapState();

    if (state->PendSvPending == FALSE) {
        return E_OS_NOFUNC;
    }

    Os_Port_Stm32L5_PendSvHandler();
    return E_OK;
#elif defined(PLATFORM_TMS570)
    const Os_Port_Tms570_StateType* state = Os_Port_Tms570_GetBootstrapState();

    if (state->DispatchRequested == FALSE) {
        return E_OS_NOFUNC;
    }

    if (state->IrqContextDepth > 0u) {
        Os_Port_Tms570_IrqContextRestore();
    } else {
        Os_Port_Tms570_IrqContextSave();
        Os_Port_Tms570_IrqContextRestore();
    }
    return E_OK;
#else
    return E_OS_STATE;
#endif
}

void Os_Port_ObserveConfiguredDispatch(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return;
    }

#if defined(PLATFORM_STM32)
    Os_Port_Stm32_ObserveKernelDispatch(TaskID);
#elif defined(PLATFORM_STM32L5)
    Os_Port_Stm32L5_ObserveKernelDispatch(TaskID);
#elif defined(PLATFORM_TMS570)
    Os_Port_Tms570_ObserveKernelDispatch(TaskID);
#else
    (void)TaskID;
#endif
}
