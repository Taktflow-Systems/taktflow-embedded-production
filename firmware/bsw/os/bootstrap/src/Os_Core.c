/**
 * @file    Os_Core.c
 * @brief   Core control and bootstrap helpers for the OSEK bootstrap kernel
 * @date    2026-03-13
 */
#include "Os_Internal.h"

#include "Os_Cfg_Types.h"

#if defined(PLATFORM_STM32) || defined(PLATFORM_STM32L5) || defined(PLATFORM_TMS570)
#include "Os_Port.h"
#include "Os_Port_TaskBinding.h"
#endif
#if defined(PLATFORM_TMS570)
#include "Os_Port_Tms570.h"
#endif

Os_TaskConfigType os_task_cfg[OS_MAX_TASKS];
Os_TaskControlBlockType os_tcb[OS_MAX_TASKS];
uint8 os_task_count = 0u;
Os_ResourceConfigType os_resource_cfg[OS_MAX_RESOURCES];
Os_ResourceControlBlockType os_resource_cb[OS_MAX_RESOURCES];
uint8 os_resource_count = 0u;
Os_AlarmConfigType os_alarm_cfg[OS_MAX_ALARMS];
Os_AlarmControlBlockType os_alarm_cb[OS_MAX_ALARMS];
uint8 os_alarm_count = 0u;
Os_ApplicationConfigType os_application_cfg[OS_MAX_APPLICATIONS];
uint8 os_application_count = 0u;
Os_IocConfigType os_ioc_cfg[OS_MAX_IOCS];
Os_IocControlBlockType os_ioc_cb[OS_MAX_IOCS];
uint8 os_ioc_count = 0u;
uint16 os_stack_budget_cfg[OS_MAX_TASKS];
Os_MemoryRegionControlType os_memory_region_cfg[OS_MAX_MEMORY_REGIONS];
uint8 os_memory_region_count = 0u;
Os_TrustedFunctionConfigType os_trusted_function_cfg[OS_MAX_TRUSTED_FUNCTIONS];
uint8 os_trusted_function_count = 0u;
AlarmBaseType os_counter_base = { 0xFFFFFFFFu, 1u, 1u };
TickType os_counter_value = 0u;
TaskType os_current_task = INVALID_TASK;
AppModeType os_active_app_mode = OSDEFAULTAPPMODE;
StatusType os_shutdown_error = E_OK;
boolean os_initialized = FALSE;
boolean os_started = FALSE;
boolean os_shutdown_requested = FALSE;
uint32 os_ready_bitmap = 0u;
uint32 os_ready_stamp_counter = 1u;
uint32 os_dispatch_count = 0u;
Os_HookType os_startup_hook = 0;
Os_ErrorHookType os_error_hook = 0;
Os_HookType os_pre_task_hook = 0;
Os_HookType os_post_task_hook = 0;
Os_ShutdownHookType os_shutdown_hook = 0;

/* Config clear helpers: production code since S-OS-10 (Os_Configure
 * wipes all areas before applying and after a rejected configuration). */
void os_clear_task_cfg(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_TASKS; idx++) {
        os_task_cfg[idx].Name = (const char*)0;
        os_task_cfg[idx].Entry = (Os_TaskEntryType)0;
        os_task_cfg[idx].Priority = 0u;
        os_task_cfg[idx].ActivationLimit = 0u;
        os_task_cfg[idx].AutostartMask = 0u;
        os_task_cfg[idx].Extended = FALSE;
        os_task_cfg[idx].Schedule = FULL;
    }
}

void os_clear_resource_cfg(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_RESOURCES; idx++) {
        os_resource_cfg[idx].Name = (const char*)0;
        os_resource_cfg[idx].CeilingPriority = 0u;
    }
}

void os_clear_alarm_cfg(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_ALARMS; idx++) {
        os_alarm_cfg[idx].Name = (const char*)0;
        os_alarm_cfg[idx].TaskID = INVALID_TASK;
        os_alarm_cfg[idx].MaxAllowedValue = 0u;
        os_alarm_cfg[idx].TicksPerBase = 0u;
        os_alarm_cfg[idx].MinCycle = 0u;
    }
}

void os_clear_application_cfg(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_APPLICATIONS; idx++) {
        os_application_cfg[idx].Name = (const char*)0;
        os_application_cfg[idx].Trusted = FALSE;
        os_application_cfg[idx].OwnedTaskMask = 0u;
        os_application_cfg[idx].AccessibleTaskMask = 0u;
        os_application_cfg[idx].OwnedResourceMask = 0u;
        os_application_cfg[idx].AccessibleResourceMask = 0u;
        os_application_cfg[idx].OwnedAlarmMask = 0u;
        os_application_cfg[idx].AccessibleAlarmMask = 0u;
        os_application_cfg[idx].OwnedIocMask = 0u;
        os_application_cfg[idx].AccessibleIocMask = 0u;
    }
}

