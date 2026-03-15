/**
 * @file    Os_Event.c
 * @brief   Event services for the OSEK bootstrap kernel
 * @date    2026-03-13
 */
#include "Os_Internal.h"

static boolean os_is_running_extended_task_context(void)
{
    if ((os_started == FALSE) || (os_current_task == INVALID_TASK)) {
        return FALSE;
    }

    if (os_tcb[os_current_task].State != RUNNING) {
        return FALSE;
    }

    if (os_is_extended_task(os_current_task) == FALSE) {
        return FALSE;
    }

    return TRUE;
}

StatusType SetEvent(TaskType TaskID, EventMaskType Mask)
{
    Os_TaskControlBlockType* tcb_ptr;
    OS_STACK_SAMPLE(OS_DET_API_SET_EVENT);

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK | OS_ALLOWED_ISR2) == FALSE) {
        return E_OS_CALLEVEL;
    }

    if (os_is_valid_task(TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_SET_EVENT, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_is_extended_task(TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_SET_EVENT, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    if (os_current_application_has_access(OBJECT_TASK, TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_SET_EVENT, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    tcb_ptr = &os_tcb[TaskID];
    if (tcb_ptr->State == SUSPENDED) {
        os_report_service_error(OS_DET_API_SET_EVENT, DET_E_PARAM_VALUE, E_OS_STATE);
        return E_OS_STATE;
    }

    tcb_ptr->SetEvents |= Mask;

    if ((tcb_ptr->State == WAITING) && ((tcb_ptr->SetEvents & tcb_ptr->WaitEvents) != 0u)) {
        tcb_ptr->State = READY;
        tcb_ptr->WaitEvents = 0u;
        tcb_ptr->ReadyStamp = os_ready_stamp_counter++;
        os_rebuild_ready_bitmap();
        (void)os_maybe_dispatch_preemption();
    }

    return E_OK;
}

StatusType ClearEvent(EventMaskType Mask)
{
    OS_STACK_SAMPLE(OS_DET_API_CLEAR_EVENT);

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK) == FALSE) {
        return E_OS_CALLEVEL;
    }

    if ((os_started == FALSE) || (os_current_task == INVALID_TASK)) {
        os_report_service_error(OS_DET_API_CLEAR_EVENT, DET_E_PARAM_VALUE, E_OS_CALLEVEL);
        return E_OS_CALLEVEL;
    }

    if (os_is_running_extended_task_context() == FALSE) {
        os_report_service_error(OS_DET_API_CLEAR_EVENT, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    os_tcb[os_current_task].SetEvents &= ~Mask;
    return E_OK;
}

StatusType GetEvent(TaskType TaskID, EventMaskRefType Event)
{
    OS_STACK_SAMPLE(OS_DET_API_GET_EVENT);

    if (Event == NULL_PTR) {
        os_report_service_error(OS_DET_API_GET_EVENT, DET_E_PARAM_POINTER, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_is_valid_task(TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_EVENT, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_is_extended_task(TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_EVENT, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    if (os_current_application_has_access(OBJECT_TASK, TaskID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_EVENT, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    if (os_tcb[TaskID].State == SUSPENDED) {
        os_report_service_error(OS_DET_API_GET_EVENT, DET_E_PARAM_VALUE, E_OS_STATE);
        return E_OS_STATE;
    }

    *Event = os_tcb[TaskID].SetEvents;
    return E_OK;
}

StatusType WaitEvent(EventMaskType Mask)
{
    OS_STACK_SAMPLE(OS_DET_API_WAIT_EVENT);

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK) == FALSE) {
        return E_OS_CALLEVEL;
    }

    if ((os_started == FALSE) || (os_current_task == INVALID_TASK)) {
        os_report_service_error(OS_DET_API_WAIT_EVENT, DET_E_PARAM_VALUE, E_OS_CALLEVEL);
        return E_OS_CALLEVEL;
    }

    if (os_is_running_extended_task_context() == FALSE) {
        os_report_service_error(OS_DET_API_WAIT_EVENT, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    if (os_tcb[os_current_task].ResourceCount != 0u) {
        os_report_service_error(OS_DET_API_WAIT_EVENT, DET_E_PARAM_VALUE, E_OS_RESOURCE);
        return E_OS_RESOURCE;
    }

    if ((os_tcb[os_current_task].SetEvents & Mask) != 0u) {
        return E_OK;
    }

    os_tcb[os_current_task].WaitEvents = Mask;
    os_tcb[os_current_task].State = WAITING;
    os_tcb[os_current_task].ReadyStamp = 0u;
    os_current_task = INVALID_TASK;
    os_rebuild_ready_bitmap();
    return E_OK;
}
