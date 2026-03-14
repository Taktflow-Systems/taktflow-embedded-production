/**
 * @file    Os.h
 * @brief   OSEK-first OS starter kernel
 * @date    2026-03-13
 *
 * @details Current bootstrap milestone of the OSEK bootstrap kernel:
 *          - static task configuration
 *          - application modes
 *          - priority-based ready selection
 *          - host-testable dispatch semantics
 *          - BCC2-style queued activations
 *          - FULL/NON task scheduling behavior
 *          - bootstrap PCP resources
 *          - first ECC1 event services for extended tasks
 *          - bootstrap counters and task-activation alarms
 *          - startup/error/pre/post/shutdown hook coverage
 *          - OS-Application ownership/access queries
 *          - bootstrap trusted-function dispatch checks
 *          - bootstrap IOC queue communication
 *          - bootstrap stack-budget monitoring
 *          - bootstrap task memory-access queries by OS-Application
 *
 *          The current implementation is a host-testable bootstrap with
 *          static priorities, bootstrap-form preemption, and PCP resource
 *          handling before real STM32/TMS570 context switch ports arrive.
 *
 * @safety_req SWR-BSW-050: Static priority task scheduling
 * @traces_to  TSR-OS-001, TSR-OS-002
 *
 * @standard OSEK/VDX, AUTOSAR_CP_OS, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef OS_H
#define OS_H

#include <stdint.h>

#include "Std_Types.h"

typedef uint8 StatusType;
typedef uint8 TaskType;
typedef TaskType* TaskRefType;
typedef uint8 AppModeType;
typedef uint8 ApplicationType;
typedef uint8 IocType;
typedef uint8 TaskStateType;
typedef TaskStateType* TaskStateRefType;
typedef uint8 Os_TaskScheduleType;
typedef uint8 ResourceType;
typedef uint8 AlarmType;
typedef uint8 ObjectTypeType;
typedef uint8 ObjectAccessType;
typedef uint8 AccessType;
typedef uint8 TrustedFunctionIndexType;
typedef uint32 EventMaskType;
typedef EventMaskType* EventMaskRefType;
typedef uint32 TickType;
typedef TickType* TickRefType;
typedef uintptr_t MemoryStartAddressType;
typedef uint32 MemorySizeType;
typedef void (*Os_TaskEntryType)(void);
typedef void (*Os_HookType)(void);
typedef void (*Os_ErrorHookType)(StatusType Error);
typedef void (*Os_ShutdownHookType)(StatusType Error);
typedef void* TrustedFunctionParameterRefType;
typedef StatusType (*Os_TrustedFunctionType)(TrustedFunctionParameterRefType Params);
#if defined(UNIT_TEST)
typedef void (*Os_TestIsrHandlerType)(void);
#endif

typedef struct {
    const char* Name;
    Os_TaskEntryType Entry;
    uint8 Priority;
    uint8 ActivationLimit;
    uint32 AutostartMask;
    boolean Extended;
    Os_TaskScheduleType Schedule;
} Os_TaskConfigType;

typedef struct {
    const char* Name;
    uint8 CeilingPriority;
} Os_ResourceConfigType;

typedef struct {
    TickType maxallowedvalue;
    TickType ticksperbase;
    TickType mincycle;
} AlarmBaseType;

typedef AlarmBaseType* AlarmBaseRefType;

typedef struct {
    const char* Name;
    TaskType TaskID;
    TickType MaxAllowedValue;
    TickType TicksPerBase;
    TickType MinCycle;
} Os_AlarmConfigType;

typedef struct {
    const char* Name;
    boolean Trusted;
    uint32 OwnedTaskMask;
    uint32 AccessibleTaskMask;
    uint32 OwnedResourceMask;
    uint32 AccessibleResourceMask;
    uint32 OwnedAlarmMask;
    uint32 AccessibleAlarmMask;
    uint32 OwnedIocMask;
    uint32 AccessibleIocMask;
} Os_ApplicationConfigType;

typedef struct {
    const char* Name;
    Os_TrustedFunctionType Handler;
    uint32 AccessibleApplicationMask;
} Os_TrustedFunctionConfigType;

typedef struct {
    const char* Name;
    uint8 QueueLength;
} Os_IocConfigType;

typedef struct {
    TaskType TaskID;
    uint16 BudgetBytes;
} Os_StackMonitorConfigType;

typedef struct {
    const char* Name;
    ApplicationType Application;
    MemoryStartAddressType StartAddress;
    MemorySizeType Size;
} Os_MemoryRegionConfigType;

#define OS_MAX_TASKS            8u
#define OS_MAX_RESOURCES        8u
#define OS_MAX_ALARMS           8u
#define OS_MAX_APPLICATIONS     4u
#define OS_MAX_IOCS             8u
#define OS_MAX_IOC_QUEUE_LENGTH 4u
#define OS_MAX_MEMORY_REGIONS   8u
#define OS_MAX_TRUSTED_FUNCTIONS 8u
#define OS_MAX_PRIORITIES       8u
#define INVALID_TASK            ((TaskType)0xFFu)
#define INVALID_RESOURCE        ((ResourceType)0xFFu)
#define INVALID_ALARM           ((AlarmType)0xFFu)
#define INVALID_IOC             ((IocType)0xFFu)
#define INVALID_OSAPPLICATION   ((ApplicationType)0xFFu)
#define OSDEFAULTAPPMODE        ((AppModeType)0u)

#define SUSPENDED               ((TaskStateType)0u)
#define READY                   ((TaskStateType)1u)
#define RUNNING                 ((TaskStateType)2u)
#define WAITING                 ((TaskStateType)3u)

#define FULL                    ((Os_TaskScheduleType)0u)
#define NON                     ((Os_TaskScheduleType)1u)

#define OBJECT_TASK             ((ObjectTypeType)0u)
#define OBJECT_RESOURCE         ((ObjectTypeType)1u)
#define OBJECT_ALARM            ((ObjectTypeType)2u)
#define OBJECT_IOC              ((ObjectTypeType)3u)

#define ACCESS                  ((ObjectAccessType)1u)
#define NO_ACCESS               ((ObjectAccessType)0u)

#define E_OS_ACCESS             ((StatusType)0x01u)
#define E_OS_CALLEVEL           ((StatusType)0x02u)
#define E_OS_ID                 ((StatusType)0x03u)
#define E_OS_LIMIT              ((StatusType)0x04u)
#define E_OS_NOFUNC             ((StatusType)0x05u)
#define E_OS_RESOURCE           ((StatusType)0x06u)
#define E_OS_STATE              ((StatusType)0x07u)
#define E_OS_VALUE              ((StatusType)0x08u)
#define IOC_E_LIMIT             ((StatusType)0x21u)
#define IOC_E_NO_DATA           ((StatusType)0x22u)

void Os_Init(void);
void StartOS(AppModeType Mode);
void ShutdownOS(StatusType Error);

StatusType ActivateTask(TaskType TaskID);
StatusType TerminateTask(void);
StatusType ChainTask(TaskType TaskID);
StatusType Schedule(void);
StatusType GetTaskID(TaskRefType TaskID);
StatusType GetTaskState(TaskType TaskID, TaskStateRefType State);
StatusType SetEvent(TaskType TaskID, EventMaskType Mask);
StatusType ClearEvent(EventMaskType Mask);
StatusType GetEvent(TaskType TaskID, EventMaskRefType Event);
StatusType WaitEvent(EventMaskType Mask);
StatusType GetResource(ResourceType ResID);
StatusType ReleaseResource(ResourceType ResID);
StatusType GetAlarmBase(AlarmType AlarmID, AlarmBaseRefType Info);
StatusType GetAlarm(AlarmType AlarmID, TickRefType Tick);
StatusType SetRelAlarm(AlarmType AlarmID, TickType increment, TickType cycle);
StatusType SetAbsAlarm(AlarmType AlarmID, TickType start, TickType cycle);
StatusType CancelAlarm(AlarmType AlarmID);
StatusType IocSend(IocType IocID, uint32 Data);
StatusType IocReceive(IocType IocID, uint32* Data);
StatusType IocEmptyQueue(IocType IocID);
AppModeType GetActiveApplicationMode(void);
ApplicationType GetApplicationID(void);
ApplicationType GetCurrentApplicationID(void);
ObjectAccessType CheckObjectAccess(ApplicationType ApplID, ObjectTypeType ObjectType, uint8 ObjectID);
ApplicationType CheckObjectOwnership(ObjectTypeType ObjectType, uint8 ObjectID);
AccessType CheckTaskMemoryAccess(TaskType TaskID, MemoryStartAddressType Address, MemorySizeType Size);
StatusType CallTrustedFunction(TrustedFunctionIndexType FunctionIndex, TrustedFunctionParameterRefType FunctionParams);
boolean Os_BootstrapProcessCounterTick(void);
void Os_BootstrapEnterIsr2(void);
void Os_BootstrapExitIsr2(void);

#if defined(UNIT_TEST)
void Os_TestReset(void);
StatusType Os_TestConfigureTasks(const Os_TaskConfigType* Config, uint8 TaskCount);
StatusType Os_TestConfigureResources(const Os_ResourceConfigType* Config, uint8 ResourceCount);
StatusType Os_TestConfigureAlarms(const Os_AlarmConfigType* Config, uint8 AlarmCount);
StatusType Os_TestConfigureApplications(const Os_ApplicationConfigType* Config, uint8 ApplicationCount);
StatusType Os_TestConfigureIoc(const Os_IocConfigType* Config, uint8 IocCount);
StatusType Os_TestConfigureStacks(const Os_StackMonitorConfigType* Config, uint8 StackCount);
StatusType Os_TestConfigureMemoryRegions(const Os_MemoryRegionConfigType* Config, uint8 RegionCount);
StatusType Os_TestConfigureTrustedFunctions(const Os_TrustedFunctionConfigType* Config, uint8 TrustedFunctionCount);
void Os_TestSetStartupHook(Os_HookType Hook);
void Os_TestSetErrorHook(Os_ErrorHookType Hook);
void Os_TestSetPreTaskHook(Os_HookType Hook);
void Os_TestSetPostTaskHook(Os_HookType Hook);
void Os_TestSetShutdownHook(Os_ShutdownHookType Hook);
StatusType Os_TestRunReadyTasks(void);
StatusType Os_TestCompletePortDispatches(void);
StatusType Os_TestRunToIdle(void);
TaskType Os_TestGetCurrentTask(void);
StatusType Os_TestSetCurrentTaskRunning(TaskType TaskID);
uint32 Os_TestGetReadyBitmap(void);
uint8 Os_TestGetPendingActivations(TaskType TaskID);
uint32 Os_TestGetDispatchCount(void);
StatusType Os_TestInvokeIsrCat2(Os_TestIsrHandlerType Handler);
uint8 Os_TestGetIsrCat2Nesting(void);
void Os_TestAdvanceCounter(TickType Ticks);
TickType Os_TestGetCounterValue(void);
uint16 Os_TestGetTaskStackPeak(TaskType TaskID);
boolean Os_TestTaskHasStackViolation(TaskType TaskID);
#endif

#endif /* OS_H */