void os_clear_ioc_cfg(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_IOCS; idx++) {
        os_ioc_cfg[idx].Name = (const char*)0;
        os_ioc_cfg[idx].QueueLength = 0u;
    }
}

void os_clear_stack_cfg(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_TASKS; idx++) {
        os_stack_budget_cfg[idx] = 0u;
    }
}

void os_clear_memory_region_cfg(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_MEMORY_REGIONS; idx++) {
        os_memory_region_cfg[idx].Name = (const char*)0;
        os_memory_region_cfg[idx].Application = INVALID_OSAPPLICATION;
        os_memory_region_cfg[idx].StartAddress = (MemoryStartAddressType)0u;
        os_memory_region_cfg[idx].Size = 0u;
    }
}

void os_clear_trusted_function_cfg(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_TRUSTED_FUNCTIONS; idx++) {
        os_trusted_function_cfg[idx].Name = (const char*)0;
        os_trusted_function_cfg[idx].Handler = (Os_TrustedFunctionType)0;
        os_trusted_function_cfg[idx].AccessibleApplicationMask = 0u;
    }
}

void os_reset_runtime_state(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_TASKS; idx++) {
        os_tcb[idx].State = SUSPENDED;
        os_tcb[idx].PendingActivations = 0u;
        os_tcb[idx].ReadyStamp = 0u;
        os_tcb[idx].CurrentPriority = 0u;
        os_tcb[idx].ResourceCount = 0u;
        os_tcb[idx].SetEvents = 0u;
        os_tcb[idx].WaitEvents = 0u;
        os_tcb[idx].StackBaseAddr = (uintptr_t)0u;
        os_tcb[idx].PeakStackUsage = 0u;
        os_tcb[idx].StackViolation = FALSE;
    }

    for (idx = 0u; idx < OS_MAX_TASKS; idx++) {
        uint8 resource_idx;

        for (resource_idx = 0u; resource_idx < OS_MAX_RESOURCES; resource_idx++) {
            os_tcb[idx].ResourceStack[resource_idx] = INVALID_RESOURCE;
        }
    }

    for (idx = 0u; idx < OS_MAX_RESOURCES; idx++) {
        os_resource_cb[idx].InUse = FALSE;
        os_resource_cb[idx].OwnerTask = INVALID_TASK;
    }

    for (idx = 0u; idx < OS_MAX_ALARMS; idx++) {
        os_alarm_cb[idx].Active = FALSE;
        os_alarm_cb[idx].AlarmTime = 0u;
        os_alarm_cb[idx].Cycle = 0u;
    }

    for (idx = 0u; idx < OS_MAX_IOCS; idx++) {
        uint8 queue_idx;

        os_ioc_cb[idx].Head = 0u;
        os_ioc_cb[idx].Tail = 0u;
        os_ioc_cb[idx].Count = 0u;

        for (queue_idx = 0u; queue_idx < OS_MAX_IOC_QUEUE_LENGTH; queue_idx++) {
            os_ioc_cb[idx].Buffer[queue_idx] = 0u;
        }
    }

    for (idx = 0u; idx < OS_MAX_SCHEDULE_TABLES; idx++) {
        os_sched_table_cb[idx].Status = SCHEDULETABLE_STOPPED;
        os_sched_table_cb[idx].StartTick = 0u;
        os_sched_table_cb[idx].ElapsedTicks = 0u;
        os_sched_table_cb[idx].InitialDelay = 0u;
        os_sched_table_cb[idx].NextTable = INVALID_SCHEDULETABLE;
    }

    os_current_task = INVALID_TASK;
    os_active_app_mode = OSDEFAULTAPPMODE;
    os_shutdown_error = E_OK;
    os_initialized = TRUE;
    os_started = FALSE;
    os_shutdown_requested = FALSE;
    os_ready_bitmap = 0u;
    os_ready_stamp_counter = 1u;
    os_dispatch_count = 0u;
    os_isr_cat2_nesting = 0u;
    os_preempted_task_depth = 0u;
    os_counter_value = 0u;
    os_all_interrupts_disabled = FALSE;
    os_suspend_all_nesting = 0u;
    os_suspend_os_nesting = 0u;
#if defined(UNIT_TEST)
    os_test_all_interrupts_disabled = 0u;
    os_test_os_interrupts_disabled = 0u;
#endif
}

boolean os_is_valid_task(TaskType TaskID)
{
    if (TaskID >= os_task_count) {
        return FALSE;
    }

    if (os_task_cfg[TaskID].Entry == (Os_TaskEntryType)0) {
        return FALSE;
    }

    return TRUE;
}

