/**
 * @file    sc_os_cfg.h
 * @brief   SC OS configuration — task and alarm IDs, init function
 * @date    2026-03-14
 *
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_OS_CFG_H
#define SC_OS_CFG_H

#define SC_TASK_MAIN_ID    0u
#define SC_ALARM_MAIN_ID   0u

/**
 * @brief  Populate kernel tables with SC task/alarm configuration.
 * @note   Must be called before Os_Init() / StartOS().
 */
void SC_Os_Configure(void);

/**
 * @brief  SC main task entry — alarm-driven, runs every 10ms tick.
 * @note   Run-to-completion: does monitoring work then TerminateTask().
 */
void SC_Task_Main(void);

#endif /* SC_OS_CFG_H */
