/**
 * @file    Sc_Hw_Cfg.h
 * @brief   Safety Controller configuration — all SC-specific constants
 * @date    2026-02-23
 *
 * @details Unified configuration header for the Safety Controller.
 *          Contains CAN mailbox IDs, E2E Data IDs, timing constants,
 *          GIO pin assignments, threshold values, and state enums.
 *
 * @safety_req SWR-SC-001 to SWR-SC-026
 * @traces_to  SSR-SC-001 to SSR-SC-017
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_CFG_H
#define SC_CFG_H

#include "sc_types.h"

/* Platform-specific constants (selected via -I path in Makefile) */
#include "Sc_Cfg_Platform.h"

/* ==================================================================
 * CAN Message IDs (DCAN1 receive mailboxes)
 * ================================================================== */

#define SC_CAN_ID_ESTOP             0x001u   /* E-Stop (CVC) */
#define SC_CAN_ID_CVC_HB            0x010u   /* CVC heartbeat */
#define SC_CAN_ID_FZC_HB            0x011u   /* FZC heartbeat */
#define SC_CAN_ID_RZC_HB            0x012u   /* RZC heartbeat */
#define SC_CAN_ID_VEHICLE_STATE     0x100u   /* Vehicle_State (torque in byte 4) */
#define SC_CAN_ID_MOTOR_CURRENT     0x301u   /* Motor_Current (RZC) */
#define SC_CAN_ID_RELAY_STATUS      0x013u   /* SC_Relay_Status (SIL broadcast) */

/* ==================================================================
 * CAN Mailbox Numbers (1-indexed, DCAN1 hardware)
 * ================================================================== */

#define SC_MB_ESTOP                 1u
#define SC_MB_CVC_HB                2u
#define SC_MB_FZC_HB                3u
#define SC_MB_RZC_HB                4u
#define SC_MB_VEHICLE_STATE         5u
#define SC_MB_MOTOR_CURRENT         6u
#define SC_MB_COUNT                 6u

/* Mailbox index (0-based for arrays) */
#define SC_MB_IDX_ESTOP             0u
#define SC_MB_IDX_CVC_HB            1u
#define SC_MB_IDX_FZC_HB            2u
#define SC_MB_IDX_RZC_HB            3u
#define SC_MB_IDX_VEHICLE_STATE     4u
#define SC_MB_IDX_MOTOR_CURRENT     5u

/* ==================================================================
 * E2E Data IDs (per message type)
 * ================================================================== */

#define SC_E2E_ESTOP_DATA_ID        0x01u
#define SC_E2E_CVC_HB_DATA_ID      0x02u
#define SC_E2E_FZC_HB_DATA_ID      0x03u
#define SC_E2E_RZC_HB_DATA_ID      0x04u
#define SC_E2E_VEHSTATE_DATA_ID    0x05u
#define SC_E2E_MOTORCUR_DATA_ID    0x0Fu

/* ==================================================================
 * CRC-8 Parameters (SAE-J1850)
 * ================================================================== */

#define SC_CRC8_POLY                0x1Du
#define SC_CRC8_INIT                0xFFu

/* ==================================================================
 * Heartbeat Timing (in 10ms ticks)
 * ================================================================== */

/* SC_HB_TIMEOUT_TICKS defined in Sc_Cfg_Platform.h */
/* SC_HB_CONFIRM_TICKS defined in Sc_Cfg_Platform.h */
#define SC_HB_RECOVERY_THRESHOLD    3u     /* 3 consecutive HBs before canceling timeout */
#define SC_HB_ALIVE_MAX             15u    /* 4-bit alive counter max */
/* SC_HB_STARTUP_GRACE_TICKS defined in Sc_Cfg_Platform.h */

/* ==================================================================
 * Bus Silence Monitoring
 * ================================================================== */

#define SC_BUS_SILENCE_TICKS        20u    /* 200ms = all-heartbeat timeout */