boolean os_is_valid_resource(ResourceType ResID)
{
    if (ResID >= os_resource_count) {
        return FALSE;
    }

    if (os_resource_cfg[ResID].Name == (const char*)0) {
        return FALSE;
    }

    return TRUE;
}

boolean os_is_extended_task(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return FALSE;
    }

    return os_task_cfg[TaskID].Extended;
}

boolean os_is_preemptive_task(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return FALSE;
    }

    return (boolean)(os_task_cfg[TaskID].Schedule == FULL);
}

boolean os_is_valid_alarm(AlarmType AlarmID)
{
    if (AlarmID >= os_alarm_count) {
        return FALSE;
    }

    if (os_alarm_cfg[AlarmID].Name == (const char*)0) {
        return FALSE;
    }

    return TRUE;
}

boolean os_is_valid_application(ApplicationType ApplID)
{
    if (ApplID >= os_application_count) {
        return FALSE;
    }

    if (os_application_cfg[ApplID].Name == (const char*)0) {
        return FALSE;
    }

    return TRUE;
}

boolean os_is_valid_ioc(IocType IocID)
{
    if (IocID >= os_ioc_count) {
        return FALSE;
    }

    if ((os_ioc_cfg[IocID].Name == (const char*)0) || (os_ioc_cfg[IocID].QueueLength == 0u)) {
        return FALSE;
    }

    return TRUE;
}

boolean os_is_valid_trusted_function(TrustedFunctionIndexType FunctionIndex)
{
    if (FunctionIndex >= os_trusted_function_count) {
        return FALSE;
    }

    if ((os_trusted_function_cfg[FunctionIndex].Name == (const char*)0) ||
        (os_trusted_function_cfg[FunctionIndex].Handler == (Os_TrustedFunctionType)0)) {
        return FALSE;
    }

    return TRUE;
}

void os_recalculate_task_priority(TaskType TaskID)
{
    uint8 priority = os_task_cfg[TaskID].Priority;
    uint8 idx;

    for (idx = 0u; idx < os_tcb[TaskID].ResourceCount; idx++) {
        ResourceType resource_id = os_tcb[TaskID].ResourceStack[idx];

        if ((resource_id != INVALID_RESOURCE) &&
            (os_resource_cfg[resource_id].CeilingPriority < priority)) {
            priority = os_resource_cfg[resource_id].CeilingPriority;
        }
    }

    os_tcb[TaskID].CurrentPriority = priority;
}

void os_rebuild_ready_bitmap(void)
{
    uint8 idx;
    uint32 bitmap = 0u;

    for (idx = 0u; idx < os_task_count; idx++) {
        if (os_tcb[idx].State == READY) {
            bitmap |= ((uint32)1u << os_tcb[idx].CurrentPriority);
        }
    }

    os_ready_bitmap = bitmap;
}

void os_report_service_error(uint8 ApiId, uint8 DetErrorId, StatusType Status)
{
    Det_ReportError(OS_DET_MODULE_ID, 0u, ApiId, DetErrorId);

    if (os_error_hook != (Os_ErrorHookType)0) {
        os_error_hook(Status);
    }
}

void Os_Init(void)
{
    os_reset_runtime_state();
}

void StartOS(AppModeType Mode)
{
    uint8 idx;

    if (os_initialized == FALSE) {
        Os_Init();
    }

    os_started = TRUE;
    os_shutdown_requested = FALSE;
    os_active_app_mode = Mode;

    for (idx = 0u; idx < os_task_count; idx++) {
        uint32 mode_mask = ((uint32)1u << Mode);

        if ((os_task_cfg[idx].AutostartMask & mode_mask) != 0u) {
            (void)os_activate_task_internal(idx, FALSE);
        }
    }

    if (os_startup_hook != (Os_HookType)0) {
        os_startup_hook();
    }

    (void)os_run_ready_tasks();

#if !defined(UNIT_TEST) && !defined(OS_BOOTSTRAP_BRINGUP)
    for (;;) {
        if (os_shutdown_requested == TRUE) {
            break;
        }

        if (os_dispatch_one() == E_OS_NOFUNC) {
            /* Idle until a future port layer wakes a task. */
        }
    }

    for (;;) {
    }
#endif
}

void ShutdownOS(StatusType Error)
{
    if ((os_shutdown_requested == FALSE) && (os_shutdown_hook != (Os_ShutdownHookType)0)) {
        os_shutdown_hook(Error);
    }

    os_shutdown_requested = TRUE;
    os_shutdown_error = Error;
}

AppModeType GetActiveApplicationMode(void)
{
    return os_active_app_mode;
}

