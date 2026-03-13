/**
 * @file    Os_Task.c
 * @brief   Task services for the OSEK bootstrap kernel
 * @date    2026-03-13
 */
#include "Os_Internal.h"

StatusType os_activate_task_internal(TaskType TaskID, boolean AllowPreemption)
{
    Os_TaskControlBlockType* tcb_ptr;

    if (os_is_valid_task(TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_ACTIVATE_TASK, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    tcb_ptr = &os_tcb[TaskID];

    if (tcb_ptr->PendingActivations >= os_task_cfg[TaskID].ActivationLimit) {
        os_report_service_error(OS_DET_API_ACTIVATE_TASK, DET_E_PARAM_VALUE, E_OS_LIMIT);
        return E_OS_LIMIT;
    }

    tcb_ptr->PendingActivations++;

    if (tcb_ptr->State == SUSPENDED) {
        tcb_ptr->State = READY;
        tcb_ptr->ReadyStamp = os_ready_stamp_counter++;
        tcb_ptr->CurrentPriority = os_task_cfg[TaskID].Priority;
        tcb_ptr->SetEvents = 0u;
        tcb_ptr->WaitEvents = 0u;
    }

    os_rebuild_ready_bitmap();

    if (AllowPreemption == TRUE) {
        (void)os_maybe_dispatch_preemption();
    }

    return E_OK;
}

StatusType ActivateTask(TaskType TaskID)
{
    OS_STACK_SAMPLE(OS_DET_API_ACTIVATE_TASK);

    if (os_started == FALSE) {
        os_report_service_error(OS_DET_API_ACTIVATE_TASK, DET_E_UNINIT, E_OS_STATE);
        return E_OS_STATE;
    }

    if (os_current_application_has_access(OBJECT_TASK, TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_ACTIVATE_TASK, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    return os_activate_task_internal(TaskID, TRUE);
}

StatusType TerminateTask(void)
{
    OS_STACK_SAMPLE(OS_DET_API_TERMINATE_TASK);

    if ((os_started == FALSE) || (os_current_task == INVALID_TASK)) {
        os_report_service_error(OS_DET_API_TERMINATE_TASK, DET_E_PARAM_VALUE, E_OS_CALLEVEL);
        return E_OS_CALLEVEL;
    }

    if (os_tcb[os_current_task].ResourceCount != 0u) {
        os_report_service_error(OS_DET_API_TERMINATE_TASK, DET_E_PARAM_VALUE, E_OS_RESOURCE);
        return E_OS_RESOURCE;
    }

    os_complete_running_task();
    return E_OK;
}

StatusType ChainTask(TaskType TaskID)
{
    OS_STACK_SAMPLE(OS_DET_API_CHAIN_TASK);

    if ((os_started == FALSE) || (os_current_task == INVALID_TASK)) {
        os_report_service_error(OS_DET_API_CHAIN_TASK, DET_E_PARAM_VALUE, E_OS_CALLEVEL);
        return E_OS_CALLEVEL;
    }

    if (os_is_valid_task(TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_CHAIN_TASK, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_current_application_has_access(OBJECT_TASK, TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_CHAIN_TASK, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    if (os_tcb[os_current_task].ResourceCount != 0u) {
        os_report_service_error(OS_DET_API_CHAIN_TASK, DET_E_PARAM_VALUE, E_OS_RESOURCE);
        return E_OS_RESOURCE;
    }

    if ((TaskID != os_current_task) &&
        (os_tcb[TaskID].PendingActivations >= os_task_cfg[TaskID].ActivationLimit)) {
        os_report_service_error(OS_DET_API_CHAIN_TASK, DET_E_PARAM_VALUE, E_OS_LIMIT);
        return E_OS_LIMIT;
    }

    os_complete_running_task();
    return os_activate_task_internal(TaskID, TRUE);
}

StatusType GetTaskID(TaskRefType TaskID)
{
    OS_STACK_SAMPLE(OS_DET_API_GET_TASK_ID);

    if (TaskID == NULL_PTR) {
        os_report_service_error(OS_DET_API_GET_TASK_ID, DET_E_PARAM_POINTER, E_OS_VALUE);
        return E_OS_VALUE;
    }

    *TaskID = os_current_task;
    return E_OK;
}

StatusType GetTaskState(TaskType TaskID, TaskStateRefType State)
{
    OS_STACK_SAMPLE(OS_DET_API_GET_TASK_STATE);

    if (State == NULL_PTR) {
        os_report_service_error(OS_DET_API_GET_TASK_STATE, DET_E_PARAM_POINTER, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_is_valid_task(TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_TASK_STATE, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    *State = os_tcb[TaskID].State;
    return E_OK;
}

#if defined(UNIT_TEST)
uint8 Os_TestGetPendingActivations(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return 0u;
    }

    return os_tcb[TaskID].PendingActivations;
}
#endif
