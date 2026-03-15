/**
 * @file    Os_Port.h
 * @brief   Bootstrap target-port boundary for the OSEK-first OS lab
 * @date    2026-03-13
 *
 * @details This header marks the first explicit boundary between the
 *          portable bootstrap kernel and future CPU-specific port code.
 *
 *          Verified ThreadX references for this boundary live in:
 *          - docs/reference/threadx-local-reference-map.md
 *          - firmware/bsw/os/bootstrap/port/stm32/README.md
 *          - firmware/bsw/os/bootstrap/port/tms570/README.md
 *
 *          This file is intentionally not integrated into the live build yet.
 */
#ifndef OS_PORT_H
#define OS_PORT_H

#include "Os.h"

void Os_PortTargetInit(void);
void Os_PortStartFirstTask(void);
void Os_PortRequestContextSwitch(void);
void Os_PortEnterIsr2(void);
void Os_PortExitIsr2(void);

void Os_PortTimingProtArmBudget(uint32 BudgetUs);
void Os_PortTimingProtDisarm(void);
uint32 Os_PortTimingProtElapsedUs(void);

void Os_PortMemProtInit(void);
void Os_PortMemProtConfigureRegions(const Os_MemProtRegionType* Regions, uint8 Count);
void Os_PortMemProtEnablePrivileged(void);
void Os_PortMemProtEnableUnprivileged(void);

void Os_PortDisableAllInterrupts(void);
void Os_PortEnableAllInterrupts(void);
void Os_PortSuspendOSInterrupts(void);
void Os_PortResumeOSInterrupts(void);

#endif /* OS_PORT_H */