void Os_BootstrapEnterIsr2(void)
{
    os_isr_cat2_nesting++;
    Os_ServiceProtEnterIsr2();
}

void Os_BootstrapExitIsr2(void)
{
    if (os_isr_cat2_nesting > 0u) {
        os_isr_cat2_nesting--;
    }

    Os_ServiceProtExitIsr2();

    if (os_isr_cat2_nesting == 0u) {
        if (os_current_task != INVALID_TASK) {
            (void)os_maybe_dispatch_preemption();
        } else {
            (void)os_run_ready_tasks();
        }
    }
}

#if defined(UNIT_TEST) || defined(OS_BOOTSTRAP_BRINGUP)
void Os_TestReset(void)
{
    os_clear_task_cfg();
    os_clear_resource_cfg();
    os_clear_alarm_cfg();
    os_clear_application_cfg();
    os_clear_ioc_cfg();
    os_clear_stack_cfg();
    os_clear_memory_region_cfg();
    os_clear_trusted_function_cfg();
    os_clear_sched_table_cfg();
    os_task_count = 0u;
    os_resource_count = 0u;
    os_alarm_count = 0u;
    os_application_count = 0u;
    os_ioc_count = 0u;
    os_memory_region_count = 0u;
    os_trusted_function_count = 0u;
    os_sched_table_count = 0u;
    os_startup_hook = 0;
    os_error_hook = 0;
    os_pre_task_hook = 0;
    os_post_task_hook = 0;
    os_shutdown_hook = 0;
    os_counter_base.maxallowedvalue = 0xFFFFFFFFu;
    os_counter_base.ticksperbase = 1u;
    os_counter_base.mincycle = 1u;
    Os_TimingProtReset();
    Os_MemProtReset();
    Os_ServiceProtReset();
    os_protection_hook = (Os_ProtectionHookType)0;
    os_reset_runtime_state();
}
#endif

/* ================================================================== *
 * Production configuration path (S-OS-10)                             *
 *                                                                      *
 * The per-area apply functions below are the former Os_TestConfigure*  *
 * bodies, verbatim (the Os_TestConfigure* names remain available as    *
 * thin wrappers under UNIT_TEST/OS_BOOTSTRAP_BRINGUP further down).    *
 * Os_Configure() applies a full aggregate Os_ConfigType and adds the   *
 * cross-area validation the FMEA requires; see Os_Cfg_Types.h for the  *
 * entry-point rationale and the error-code mapping.                    *
 * ================================================================== */

StatusType os_cfg_apply_tasks(const Os_TaskConfigType* Config, uint8 TaskCount)
{
    uint8 idx;

    if ((Config == NULL_PTR) && (TaskCount > 0u)) {
        return E_OS_VALUE;
    }

    if (TaskCount > OS_MAX_TASKS) {
        return E_OS_LIMIT;
    }

    os_clear_task_cfg();

    for (idx = 0u; idx < TaskCount; idx++) {
        if ((Config[idx].Entry == (Os_TaskEntryType)0) ||
            (Config[idx].Priority >= OS_MAX_PRIORITIES) ||
            (Config[idx].ActivationLimit == 0u) ||
            (Config[idx].Schedule > NON)) {
            return E_OS_VALUE;
        }

        os_task_cfg[idx].Name = Config[idx].Name;
        os_task_cfg[idx].Entry = Config[idx].Entry;
        os_task_cfg[idx].Priority = Config[idx].Priority;
        os_task_cfg[idx].ActivationLimit = Config[idx].ActivationLimit;
        os_task_cfg[idx].AutostartMask = Config[idx].AutostartMask;
        os_task_cfg[idx].Extended = Config[idx].Extended;
        os_task_cfg[idx].Schedule = Config[idx].Schedule;
    }

    os_task_count = TaskCount;
    Os_Init();
    return E_OK;
}

StatusType os_cfg_apply_resources(const Os_ResourceConfigType* Config, uint8 ResourceCount)
{
    uint8 idx;

    if ((Config == NULL_PTR) && (ResourceCount > 0u)) {
        return E_OS_VALUE;
    }

    if (ResourceCount > OS_MAX_RESOURCES) {
        return E_OS_LIMIT;
    }

    os_clear_resource_cfg();

    for (idx = 0u; idx < ResourceCount; idx++) {
        if ((Config[idx].Name == (const char*)0) ||
            (Config[idx].CeilingPriority >= OS_MAX_PRIORITIES)) {
            return E_OS_VALUE;
        }

        os_resource_cfg[idx].Name = Config[idx].Name;
        os_resource_cfg[idx].CeilingPriority = Config[idx].CeilingPriority;
    }

    os_resource_count = ResourceCount;
    Os_Init();
    return E_OK;
}