/* ==================================================================
 * Plausibility Thresholds
 * ================================================================== */

#define SC_PLAUS_REL_THRESHOLD      20u    /* 20% relative difference */
#define SC_PLAUS_ABS_THRESHOLD_MA   2000u  /* 2000 mA absolute (near-zero) */
/* SC_PLAUS_DEBOUNCE_TICKS defined in Sc_Cfg_Platform.h */

/* ==================================================================
 * Backup Cutoff (SWR-SC-024)
 * ================================================================== */

#define SC_BACKUP_CUTOFF_CURRENT_MA 1000u  /* 1000 mA threshold */
#define SC_BACKUP_CUTOFF_TICKS      10u    /* 100ms timeout */

/* ==================================================================
 * Creep Guard Thresholds (SSR-SC-018, SM-024)
 *
 * Standstill torque cross-plausibility: if torque_pct == 0 and
 * motor current > threshold for debounce cycles, kill relay.
 * Detects BTS7960 FET short (MB-006) — motor activates despite
 * zero torque command.
 * ================================================================== */

#define SC_CREEP_CURRENT_THRESH     500u   /* 500 mA — above this with zero torque = fault */
#define SC_CREEP_DEBOUNCE_CYCLES    2u     /* 2 × 10ms = 20ms debounce (< 50ms FTTI) */

/* ==================================================================
 * GIO Pin Assignments
 * ================================================================== */

#define SC_GIO_PORT_A               0u
#define SC_GIO_PORT_B               1u

#define SC_PIN_RELAY                0u     /* GIO_A0: Kill relay output */
#define SC_PIN_LED_CVC              1u     /* GIO_A1: CVC fault LED */
#define SC_PIN_LED_FZC              2u     /* GIO_A2: FZC fault LED */
#define SC_PIN_LED_RZC              3u     /* GIO_A3: RZC fault LED */
#define SC_PIN_LED_SYS              4u     /* GIO_A4: System fault LED (amber) */
#define SC_PIN_WDI                  5u     /* GIO_A5: TPS3823 watchdog input */
#define SC_PIN_LED_HB               1u     /* GIO_B1: Heartbeat LED (onboard) */

/* ==================================================================
 * LED Blink Timing (in 10ms ticks)
 * ================================================================== */

#define SC_LED_BLINK_ON_TICKS       25u    /* 250ms on */
#define SC_LED_BLINK_OFF_TICKS      25u    /* 250ms off = 500ms period */
#define SC_LED_BLINK_PERIOD         50u    /* Total blink period in ticks */

/* ==================================================================
 * Stack Canary
 * ================================================================== */

#define SC_STACK_CANARY_VALUE       0xDEADBEEFu
#define SC_STACK_CANARY_ADDR        0x08000420u

/* ==================================================================
 * RAM Self-Test
 * ================================================================== */

#define SC_RAM_TEST_ADDR            0x08000400u
#define SC_RAM_TEST_SIZE            32u    /* 32 bytes */

/* ==================================================================
 * Watchdog Conditions Bitmask
 * ================================================================== */

#define SC_WDG_COND_MONITOR         0x01u  /* Monitoring functions executed */
#define SC_WDG_COND_RAM_OK          0x02u  /* RAM pattern intact */
#define SC_WDG_COND_CAN_OK          0x04u  /* DCAN1 not bus-off */
#define SC_WDG_COND_ESM_OK          0x08u  /* ESM error not asserted */
#define SC_WDG_COND_STACK_OK        0x10u  /* Stack canary intact */
#define SC_WDG_COND_ALL             0x1Fu  /* All conditions met */

/* ==================================================================
 * E2E Failure Threshold
 * ================================================================== */

#define SC_E2E_MAX_CONSEC_FAIL      3u     /* 3 consecutive E2E failures */

/* ==================================================================
 * Relay Readback
 * ================================================================== */

