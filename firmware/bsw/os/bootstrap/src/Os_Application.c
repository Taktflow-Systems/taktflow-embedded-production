/**
 * @file    Os_Application.c
 * @brief   OS-Application ownership and access queries for the bootstrap kernel
 * @date    2026-03-13
 */
#include "Os_Internal.h"

static uint32 os_get_object_bit(ObjectTypeType ObjectType, uint8 ObjectID)
{
    (void)ObjectType;
    return ((uint32)1u << ObjectID);
}

static ApplicationType os_find_owner_application(ObjectTypeType ObjectType, uint8 ObjectID)
{
    ApplicationType idx;
    uint32 object_bit = os_get_object_bit(ObjectType, ObjectID);

    for (idx = 0u; idx < os_application_count; idx++) {
        switch (ObjectType) {
            case OBJECT_TASK:
                if ((os_application_cfg[idx].OwnedTaskMask & object_bit) != 0u) {
                    return idx;
                }
                break;

            case OBJECT_RESOURCE:
                if ((os_application_cfg[idx].OwnedResourceMask & object_bit) != 0u) {
                    return idx;
                }
                break;

            case OBJECT_ALARM:
                if ((os_application_cfg[idx].OwnedAlarmMask & object_bit) != 0u) {
                    return idx;
                }
                break;

            case OBJECT_IOC:
                if ((os_application_cfg[idx].OwnedIocMask & object_bit) != 0u) {
                    return idx;
                }
                break;

            default:
                return INVALID_OSAPPLICATION;
        }
    }

    return INVALID_OSAPPLICATION;
}

ApplicationType GetApplicationID(void)
{
    if ((os_current_task == INVALID_TASK) || (os_tcb[os_current_task].State != RUNNING)) {
        return INVALID_OSAPPLICATION;
    }

    return os_find_owner_application(OBJECT_TASK, os_current_task);
}

ApplicationType GetCurrentApplicationID(void)
{
    return GetApplicationID();
}

boolean os_current_application_has_access(ObjectTypeType ObjectType, uint8 ObjectID)
{
    ApplicationType caller_app;

    if (os_application_count == 0u) {
        return TRUE;
    }

    if ((os_current_task == INVALID_TASK) || (os_tcb[os_current_task].State != RUNNING)) {
        return TRUE;
    }

    caller_app = GetCurrentApplicationID();
    if (caller_app == INVALID_OSAPPLICATION) {
        return FALSE;
    }

    if (CheckObjectOwnership(ObjectType, ObjectID) == caller_app) {
        return TRUE;
    }

    return (boolean)(CheckObjectAccess(caller_app, ObjectType, ObjectID) == ACCESS);
}

ObjectAccessType CheckObjectAccess(ApplicationType ApplID, ObjectTypeType ObjectType, uint8 ObjectID)
{
    uint32 object_bit;

    if (os_is_valid_application(ApplID) == FALSE) {
        return NO_ACCESS;
    }

    object_bit = os_get_object_bit(ObjectType, ObjectID);

    switch (ObjectType) {
        case OBJECT_TASK:
            return (ObjectAccessType)(((os_application_cfg[ApplID].AccessibleTaskMask & object_bit) != 0u)
                                          ? ACCESS
                                          : NO_ACCESS);

        case OBJECT_RESOURCE:
            return (ObjectAccessType)(((os_application_cfg[ApplID].AccessibleResourceMask & object_bit) != 0u)
                                          ? ACCESS
                                          : NO_ACCESS);

        case OBJECT_ALARM:
            return (ObjectAccessType)(((os_application_cfg[ApplID].AccessibleAlarmMask & object_bit) != 0u)
                                          ? ACCESS
                                          : NO_ACCESS);

        case OBJECT_IOC:
            return (ObjectAccessType)(((os_application_cfg[ApplID].AccessibleIocMask & object_bit) != 0u)
                                          ? ACCESS
                                          : NO_ACCESS);

        default:
            return NO_ACCESS;
    }
}

ApplicationType CheckObjectOwnership(ObjectTypeType ObjectType, uint8 ObjectID)
{
    switch (ObjectType) {
        case OBJECT_TASK:
            if (os_is_valid_task((TaskType)ObjectID) == FALSE) {
                return INVALID_OSAPPLICATION;
            }
            break;

        case OBJECT_RESOURCE:
            if (os_is_valid_resource((ResourceType)ObjectID) == FALSE) {
                return INVALID_OSAPPLICATION;
            }
            break;

        case OBJECT_ALARM:
            if (os_is_valid_alarm((AlarmType)ObjectID) == FALSE) {
                return INVALID_OSAPPLICATION;
            }
            break;

        case OBJECT_IOC:
            if (os_is_valid_ioc((IocType)ObjectID) == FALSE) {
                return INVALID_OSAPPLICATION;
            }
            break;

        default:
            return INVALID_OSAPPLICATION;
    }

    return os_find_owner_application(ObjectType, ObjectID);
}

StatusType CallTrustedFunction(TrustedFunctionIndexType FunctionIndex,
                               TrustedFunctionParameterRefType FunctionParams)
{
    ApplicationType caller_app;
    uint32 caller_bit;
    OS_STACK_SAMPLE(OS_DET_API_CALL_TRUSTED_FUNCTION);

    if (os_is_valid_trusted_function(FunctionIndex) == FALSE) {
        os_report_service_error(OS_DET_API_CALL_TRUSTED_FUNCTION, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    caller_app = GetCurrentApplicationID();
    if (caller_app == INVALID_OSAPPLICATION) {
        os_report_service_error(OS_DET_API_CALL_TRUSTED_FUNCTION, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    caller_bit = ((uint32)1u << caller_app);
    if ((os_trusted_function_cfg[FunctionIndex].AccessibleApplicationMask & caller_bit) == 0u) {
        os_report_service_error(OS_DET_API_CALL_TRUSTED_FUNCTION, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    return os_trusted_function_cfg[FunctionIndex].Handler(FunctionParams);
}