StatusType os_cfg_apply_alarms(const Os_AlarmConfigType* Config, uint8 AlarmCount)
{
    uint8 idx;

    if ((Config == NULL_PTR) && (AlarmCount > 0u)) {
        return E_OS_VALUE;
    }

    if (AlarmCount > OS_MAX_ALARMS) {
        return E_OS_LIMIT;
    }

    os_clear_alarm_cfg();

    if (AlarmCount > 0u) {
        if ((Config[0].MaxAllowedValue == 0u) ||
            (Config[0].TicksPerBase == 0u) ||
            (Config[0].MinCycle == 0u)) {
            return E_OS_VALUE;
        }

        os_counter_base.maxallowedvalue = Config[0].MaxAllowedValue;
        os_counter_base.ticksperbase = Config[0].TicksPerBase;
        os_counter_base.mincycle = Config[0].MinCycle;
    }

    for (idx = 0u; idx < AlarmCount; idx++) {
        if ((Config[idx].Name == (const char*)0) ||
            (os_is_valid_task(Config[idx].TaskID) == FALSE) ||
            (Config[idx].MaxAllowedValue != os_counter_base.maxallowedvalue) ||
            (Config[idx].TicksPerBase != os_counter_base.ticksperbase) ||
            (Config[idx].MinCycle != os_counter_base.mincycle)) {
            return E_OS_VALUE;
        }

        os_alarm_cfg[idx].Name = Config[idx].Name;
        os_alarm_cfg[idx].TaskID = Config[idx].TaskID;
        os_alarm_cfg[idx].MaxAllowedValue = Config[idx].MaxAllowedValue;
        os_alarm_cfg[idx].TicksPerBase = Config[idx].TicksPerBase;
        os_alarm_cfg[idx].MinCycle = Config[idx].MinCycle;
    }

    os_alarm_count = AlarmCount;
    Os_Init();
    return E_OK;
}

StatusType os_cfg_apply_applications(const Os_ApplicationConfigType* Config, uint8 ApplicationCount)
{
    uint8 idx;

    if ((Config == NULL_PTR) && (ApplicationCount > 0u)) {
        return E_OS_VALUE;
    }

    if (ApplicationCount > OS_MAX_APPLICATIONS) {
        return E_OS_LIMIT;
    }

    os_clear_application_cfg();

    for (idx = 0u; idx < ApplicationCount; idx++) {
        if (Config[idx].Name == (const char*)0) {
            return E_OS_VALUE;
        }

        os_application_cfg[idx] = Config[idx];
    }

    os_application_count = ApplicationCount;
    Os_Init();
    return E_OK;
}

StatusType os_cfg_apply_ioc(const Os_IocConfigType* Config, uint8 IocCount)
{
    uint8 idx;

    if ((Config == NULL_PTR) && (IocCount > 0u)) {
        return E_OS_VALUE;
    }

    if (IocCount > OS_MAX_IOCS) {
        return E_OS_LIMIT;
    }

    os_clear_ioc_cfg();

    for (idx = 0u; idx < IocCount; idx++) {
        if ((Config[idx].Name == (const char*)0) ||
            (Config[idx].QueueLength == 0u) ||
            (Config[idx].QueueLength > OS_MAX_IOC_QUEUE_LENGTH)) {
            return E_OS_VALUE;
        }

        os_ioc_cfg[idx] = Config[idx];
    }

    os_ioc_count = IocCount;
    Os_Init();
    return E_OK;
}

StatusType os_cfg_apply_stacks(const Os_StackMonitorConfigType* Config, uint8 StackCount)
{
    uint8 idx;

    if ((Config == NULL_PTR) && (StackCount > 0u)) {
        return E_OS_VALUE;
    }

    if (StackCount > OS_MAX_TASKS) {
        return E_OS_LIMIT;
    }

    os_clear_stack_cfg();

    for (idx = 0u; idx < StackCount; idx++) {
        if ((os_is_valid_task(Config[idx].TaskID) == FALSE) ||
            (Config[idx].BudgetBytes == 0u)) {
            return E_OS_VALUE;
        }

        os_stack_budget_cfg[Config[idx].TaskID] = Config[idx].BudgetBytes;
    }

    Os_Init();
    return E_OK;
}

