/**
 * @file    Os_Internal.h
 * @brief   Internal state for the OSEK bootstrap kernel
 * @date    2026-03-13
 */
#ifndef OS_INTERNAL_H
#define OS_INTERNAL_H

#include <stdint.h>

#include "Os.h"

#include "Det.h"

#define OS_DET_MODULE_ID               0x50u
#define OS_DET_API_ACTIVATE_TASK       0x03u
#define OS_DET_API_TERMINATE_TASK      0x04u
#define OS_DET_API_CHAIN_TASK          0x05u
#define OS_DET_API_SCHEDULE            0x06u
#define OS_DET_API_GET_TASK_ID         0x07u
#define OS_DET_API_GET_TASK_STATE      0x08u
#define OS_DET_API_GET_RESOURCE        0x09u
#define OS_DET_API_RELEASE_RESOURCE    0x0Au
#define OS_DET_API_SET_EVENT           0x0Bu
#define OS_DET_API_CLEAR_EVENT         0x0Cu
#define OS_DET_API_GET_EVENT           0x0Du
#define OS_DET_API_WAIT_EVENT          0x0Eu
#define OS_DET_API_GET_ALARM_BASE      0x0Fu
#define OS_DET_API_GET_ALARM           0x10u
#define OS_DET_API_SET_REL_ALARM       0x11u
#define OS_DET_API_SET_ABS_ALARM       0x12u
#define OS_DET_API_CANCEL_ALARM        0x13u
#define OS_DET_API_CALL_TRUSTED_FUNCTION 0x14u
#define OS_DET_API_IOC_SEND            0x15u
#define OS_DET_API_IOC_RECEIVE         0x16u
#define OS_DET_API_IOC_EMPTY_QUEUE     0x17u
#define OS_DET_API_START_SCHED_TABLE_REL  0x18u
#define OS_DET_API_START_SCHED_TABLE_ABS  0x19u
#define OS_DET_API_STOP_SCHED_TABLE       0x1Au
#define OS_DET_API_NEXT_SCHED_TABLE       0x1Bu
#define OS_DET_API_GET_SCHED_TABLE_STATUS 0x1Cu

typedef struct {
    TaskStateType State;
    uint8 PendingActivations;
    uint32 ReadyStamp;
    uint8 CurrentPriority;
    ResourceType ResourceStack[OS_MAX_RESOURCES];
    uint8 ResourceCount;
    EventMaskType SetEvents;
    EventMaskType WaitEvents;
    uintptr_t StackBaseAddr;
    uint16 PeakStackUsage;
    boolean StackViolation;
} Os_TaskControlBlockType;

typedef struct {
    boolean InUse;
    TaskType OwnerTask;
} Os_ResourceControlBlockType;

typedef struct {
    boolean Active;
    TickType AlarmTime;
    TickType Cycle;
} Os_AlarmControlBlockType;

typedef struct {
    ScheduleTableStatusType Status;
    TickType StartTick;
    TickType ElapsedTicks;
    TickType InitialDelay;
    ScheduleTableType NextTable;
} Os_ScheduleTableControlBlockType;

typedef struct {
    uint32 Buffer[OS_MAX_IOC_QUEUE_LENGTH];
    uint8 Head;
    uint8 Tail;
    uint8 Count;
} Os_IocControlBlockType;

typedef struct {
    const char* Name;
    ApplicationType Application;
    MemoryStartAddressType StartAddress;
    MemorySizeType Size;
} Os_MemoryRegionControlType;

extern Os_ApplicationConfigType os_application_cfg[OS_MAX_APPLICATIONS];
extern uint8 os_application_count;
extern Os_IocConfigType os_ioc_cfg[OS_MAX_IOCS];
extern Os_IocControlBlockType os_ioc_cb[OS_MAX_IOCS];
extern uint8 os_ioc_count;
extern uint16 os_stack_budget_cfg[OS_MAX_TASKS];
extern Os_MemoryRegionControlType os_memory_region_cfg[OS_MAX_MEMORY_REGIONS];
extern uint8 os_memory_region_count;
extern Os_TrustedFunctionConfigType os_trusted_function_cfg[OS_MAX_TRUSTED_FUNCTIONS];
extern uint8 os_trusted_function_count;

extern Os_MemProtTaskConfigType os_mem_prot_task_cfg[OS_MAX_TASKS];
extern boolean os_mem_prot_configured[OS_MAX_TASKS];

extern Os_ScheduleTableConfigType os_sched_table_cfg[OS_MAX_SCHEDULE_TABLES];
extern Os_ScheduleTableControlBlockType os_sched_table_cb[OS_MAX_SCHEDULE_TABLES];
extern uint8 os_sched_table_count;

