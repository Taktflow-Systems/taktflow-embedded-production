/** @file sc_os_cfg.h @brief Production OSEK configuration for the SC. */
#ifndef SC_OS_CFG_H
#define SC_OS_CFG_H

#include "Os.h"
#include "Os_Cfg_Types.h"

#define SC_TASK_MAIN_ID   ((TaskType)0u)
#define SC_TASK_IDLE_ID   ((TaskType)1u)
#define SC_TASK_COUNT     ((uint8)2u)
#define SC_ALARM_MAIN_ID  ((AlarmType)0u)
#define SC_ALARM_COUNT    ((uint8)1u)

/* Numeric priority zero is the kernel's highest priority. Only this task
 * executes the verified sequence and is permitted to request a WDI feed. */
#define SC_TASK_MAIN_PRIORITY ((uint8)0u)

extern const Os_ConfigType sc_os_config;

void SC_Task_Main(void);
void SC_Task_Idle(void);

#endif