#define SC_RELAY_READBACK_THRESHOLD 2u     /* 2 consecutive mismatches */

/* ==================================================================
 * Self-Test Runtime Period
 * ================================================================== */

#define SC_SELFTEST_RUNTIME_PERIOD  6000u  /* 60s = 6000 ticks at 10ms */
#define SC_SELFTEST_RUNTIME_STEPS   4u     /* 4 steps spread across ticks */

/* ==================================================================
 * ECU Index Enum
 * ================================================================== */

#define SC_ECU_CVC                  0u
#define SC_ECU_FZC                  1u
#define SC_ECU_RZC                  2u
#define SC_ECU_COUNT                3u

/* ==================================================================
 * SC State Enum
 * ================================================================== */

#define SC_STATE_INIT               0u
#define SC_STATE_MONITORING         1u
#define SC_STATE_FAULT              2u
#define SC_STATE_KILL               3u

/* ==================================================================
 * Heartbeat Content Validation Thresholds (SWR-SC-027, SWR-SC-028)
 *
 * Both counters count heartbeat receptions (~50ms each).
 * STUCK_DEGRADED: 100 × 50ms = 5 s stuck in DEGRADED/LIMP.
 * FAULT_ESCALATE:  20 × 50ms = 1 s with >=2 FaultStatus bits.
 * ================================================================== */

#define SC_HB_STUCK_DEGRADED_MAX    100u   /* receptions in DEGRADED/LIMP -> content fault */
#define SC_HB_FAULT_ESCALATE_MAX     20u   /* receptions with >=2 fault bits -> content fault */

/* ==================================================================
 * SC_Status TX Period (SWR-SC-030)
 * ================================================================== */

#define SC_MONITORING_TX_PERIOD      50u   /* 50 × 10ms = 500ms SC_Status broadcast */

/* ==================================================================
 * SC_Status Byte 2: Mode (low nibble) and Fault Flags (high nibble)
 * ================================================================== */

#define SC_STATUS_MODE_INIT          0u
#define SC_STATUS_MODE_MONITORING    1u
#define SC_STATUS_MODE_FAULT         2u
#define SC_STATUS_MODE_SAFE_STOP     3u

#define SC_STATUS_FAULT_CVC_HB       0x01u
#define SC_STATUS_FAULT_FZC_HB       0x02u
#define SC_STATUS_FAULT_RZC_HB       0x04u
#define SC_STATUS_FAULT_PLAUS        0x08u

/* ==================================================================
 * SC_Status Byte 3: FaultReason (bits [6:3]) — 4-bit enum
 *
 * Direct pass-through of SC_KILL_REASON_* values (GAP-SC-007 fix).
 * Previously used bitmask encoding that collapsed multiple reasons.
 * Values match SC_KILL_REASON_* for 1:1 post-incident diagnosability.
 * ================================================================== */

#define SC_STATUS_REASON_NONE        0x00u  /* = SC_KILL_REASON_NONE */
#define SC_STATUS_REASON_HB_TIMEOUT  0x01u  /* = SC_KILL_REASON_HB_TIMEOUT */
#define SC_STATUS_REASON_PLAUS       0x02u  /* = SC_KILL_REASON_PLAUSIBILITY */
#define SC_STATUS_REASON_SELFTEST    0x03u  /* = SC_KILL_REASON_SELFTEST */
#define SC_STATUS_REASON_ESM         0x04u  /* = SC_KILL_REASON_ESM */
#define SC_STATUS_REASON_BUSOFF      0x05u  /* = SC_KILL_REASON_BUSOFF */
#define SC_STATUS_REASON_READBACK    0x06u  /* = SC_KILL_REASON_READBACK */
#define SC_STATUS_REASON_ESTOP       0x07u  /* = SC_KILL_REASON_ESTOP */
#define SC_STATUS_REASON_BUS_SILENCE 0x08u  /* = SC_KILL_REASON_BUS_SILENCE */
#define SC_STATUS_REASON_E2E_FAIL    0x09u  /* = SC_KILL_REASON_E2E_FAIL */
#define SC_STATUS_REASON_CREEP_GUARD 0x0Au  /* = SC_KILL_REASON_CREEP_GUARD */

