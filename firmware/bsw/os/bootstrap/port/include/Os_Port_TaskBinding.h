/**
 * @file    Os_Port_TaskBinding.h
 * @brief   Bootstrap binding from configured OS tasks into target port contexts
 * @date    2026-03-13
 */
#ifndef OS_PORT_TASK_BINDING_H
#define OS_PORT_TASK_BINDING_H

#include "Os_Port.h"

StatusType Os_Port_PrepareConfiguredTask(TaskType TaskID, uintptr_t StackTop);
StatusType Os_Port_PrepareConfiguredFirstTask(TaskType TaskID, uintptr_t StackTop);
StatusType Os_Port_SelectConfiguredTask(TaskType TaskID);
void Os_Port_SynchronizeConfiguredTask(TaskType TaskID);
StatusType Os_Port_RequestConfiguredDispatch(TaskType TaskID);
StatusType Os_Port_CompleteConfiguredDispatch(void);
void Os_Port_ObserveConfiguredDispatch(TaskType TaskID);

#endif /* OS_PORT_TASK_BINDING_H */