StatusType os_cfg_apply_memory_regions(const Os_MemoryRegionConfigType* Config, uint8 RegionCount)
{
    uint8 idx;

    if ((Config == NULL_PTR) && (RegionCount > 0u)) {
        return E_OS_VALUE;
    }

    if (RegionCount > OS_MAX_MEMORY_REGIONS) {
        return E_OS_LIMIT;
    }

    os_clear_memory_region_cfg();

    for (idx = 0u; idx < RegionCount; idx++) {
        if ((Config[idx].Name == (const char*)0) ||
            (os_is_valid_application(Config[idx].Application) == FALSE) ||
            (Config[idx].StartAddress == (MemoryStartAddressType)0u) ||
            (Config[idx].Size == 0u)) {
            return E_OS_VALUE;
        }

        os_memory_region_cfg[idx].Name = Config[idx].Name;
        os_memory_region_cfg[idx].Application = Config[idx].Application;
        os_memory_region_cfg[idx].StartAddress = Config[idx].StartAddress;
        os_memory_region_cfg[idx].Size = Config[idx].Size;
    }

    os_memory_region_count = RegionCount;
    Os_Init();
    return E_OK;
}

StatusType os_cfg_apply_trusted_functions(const Os_TrustedFunctionConfigType* Config,
                                            uint8 TrustedFunctionCount)
{
    uint8 idx;

    if ((Config == NULL_PTR) && (TrustedFunctionCount > 0u)) {
        return E_OS_VALUE;
    }

    if (TrustedFunctionCount > OS_MAX_TRUSTED_FUNCTIONS) {
        return E_OS_LIMIT;
    }

    os_clear_trusted_function_cfg();

    for (idx = 0u; idx < TrustedFunctionCount; idx++) {
        if ((Config[idx].Name == (const char*)0) ||
            (Config[idx].Handler == (Os_TrustedFunctionType)0)) {
            return E_OS_VALUE;
        }

        os_trusted_function_cfg[idx] = Config[idx];
    }

    os_trusted_function_count = TrustedFunctionCount;
    Os_Init();
    return E_OK;
}

/**
 * @brief   Wipe every configuration area and reset runtime state
 * @note    Fail-closed: after a wipe the kernel holds zero tasks, so a
 *          subsequent StartOS dispatches nothing.
 */
static void os_cfg_wipe(void)
{
    os_clear_task_cfg();
    os_clear_resource_cfg();
    os_clear_alarm_cfg();
    os_clear_application_cfg();
    os_clear_ioc_cfg();
    os_clear_stack_cfg();
    os_clear_memory_region_cfg();
    os_clear_trusted_function_cfg();
    os_clear_sched_table_cfg();
    os_task_count = 0u;
    os_resource_count = 0u;
    os_alarm_count = 0u;
    os_application_count = 0u;
    os_ioc_count = 0u;
    os_memory_region_count = 0u;
    os_trusted_function_count = 0u;
    os_counter_base.maxallowedvalue = 0xFFFFFFFFu;
    os_counter_base.ticksperbase = 1u;
    os_counter_base.mincycle = 1u;
    Os_MemProtReset();
    os_reset_runtime_state();
}

/**
 * @brief   Is TaskID statically allowed to use resource ResID?
 * @note    Mirrors the runtime access model (Os_Application.c): with no
 *          OS-Applications configured every task may take every
 *          resource; otherwise a task is an accessor if some
 *          application owns the task AND owns or may access the
 *          resource.
 */
