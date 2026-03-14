/**
 * @file    test_Os_Port_Tms570_bootstrap_support.h
 * @brief   Shared support for TMS570 Cortex-R5 bootstrap OS port tests
 * @date    2026-03-13
 */
#ifndef TEST_OS_PORT_TMS570_BOOTSTRAP_SUPPORT_H
#define TEST_OS_PORT_TMS570_BOOTSTRAP_SUPPORT_H

#include <stdint.h>

#include "unity.h"

#include "Os.h"
#include "Os_Port_Tms570.h"
#include "Os_Port_TaskBinding.h"

#define OS_PORT_TMS570_INITIAL_FRAME_BYTES 76u
#define OS_PORT_TMS570_INITIAL_STACK_TYPE  1u
#define OS_PORT_TMS570_INITIAL_CPSR        0x13u
#define OS_PORT_TMS570_FIRST_TASK_ID       ((TaskType)0u)
#define OS_PORT_TMS570_SECOND_TASK_ID      ((TaskType)1u)
#define ALARM_ACTIVATE                     ((AlarmType)0u)

extern uint8 dummy_task_runs;
extern uint8 scheduler_bridge_low_runs;
extern uint8 scheduler_bridge_high_runs;
extern StatusType scheduler_bridge_activate_status;
extern uint8 isr_bridge_low_runs;
extern uint8 isr_bridge_high_runs;
extern StatusType isr_bridge_invoke_status;
extern StatusType isr_bridge_activate_status;

void dummy_task_entry(void);
void dummy_task_entry_alt(void);
void scheduler_bridge_high_task(void);
void scheduler_bridge_low_task(void);
void isr_bridge_high_task(void);
void isr_bridge_isr_activate_high(void);
void isr_bridge_low_task(void);
void prepare_running_first_task_for_fiq_tests(void);

extern const Os_TaskConfigType os_port_tms570_binding_tasks[2];
extern const Os_TaskConfigType os_port_tms570_scheduler_bridge_tasks[2];
extern const Os_TaskConfigType os_port_tms570_isr_bridge_tasks[2];

void setUp(void);
void tearDown(void);

void test_Os_Port_Tms570_RegisterCoreTests(void);
void test_Os_Port_Tms570_RegisterIrqTests(void);
void test_Os_Port_Tms570_RegisterFiqTests(void);
void test_Os_Port_Tms570_RegisterIntegrationTests(void);

#endif