/* ==================================================================
 * TX Mailbox for SC_Status (SWR-SC-029) — firmware-only TX mailbox
 * ================================================================== */

#define SC_MB_TX_STATUS              7u    /* DCAN1 mailbox 7: TX only, CAN ID 0x013 */

/* ==================================================================
 * Torque Lookup Table Size
 * ================================================================== */

#define SC_TORQUE_LUT_SIZE          16u

/* ==================================================================
 * CAN DLC (standard 8 bytes)
 * ================================================================== */

#define SC_CAN_DLC                  8u
#define SC_RELAY_STATUS_DLC         4u     /* Relay status message: 4 bytes */

/* ==================================================================
 * SIL Relay Broadcast Period (in 10ms ticks)
 * ================================================================== */

#define SC_RELAY_BROADCAST_TICKS    5u     /* 50ms = 5 x 10ms ticks */

/* ==================================================================
 * Kill Reason Enum (for relay status broadcast)
 * ================================================================== */

#define SC_KILL_REASON_NONE         0u
#define SC_KILL_REASON_HB_TIMEOUT   1u
#define SC_KILL_REASON_PLAUSIBILITY 2u
#define SC_KILL_REASON_SELFTEST     3u
#define SC_KILL_REASON_ESM          4u
#define SC_KILL_REASON_BUSOFF       5u
#define SC_KILL_REASON_READBACK     6u
#define SC_KILL_REASON_ESTOP        7u
#define SC_KILL_REASON_BUS_SILENCE  8u
#define SC_KILL_REASON_E2E_FAIL    9u
#define SC_KILL_REASON_CREEP_GUARD 10u  /* SSR-SC-018: standstill torque cross-plausibility */

/* ==================================================================
 * Fault Source Enum (for relay status broadcast)
 * ================================================================== */

#define SC_FAULT_SOURCE_NONE        0u
#define SC_FAULT_SOURCE_CVC         1u
#define SC_FAULT_SOURCE_FZC         2u
#define SC_FAULT_SOURCE_RZC         3u

/* ==================================================================
 * DCAN1 Baud Rate Configuration (500 kbps from 75 MHz VCLK1)
 *
 * DCAN BTR register uses +1 encoding: actual = field + 1.
 * Bit rate  = VCLK1 / ((BRP+1) * (1 + (TSEG1+1) + (TSEG2+1)))
 *           = 75 MHz / (10 * 15) = 500,000 bps
 * Sample pt = (1 + (TSEG1+1)) / 15 = 12/15 = 80%
 *
 * SJW field is 2 bits (bits 6-7 of BTR), max field value = 3
 * (actual SJW = 4). Values > 3 overflow into TSEG1 field.
 *
 * SJW >= 4 required for internal oscillator tolerance (HSI ±1%
 * per chip = ±2% relative between two nodes). Lesson from STM32
 * CAN bring-up: SJW=1 fails at 500 kbps with HSI clocks.
 *
 * NOTE: Verify VCLK1 in HALCoGen clock tree after project creation.
 * If VCLK1 differs from 75 MHz, recalculate BRP.
 * ================================================================== */

#define SC_DCAN_BRP                 9u     /* Baud rate prescaler (BRP+1=10, Tq=7.5MHz) */
#define SC_DCAN_TSEG1               10u    /* Time segment 1 (TSEG1+1=11) */
#define SC_DCAN_TSEG2               2u     /* Time segment 2 (TSEG2+1=3) */
#define SC_DCAN_SJW                 3u     /* Sync jump width (SJW+1=4, max for 2-bit field) */

#endif /* SC_CFG_H */