extern Os_TaskConfigType os_task_cfg[OS_MAX_TASKS];
extern Os_TaskControlBlockType os_tcb[OS_MAX_TASKS];
extern uint8 os_task_count;
extern Os_ResourceConfigType os_resource_cfg[OS_MAX_RESOURCES];
extern Os_ResourceControlBlockType os_resource_cb[OS_MAX_RESOURCES];
extern uint8 os_resource_count;
extern Os_AlarmConfigType os_alarm_cfg[OS_MAX_ALARMS];
extern Os_AlarmControlBlockType os_alarm_cb[OS_MAX_ALARMS];
extern uint8 os_alarm_count;
extern AlarmBaseType os_counter_base;
extern TickType os_counter_value;
extern TaskType os_current_task;
extern AppModeType os_active_app_mode;
extern StatusType os_shutdown_error;
extern boolean os_initialized;
extern boolean os_started;
extern boolean os_shutdown_requested;
extern uint32 os_ready_bitmap;
extern uint32 os_ready_stamp_counter;
extern uint32 os_dispatch_count;
extern Os_HookType os_startup_hook;
extern Os_ErrorHookType os_error_hook;
extern Os_HookType os_pre_task_hook;
extern Os_HookType os_post_task_hook;
extern Os_ShutdownHookType os_shutdown_hook;
extern uint8 os_isr_cat2_nesting;
extern TaskType os_preempted_task_stack[OS_MAX_TASKS];
extern uint8 os_preempted_task_depth;
extern Os_ProtectionHookType os_protection_hook;
extern uint8 os_call_level;
extern boolean os_all_interrupts_disabled;
extern uint8 os_suspend_all_nesting;
extern uint8 os_suspend_os_nesting;

#define OS_CALLLEVEL_TASK               0u
#define OS_CALLLEVEL_ISR2               1u
#define OS_CALLLEVEL_ERROR_HOOK         2u
#define OS_CALLLEVEL_PRE_TASK_HOOK      3u
#define OS_CALLLEVEL_POST_TASK_HOOK     4u
#define OS_CALLLEVEL_STARTUP_HOOK       5u
#define OS_CALLLEVEL_SHUTDOWN_HOOK      6u
#define OS_CALLLEVEL_COUNT              7u

#define OS_LEVEL_BIT(level)     ((uint8)(1u << (level)))
#define OS_ALLOWED_TASK         OS_LEVEL_BIT(OS_CALLLEVEL_TASK)
#define OS_ALLOWED_ISR2         OS_LEVEL_BIT(OS_CALLLEVEL_ISR2)
#define OS_ALLOWED_ERROR_HOOK   OS_LEVEL_BIT(OS_CALLLEVEL_ERROR_HOOK)
#define OS_ALLOWED_PRE_TASK     OS_LEVEL_BIT(OS_CALLLEVEL_PRE_TASK_HOOK)
#define OS_ALLOWED_POST_TASK    OS_LEVEL_BIT(OS_CALLLEVEL_POST_TASK_HOOK)
#define OS_ALLOWED_STARTUP      OS_LEVEL_BIT(OS_CALLLEVEL_STARTUP_HOOK)
#define OS_ALLOWED_SHUTDOWN     OS_LEVEL_BIT(OS_CALLLEVEL_SHUTDOWN_HOOK)

#if defined(UNIT_TEST)
void os_clear_task_cfg(void);
void os_clear_resource_cfg(void);
void os_clear_alarm_cfg(void);
void os_clear_application_cfg(void);
void os_clear_ioc_cfg(void);
void os_clear_stack_cfg(void);
void os_clear_memory_region_cfg(void);
void os_clear_trusted_function_cfg(void);
void os_clear_sched_table_cfg(void);
#endif
void os_reset_runtime_state(void);
boolean os_is_valid_task(TaskType TaskID);
boolean os_is_valid_resource(ResourceType ResID);
boolean os_is_extended_task(TaskType TaskID);
boolean os_is_preemptive_task(TaskType TaskID);
boolean os_is_valid_alarm(AlarmType AlarmID);
boolean os_is_valid_application(ApplicationType ApplID);
boolean os_is_valid_ioc(IocType IocID);
boolean os_is_valid_trusted_function(TrustedFunctionIndexType FunctionIndex);
boolean os_current_application_has_access(ObjectTypeType ObjectType, uint8 ObjectID);
void os_stack_monitor_enter_task(TaskType TaskID, uintptr_t BaseAddress);
void os_stack_monitor_leave_task(TaskType TaskID);
void os_stack_monitor_sample(uintptr_t CurrentAddress, uint8 ApiId);
void os_recalculate_task_priority(TaskType TaskID);
void os_rebuild_ready_bitmap(void);
void os_report_service_error(uint8 ApiId, uint8 DetErrorId, StatusType Status);
StatusType os_activate_task_internal(TaskType TaskID, boolean AllowPreemption);
TaskType os_select_next_ready_task(void);
void os_complete_running_task(void);
StatusType os_dispatch_one(void);
StatusType os_run_ready_tasks(void);
StatusType os_maybe_dispatch_preemption(void);
void os_sched_table_process_tick(void);
boolean os_is_valid_sched_table(ScheduleTableType TableID);

void Os_ServiceProtEnterIsr2(void);
void Os_ServiceProtExitIsr2(void);
void Os_ServiceProtEnterErrorHook(void);
void Os_ServiceProtExitErrorHook(void);
void Os_ServiceProtEnterPreTaskHook(void);
void Os_ServiceProtExitPreTaskHook(void);
void Os_ServiceProtEnterPostTaskHook(void);
void Os_ServiceProtExitPostTaskHook(void);
void Os_ServiceProtEnterStartupHook(void);
void Os_ServiceProtExitStartupHook(void);
void Os_ServiceProtEnterShutdownHook(void);
void Os_ServiceProtExitShutdownHook(void);

#define OS_STACK_SAMPLE(ApiId)                 \
    do {                                      \
        uint8 os_stack_marker = 0u;           \
        os_stack_monitor_sample((uintptr_t)&os_stack_marker, (ApiId)); \
    } while (0)

#endif /* OS_INTERNAL_H */
