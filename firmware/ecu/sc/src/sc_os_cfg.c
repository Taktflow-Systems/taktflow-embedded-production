/** @file sc_os_cfg.c @brief Production alarm-driven SC OSEK configuration. */
#include "sc_os_cfg.h"

#define SC_OS_STACK_BYTES 2048u

static const Os_TaskConfigType sc_tasks[SC_TASK_COUNT] = {
    { "SC_Safety", SC_Task_Main, SC_TASK_MAIN_PRIORITY, 1u, 0u, FALSE, NON },
    { "SC_Idle", SC_Task_Idle, (uint8)(OS_MAX_PRIORITIES - 1u), 1u,
      ((uint32)1u << OSDEFAULTAPPMODE), FALSE, FULL },
};

static const Os_AlarmConfigType sc_alarms[SC_ALARM_COUNT] = {
    { "SC_10ms", SC_TASK_MAIN_ID, 0xFFFFFFFFu, 1u, 1u },
};

static const Os_StackMonitorConfigType sc_stack_monitors[SC_TASK_COUNT] = {
    { SC_TASK_MAIN_ID, SC_OS_STACK_BYTES },
    { SC_TASK_IDLE_ID, SC_OS_STACK_BYTES },
};

static uint8 sc_main_stack[SC_OS_STACK_BYTES] __attribute__((aligned(8)));
static uint8 sc_idle_stack[SC_OS_STACK_BYTES] __attribute__((aligned(8)));

static const Os_TaskStackConfigType sc_task_stacks[SC_TASK_COUNT] = {
    { SC_TASK_MAIN_ID, sc_main_stack, SC_OS_STACK_BYTES },
    { SC_TASK_IDLE_ID, sc_idle_stack, SC_OS_STACK_BYTES },
};

const Os_ConfigType sc_os_config = {
    .Tasks = sc_tasks, .TaskCount = SC_TASK_COUNT,
    .Resources = NULL_PTR, .ResourceCount = 0u,
    .Alarms = sc_alarms, .AlarmCount = SC_ALARM_COUNT,
    .ScheduleTables = NULL_PTR, .ScheduleTableCount = 0u,
    .Applications = NULL_PTR, .ApplicationCount = 0u,
    .TrustedFunctions = NULL_PTR, .TrustedFunctionCount = 0u,
    .Iocs = NULL_PTR, .IocCount = 0u,
    .Stacks = sc_stack_monitors, .StackCount = SC_TASK_COUNT,
    .MemoryRegions = NULL_PTR, .MemoryRegionCount = 0u,
    .MemProtTasks = NULL_PTR, .MemProtTaskCount = 0u,
    .TaskStacks = sc_task_stacks, .TaskStackCount = SC_TASK_COUNT,
};
