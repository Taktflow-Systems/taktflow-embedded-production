/**
 * @file    Os_ScheduleTable.c
 * @brief   AUTOSAR OS Schedule Table services (§10)
 * @date    2026-03-15
 *
 * @details Schedule tables provide time-driven task activation and event
 *          setting at configured expiry points along a repeating or
 *          single-shot timeline driven by the system counter.
 *
 * @standard AUTOSAR_CP_OS §10
 * @copyright Taktflow Systems 2026
 */
#include "Os_Internal.h"

Os_ScheduleTableConfigType os_sched_table_cfg[OS_MAX_SCHEDULE_TABLES];
Os_ScheduleTableControlBlockType os_sched_table_cb[OS_MAX_SCHEDULE_TABLES];
uint8 os_sched_table_count = 0u;

boolean os_is_valid_sched_table(ScheduleTableType TableID)
{
    return (boolean)(TableID < os_sched_table_count);
}

#if defined(UNIT_TEST)
void os_clear_sched_table_cfg(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_SCHEDULE_TABLES; idx++) {
        os_sched_table_cfg[idx].Name = NULL_PTR;
        os_sched_table_cfg[idx].Duration = 0u;
        os_sched_table_cfg[idx].Repeating = FALSE;
        os_sched_table_cfg[idx].ExpiryPoints = NULL_PTR;
        os_sched_table_cfg[idx].ExpiryPointCount = 0u;
        os_sched_table_cb[idx].Status = SCHEDULETABLE_STOPPED;
        os_sched_table_cb[idx].StartTick = 0u;
        os_sched_table_cb[idx].ElapsedTicks = 0u;
        os_sched_table_cb[idx].InitialDelay = 0u;
        os_sched_table_cb[idx].NextTable = INVALID_SCHEDULETABLE;
    }

    os_sched_table_count = 0u;
}

StatusType Os_TestConfigureScheduleTables(const Os_ScheduleTableConfigType* Config, uint8 TableCount)
{
    uint8 idx;

    if ((Config == NULL_PTR) || (TableCount > OS_MAX_SCHEDULE_TABLES)) {
        return E_OS_VALUE;
    }

    os_clear_sched_table_cfg();

    for (idx = 0u; idx < TableCount; idx++) {
        os_sched_table_cfg[idx] = Config[idx];
    }

    os_sched_table_count = TableCount;
    return E_OK;
}
#endif