static boolean os_cfg_task_is_resource_accessor(TaskType TaskID, ResourceType ResID)
{
    uint8 app;
    uint32 task_bit = ((uint32)1u << TaskID);
    uint32 resource_bit = ((uint32)1u << ResID);

    if (os_application_count == 0u) {
        return TRUE;
    }

    for (app = 0u; app < os_application_count; app++) {
        if (((os_application_cfg[app].OwnedTaskMask & task_bit) != 0u) &&
            (((os_application_cfg[app].OwnedResourceMask |
               os_application_cfg[app].AccessibleResourceMask) & resource_bit) != 0u)) {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief   FMEA OS-R-04: reject a resource ceiling numerically greater
 *          (i.e. lower priority — this kernel treats numerically lower
 *          as higher priority) than any accessor task's base priority
 * @return  E_OK or E_OS_RESOURCE
 */
static StatusType os_cfg_validate_resource_ceilings(void)
{
    uint8 res;
    uint8 task;

    for (res = 0u; res < os_resource_count; res++) {
        for (task = 0u; task < os_task_count; task++) {
            if ((os_cfg_task_is_resource_accessor(task, res) == TRUE) &&
                (os_resource_cfg[res].CeilingPriority > os_task_cfg[task].Priority)) {
                return E_OS_RESOURCE;
            }
        }
    }

    return E_OK;
}

/**
 * @brief   FMEA OS-C-03: require a configured idle task
 * @note    Idle-task convention: Priority == OS_MAX_PRIORITIES-1 (the
 *          lowest priority level), autostarted (AutostartMask != 0),
 *          Schedule == FULL (a never-terminating NON idle task would
 *          starve every other task). "Never terminates" is an
 *          Assumption of Use on the task body — not statically
 *          checkable here.
 * @return  E_OK or E_OS_NOFUNC
 */
static StatusType os_cfg_validate_idle_task(void)
{
    uint8 idx;

    for (idx = 0u; idx < os_task_count; idx++) {
        if ((os_task_cfg[idx].Priority == (uint8)(OS_MAX_PRIORITIES - 1u)) &&
            (os_task_cfg[idx].AutostartMask != 0u) &&
            (os_task_cfg[idx].Schedule == FULL)) {
            return E_OK;
        }
    }

    return E_OS_NOFUNC;
}

StatusType Os_Configure(const Os_ConfigType* Config)
{
    StatusType status;

    if (Config == NULL_PTR) {
        return E_OS_VALUE;
    }

    os_cfg_wipe();

    /* Dependency order: tasks first (alarms/stacks reference them),
     * applications before memory regions (regions reference an app). */
    status = os_cfg_apply_tasks(Config->Tasks, Config->TaskCount);

    if (status == E_OK) {
        status = os_cfg_apply_resources(Config->Resources, Config->ResourceCount);
    }

    if (status == E_OK) {
        status = os_cfg_apply_applications(Config->Applications, Config->ApplicationCount);
    }

    if (status == E_OK) {
        status = os_cfg_apply_ioc(Config->Iocs, Config->IocCount);
    }

    if (status == E_OK) {
        status = os_cfg_apply_alarms(Config->Alarms, Config->AlarmCount);
    }

    if (status == E_OK) {
        status = os_cfg_apply_stacks(Config->Stacks, Config->StackCount);
    }

    if (status == E_OK) {
        status = os_cfg_apply_memory_regions(Config->MemoryRegions, Config->MemoryRegionCount);
    }

    if (status == E_OK) {
        status = os_cfg_apply_trusted_functions(Config->TrustedFunctions, Config->TrustedFunctionCount);
    }

    /* (NULL, 0) means "no schedule tables" (already cleared by the wipe);
     * os_cfg_apply_schedule_tables itself rejects a NULL pointer. */
    if ((status == E_OK) &&
        ((Config->ScheduleTables != NULL_PTR) || (Config->ScheduleTableCount > 0u))) {
        status = os_cfg_apply_schedule_tables(Config->ScheduleTables, Config->ScheduleTableCount);
    }

    if ((status == E_OK) && (Config->MemProtTaskCount > 0u)) {
        if (Config->MemProtTasks == NULL_PTR) {
            status = E_OS_VALUE;
        } else if (Config->MemProtTaskCount > OS_MAX_TASKS) {
            status = E_OS_LIMIT;
        } else {
            uint8 idx;

            for (idx = 0u; (idx < Config->MemProtTaskCount) && (status == E_OK); idx++) {
                if (Config->MemProtTasks[idx].RegionCount > 0u) {
                    status = Os_MemProtConfigureTask((TaskType)idx,
                                                     Config->MemProtTasks[idx].Regions,
                                                     Config->MemProtTasks[idx].RegionCount);
                }
            }
        }
    }

    /* Cross-area validation (FMEA closures) */
    if (status == E_OK) {
        status = os_cfg_validate_resource_ceilings();
    }

    if (status == E_OK) {
        status = os_cfg_validate_idle_task();
    }

    if (status != E_OK) {
        /* Refuse to start: leave the kernel unconfigured (fail-closed). */
        os_cfg_wipe();
        return status;
    }

    Os_Init();
    return E_OK;
}

#if defined(UNIT_TEST) || defined(OS_BOOTSTRAP_BRINGUP)
/* Os_TestConfigure* family: thin wrappers over the production per-area
 * apply path (S-OS-10). No behavior change — the pre-existing kernel
 * suites are the regression net for this equivalence. */
StatusType Os_TestConfigureTasks(const Os_TaskConfigType* Config, uint8 TaskCount)
{
    return os_cfg_apply_tasks(Config, TaskCount);
}

StatusType Os_TestConfigureResources(const Os_ResourceConfigType* Config, uint8 ResourceCount)
{
    return os_cfg_apply_resources(Config, ResourceCount);
}

StatusType Os_TestConfigureAlarms(const Os_AlarmConfigType* Config, uint8 AlarmCount)
{
    return os_cfg_apply_alarms(Config, AlarmCount);
}

StatusType Os_TestConfigureApplications(const Os_ApplicationConfigType* Config, uint8 ApplicationCount)
{
    return os_cfg_apply_applications(Config, ApplicationCount);
}

StatusType Os_TestConfigureIoc(const Os_IocConfigType* Config, uint8 IocCount)
{
    return os_cfg_apply_ioc(Config, IocCount);
}

StatusType Os_TestConfigureStacks(const Os_StackMonitorConfigType* Config, uint8 StackCount)
{
    return os_cfg_apply_stacks(Config, StackCount);
}

StatusType Os_TestConfigureMemoryRegions(const Os_MemoryRegionConfigType* Config, uint8 RegionCount)
{
    return os_cfg_apply_memory_regions(Config, RegionCount);
}

StatusType Os_TestConfigureTrustedFunctions(const Os_TrustedFunctionConfigType* Config,
                                            uint8 TrustedFunctionCount)
{
    return os_cfg_apply_trusted_functions(Config, TrustedFunctionCount);
}

void Os_TestSetStartupHook(Os_HookType Hook)
{
    os_startup_hook = Hook;
}

void Os_TestSetErrorHook(Os_ErrorHookType Hook)
{
    os_error_hook = Hook;
}

void Os_TestSetPreTaskHook(Os_HookType Hook)
{
    os_pre_task_hook = Hook;
}

void Os_TestSetPostTaskHook(Os_HookType Hook)
{
    os_post_task_hook = Hook;
}

void Os_TestSetShutdownHook(Os_ShutdownHookType Hook)
{
    os_shutdown_hook = Hook;
}

StatusType Os_TestRunReadyTasks(void)
{
    return os_run_ready_tasks();
}

StatusType Os_TestCompletePortDispatches(void)
{
    StatusType status = E_OS_NOFUNC;

#if defined(PLATFORM_STM32) || defined(PLATFORM_STM32L5) || defined(PLATFORM_TMS570)
    while (Os_Port_CompleteConfiguredDispatch() == E_OK) {
        status = E_OK;
    }
#endif

    return status;
}

StatusType Os_TestRunToIdle(void)
{
    StatusType status = E_OS_NOFUNC;
    boolean progressed;

    do {
        progressed = FALSE;

        if (Os_TestRunReadyTasks() == E_OK) {
            status = E_OK;
            progressed = TRUE;
        }

        if (Os_TestCompletePortDispatches() == E_OK) {
            status = E_OK;
            progressed = TRUE;
        }
    } while (progressed == TRUE);

    return status;
}

TaskType Os_TestGetCurrentTask(void)
{
    return os_current_task;
}

uint32 Os_TestGetReadyBitmap(void)
{
    return os_ready_bitmap;
}

uint32 Os_TestGetDispatchCount(void)
{
    return os_dispatch_count;
}

StatusType Os_TestInvokeIsrCat2(Os_TestIsrHandlerType Handler)
{
    if (Handler == (Os_TestIsrHandlerType)0) {
        return E_OS_VALUE;
    }

#if defined(PLATFORM_TMS570)
    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();
#elif defined(PLATFORM_STM32) || defined(PLATFORM_STM32L5)
    Os_PortEnterIsr2();
#else
    Os_BootstrapEnterIsr2();
#endif
    Handler();

#if defined(PLATFORM_TMS570)
    Os_Port_Tms570_IrqNestingEnd();
    Os_Port_Tms570_IrqContextRestore();
#elif defined(PLATFORM_STM32) || defined(PLATFORM_STM32L5)
    Os_PortExitIsr2();
#else
    Os_BootstrapExitIsr2();
#endif

    return E_OK;
}

uint8 Os_TestGetIsrCat2Nesting(void)
{
    return os_isr_cat2_nesting;
}

TickType Os_TestGetCounterValue(void)
{
    return os_counter_value;
}

uint16 Os_TestGetTaskStackPeak(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return 0u;
    }

    return os_tcb[TaskID].PeakStackUsage;
}

boolean Os_TestTaskHasStackViolation(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return FALSE;
    }

    return os_tcb[TaskID].StackViolation;
}

void Os_TestSetProtectionHook(Os_ProtectionHookType Hook)
{
    os_protection_hook = Hook;
}

StatusType Os_TestSetCurrentTaskRunning(TaskType TaskID)
{
    if ((os_started == FALSE) || (os_is_valid_task(TaskID) == FALSE)) {
        return E_OS_STATE;
    }

    if ((os_current_task != INVALID_TASK) &&
        (os_current_task != TaskID) &&
        (os_tcb[os_current_task].State == RUNNING)) {
        os_tcb[os_current_task].State = READY;
        os_tcb[os_current_task].ReadyStamp = os_ready_stamp_counter++;
    }

    if (os_tcb[TaskID].PendingActivations == 0u) {
        os_tcb[TaskID].PendingActivations = 1u;
    }

    os_current_task = TaskID;
    os_tcb[TaskID].State = RUNNING;
    os_tcb[TaskID].CurrentPriority = os_task_cfg[TaskID].Priority;
    os_tcb[TaskID].ReadyStamp = 0u;
    os_rebuild_ready_bitmap();
    return E_OK;
}
#endif
