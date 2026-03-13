/**
 * @file    Os_Ioc.c
 * @brief   Bootstrap IOC queue services for the OSEK-first OS lab
 * @date    2026-03-13
 */
#include "Os_Internal.h"

static uint8 os_ioc_next_index(uint8 Index, uint8 QueueLength)
{
    Index++;

    if (Index >= QueueLength) {
        return 0u;
    }

    return Index;
}

StatusType IocSend(IocType IocID, uint32 Data)
{
    Os_IocControlBlockType* cb_ptr;
    uint8 queue_length;
    OS_STACK_SAMPLE(OS_DET_API_IOC_SEND);

    if (os_started == FALSE) {
        os_report_service_error(OS_DET_API_IOC_SEND, DET_E_UNINIT, E_OS_STATE);
        return E_OS_STATE;
    }

    if (os_is_valid_ioc(IocID) == FALSE) {
        os_report_service_error(OS_DET_API_IOC_SEND, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_current_application_has_access(OBJECT_IOC, IocID) == FALSE) {
        os_report_service_error(OS_DET_API_IOC_SEND, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    cb_ptr = &os_ioc_cb[IocID];
    queue_length = os_ioc_cfg[IocID].QueueLength;

    if (cb_ptr->Count >= queue_length) {
        os_report_service_error(OS_DET_API_IOC_SEND, DET_E_PARAM_VALUE, IOC_E_LIMIT);
        return IOC_E_LIMIT;
    }

    cb_ptr->Buffer[cb_ptr->Tail] = Data;
    cb_ptr->Tail = os_ioc_next_index(cb_ptr->Tail, queue_length);
    cb_ptr->Count++;
    return E_OK;
}

StatusType IocReceive(IocType IocID, uint32* Data)
{
    Os_IocControlBlockType* cb_ptr;
    uint8 queue_length;
    OS_STACK_SAMPLE(OS_DET_API_IOC_RECEIVE);

    if (Data == NULL_PTR) {
        os_report_service_error(OS_DET_API_IOC_RECEIVE, DET_E_PARAM_POINTER, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_started == FALSE) {
        os_report_service_error(OS_DET_API_IOC_RECEIVE, DET_E_UNINIT, E_OS_STATE);
        return E_OS_STATE;
    }

    if (os_is_valid_ioc(IocID) == FALSE) {
        os_report_service_error(OS_DET_API_IOC_RECEIVE, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_current_application_has_access(OBJECT_IOC, IocID) == FALSE) {
        os_report_service_error(OS_DET_API_IOC_RECEIVE, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    cb_ptr = &os_ioc_cb[IocID];
    queue_length = os_ioc_cfg[IocID].QueueLength;

    if (cb_ptr->Count == 0u) {
        os_report_service_error(OS_DET_API_IOC_RECEIVE, DET_E_PARAM_VALUE, IOC_E_NO_DATA);
        return IOC_E_NO_DATA;
    }

    *Data = cb_ptr->Buffer[cb_ptr->Head];
    cb_ptr->Head = os_ioc_next_index(cb_ptr->Head, queue_length);
    cb_ptr->Count--;
    return E_OK;
}

StatusType IocEmptyQueue(IocType IocID)
{
    OS_STACK_SAMPLE(OS_DET_API_IOC_EMPTY_QUEUE);

    if (os_started == FALSE) {
        os_report_service_error(OS_DET_API_IOC_EMPTY_QUEUE, DET_E_UNINIT, E_OS_STATE);
        return E_OS_STATE;
    }

    if (os_is_valid_ioc(IocID) == FALSE) {
        os_report_service_error(OS_DET_API_IOC_EMPTY_QUEUE, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_current_application_has_access(OBJECT_IOC, IocID) == FALSE) {
        os_report_service_error(OS_DET_API_IOC_EMPTY_QUEUE, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    os_ioc_cb[IocID].Head = 0u;
    os_ioc_cb[IocID].Tail = 0u;
    os_ioc_cb[IocID].Count = 0u;
    return E_OK;
}