StatusType StartScheduleTableRel(ScheduleTableType ScheduleTableID, TickType Offset)
{
    OS_STACK_SAMPLE(OS_DET_API_START_SCHED_TABLE_REL);

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK | OS_ALLOWED_ISR2) == FALSE) {
        return E_OS_CALLEVEL;
    }

    if (os_is_valid_sched_table(ScheduleTableID) == FALSE) {
        os_report_service_error(OS_DET_API_START_SCHED_TABLE_REL, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if ((Offset == 0u) || (Offset > os_counter_base.maxallowedvalue)) {
        os_report_service_error(OS_DET_API_START_SCHED_TABLE_REL, DET_E_PARAM_VALUE, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_sched_table_cb[ScheduleTableID].Status != SCHEDULETABLE_STOPPED) {
        os_report_service_error(OS_DET_API_START_SCHED_TABLE_REL, DET_E_PARAM_VALUE, E_OS_STATE);
        return E_OS_STATE;
    }

    os_sched_table_cb[ScheduleTableID].Status = SCHEDULETABLE_RUNNING;
    os_sched_table_cb[ScheduleTableID].StartTick = os_counter_value;
    os_sched_table_cb[ScheduleTableID].ElapsedTicks = 0u;
    os_sched_table_cb[ScheduleTableID].InitialDelay = Offset;
    os_sched_table_cb[ScheduleTableID].NextTable = INVALID_SCHEDULETABLE;
    return E_OK;
}

StatusType StartScheduleTableAbs(ScheduleTableType ScheduleTableID, TickType Start)
{
    OS_STACK_SAMPLE(OS_DET_API_START_SCHED_TABLE_ABS);

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK | OS_ALLOWED_ISR2) == FALSE) {
        return E_OS_CALLEVEL;
    }

    if (os_is_valid_sched_table(ScheduleTableID) == FALSE) {
        os_report_service_error(OS_DET_API_START_SCHED_TABLE_ABS, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (Start > os_counter_base.maxallowedvalue) {
        os_report_service_error(OS_DET_API_START_SCHED_TABLE_ABS, DET_E_PARAM_VALUE, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_sched_table_cb[ScheduleTableID].Status != SCHEDULETABLE_STOPPED) {
        os_report_service_error(OS_DET_API_START_SCHED_TABLE_ABS, DET_E_PARAM_VALUE, E_OS_STATE);
        return E_OS_STATE;
    }

    os_sched_table_cb[ScheduleTableID].Status = SCHEDULETABLE_RUNNING;
    os_sched_table_cb[ScheduleTableID].StartTick = Start;
    os_sched_table_cb[ScheduleTableID].ElapsedTicks = 0u;
    os_sched_table_cb[ScheduleTableID].NextTable = INVALID_SCHEDULETABLE;
    return E_OK;
}

StatusType StopScheduleTable(ScheduleTableType ScheduleTableID)
{
    OS_STACK_SAMPLE(OS_DET_API_STOP_SCHED_TABLE);

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK | OS_ALLOWED_ISR2) == FALSE) {
        return E_OS_CALLEVEL;
    }

    if (os_is_valid_sched_table(ScheduleTableID) == FALSE) {
        os_report_service_error(OS_DET_API_STOP_SCHED_TABLE, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_sched_table_cb[ScheduleTableID].Status == SCHEDULETABLE_STOPPED) {
        os_report_service_error(OS_DET_API_STOP_SCHED_TABLE, DET_E_PARAM_VALUE, E_OS_NOFUNC);
        return E_OS_NOFUNC;
    }

    os_sched_table_cb[ScheduleTableID].Status = SCHEDULETABLE_STOPPED;
    os_sched_table_cb[ScheduleTableID].ElapsedTicks = 0u;
    os_sched_table_cb[ScheduleTableID].NextTable = INVALID_SCHEDULETABLE;
    return E_OK;
}

StatusType NextScheduleTable(ScheduleTableType ScheduleTableID_From,
                             ScheduleTableType ScheduleTableID_To)
{
    OS_STACK_SAMPLE(OS_DET_API_NEXT_SCHED_TABLE);

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK | OS_ALLOWED_ISR2) == FALSE) {
        return E_OS_CALLEVEL;
    }

    if (os_is_valid_sched_table(ScheduleTableID_From) == FALSE) {
        os_report_service_error(OS_DET_API_NEXT_SCHED_TABLE, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_is_valid_sched_table(ScheduleTableID_To) == FALSE) {
        os_report_service_error(OS_DET_API_NEXT_SCHED_TABLE, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_sched_table_cb[ScheduleTableID_From].Status != SCHEDULETABLE_RUNNING) {
        os_report_service_error(OS_DET_API_NEXT_SCHED_TABLE, DET_E_PARAM_VALUE, E_OS_NOFUNC);
        return E_OS_NOFUNC;
    }

    if (os_sched_table_cb[ScheduleTableID_To].Status != SCHEDULETABLE_STOPPED) {
        os_report_service_error(OS_DET_API_NEXT_SCHED_TABLE, DET_E_PARAM_VALUE, E_OS_STATE);
        return E_OS_STATE;
    }

    os_sched_table_cb[ScheduleTableID_To].Status = SCHEDULETABLE_NEXT;
    os_sched_table_cb[ScheduleTableID_From].NextTable = ScheduleTableID_To;
    return E_OK;
}

StatusType GetScheduleTableStatus(ScheduleTableType ScheduleTableID,
                                  ScheduleTableStatusType* ScheduleStatus)
{
    OS_STACK_SAMPLE(OS_DET_API_GET_SCHED_TABLE_STATUS);

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK | OS_ALLOWED_ISR2 | OS_ALLOWED_ERROR_HOOK |
                            OS_ALLOWED_PRE_TASK | OS_ALLOWED_POST_TASK) == FALSE) {
        return E_OS_CALLEVEL;
    }

    if (ScheduleStatus == NULL_PTR) {
        os_report_service_error(OS_DET_API_GET_SCHED_TABLE_STATUS, DET_E_PARAM_POINTER, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_is_valid_sched_table(ScheduleTableID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_SCHED_TABLE_STATUS, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    *ScheduleStatus = os_sched_table_cb[ScheduleTableID].Status;
    return E_OK;
}

/**
 * @brief Process one counter tick for all running schedule tables.
 *
 * Called from Os_BootstrapProcessCounterTick() after alarm processing.
 * For each running table, advances elapsed ticks and fires any expiry
 * points whose offset matches. At end of duration, either repeats or
 * transitions to a chained NextTable.
 */
void os_sched_table_process_tick(void)
{
    ScheduleTableType idx;
    uint8 ep;

    for (idx = 0u; idx < os_sched_table_count; idx++) {
        if (os_sched_table_cb[idx].Status != SCHEDULETABLE_RUNNING) {
            continue;
        }

        /* Consume initial delay before starting expiry processing */
        if (os_sched_table_cb[idx].InitialDelay > 0u) {
            os_sched_table_cb[idx].InitialDelay--;
            continue;
        }

        os_sched_table_cb[idx].ElapsedTicks++;

        /* Fire expiry points matching current elapsed offset */
        for (ep = 0u; ep < os_sched_table_cfg[idx].ExpiryPointCount; ep++) {
            if (os_sched_table_cfg[idx].ExpiryPoints[ep].Offset ==
                os_sched_table_cb[idx].ElapsedTicks) {
                TaskType taskId = os_sched_table_cfg[idx].ExpiryPoints[ep].TaskID;
                EventMaskType eventMask = os_sched_table_cfg[idx].ExpiryPoints[ep].EventMask;

                if (eventMask != 0u) {
                    (void)SetEvent(taskId, eventMask);
                } else {
                    (void)os_activate_task_internal(taskId, FALSE);
                }
            }
        }

        /* Check if table duration is complete */
        if (os_sched_table_cb[idx].ElapsedTicks >= os_sched_table_cfg[idx].Duration) {
            if (os_sched_table_cb[idx].NextTable != INVALID_SCHEDULETABLE) {
                /* Transition to chained table */
                ScheduleTableType next = os_sched_table_cb[idx].NextTable;
                os_sched_table_cb[idx].Status = SCHEDULETABLE_STOPPED;
                os_sched_table_cb[idx].ElapsedTicks = 0u;
                os_sched_table_cb[idx].NextTable = INVALID_SCHEDULETABLE;
                os_sched_table_cb[next].Status = SCHEDULETABLE_RUNNING;
                os_sched_table_cb[next].StartTick = os_counter_value;
                os_sched_table_cb[next].ElapsedTicks = 0u;
            } else if (os_sched_table_cfg[idx].Repeating == TRUE) {
                /* Restart from 0 */
                os_sched_table_cb[idx].ElapsedTicks = 0u;
            } else {
                /* Single-shot complete */
                os_sched_table_cb[idx].Status = SCHEDULETABLE_STOPPED;
                os_sched_table_cb[idx].ElapsedTicks = 0u;
            }
        }
    }
}
