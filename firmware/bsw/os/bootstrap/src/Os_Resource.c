/**
 * @file    Os_Resource.c
 * @brief   Resource management and PCP handling for the OSEK bootstrap kernel
 * @date    2026-03-13
 */
#include "Os_Internal.h"

static boolean os_is_running_task_context(void)
{
    if ((os_started == FALSE) || (os_current_task == INVALID_TASK)) {
        return FALSE;
    }

    if (os_tcb[os_current_task].State != RUNNING) {
        return FALSE;
    }

    return TRUE;
}

StatusType GetResource(ResourceType ResID)
{
    Os_TaskControlBlockType* current_tcb;
    OS_STACK_SAMPLE(OS_DET_API_GET_RESOURCE);

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK | OS_ALLOWED_ISR2) == FALSE) {
        return E_OS_CALLEVEL;
    }

    if (os_is_running_task_context() == FALSE) {
        os_report_service_error(OS_DET_API_GET_RESOURCE, DET_E_PARAM_VALUE, E_OS_CALLEVEL);
        return E_OS_CALLEVEL;
    }

    if (os_is_valid_resource(ResID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_RESOURCE, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_current_application_has_access(OBJECT_RESOURCE, ResID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_RESOURCE, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    if (os_resource_cb[ResID].InUse == TRUE) {
        os_report_service_error(OS_DET_API_GET_RESOURCE, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    current_tcb = &os_tcb[os_current_task];
    if (current_tcb->ResourceCount >= OS_MAX_RESOURCES) {
        os_report_service_error(OS_DET_API_GET_RESOURCE, DET_E_PARAM_VALUE, E_OS_LIMIT);
        return E_OS_LIMIT;
    }

    current_tcb->ResourceStack[current_tcb->ResourceCount] = ResID;
    current_tcb->ResourceCount++;
    os_resource_cb[ResID].InUse = TRUE;
    os_resource_cb[ResID].OwnerTask = os_current_task;
    os_recalculate_task_priority(os_current_task);

    return E_OK;
}

StatusType ReleaseResource(ResourceType ResID)
{
    Os_TaskControlBlockType* current_tcb;
    OS_STACK_SAMPLE(OS_DET_API_RELEASE_RESOURCE);

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK | OS_ALLOWED_ISR2) == FALSE) {
        return E_OS_CALLEVEL;
    }

    if (os_is_running_task_context() == FALSE) {
        os_report_service_error(OS_DET_API_RELEASE_RESOURCE, DET_E_PARAM_VALUE, E_OS_CALLEVEL);
        return E_OS_CALLEVEL;
    }

    if (os_is_valid_resource(ResID) == FALSE) {
        os_report_service_error(OS_DET_API_RELEASE_RESOURCE, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    current_tcb = &os_tcb[os_current_task];
    if ((os_resource_cb[ResID].InUse == FALSE) ||
        (os_resource_cb[ResID].OwnerTask != os_current_task) ||
        (current_tcb->ResourceCount == 0u) ||
        (current_tcb->ResourceStack[current_tcb->ResourceCount - 1u] != ResID)) {
        os_report_service_error(OS_DET_API_RELEASE_RESOURCE, DET_E_PARAM_VALUE, E_OS_NOFUNC);
        return E_OS_NOFUNC;
    }

    current_tcb->ResourceCount--;
    current_tcb->ResourceStack[current_tcb->ResourceCount] = INVALID_RESOURCE;
    os_resource_cb[ResID].InUse = FALSE;
    os_resource_cb[ResID].OwnerTask = INVALID_TASK;
    os_recalculate_task_priority(os_current_task);
    (void)os_maybe_dispatch_preemption();

    return E_OK;
}
