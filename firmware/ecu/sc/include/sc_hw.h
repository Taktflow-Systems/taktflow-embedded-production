/**
 * @file    sc_hw.h
 * @brief   Platform-agnostic hardware function declarations for SC
 * @date    2026-03-10
 *
 * @details Declares all hardware externs used by SC application code.
 *          Platform-specific implementations in sc_hw_tms570.c (target)
 *          and sc_hw_posix.c (SIL). Link-time selection — no #ifdef.
 *
 * @safety_req SWR-SC-025
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_HW_H
#define SC_HW_H

#include "sc_types.h"

/* ---- HALCoGen-style system init ---- */

void systemInit(void);
void gioInit(void);
void rtiInit(void);
void rtiStartCounter(void);
boolean rtiIsTickPending(void);
void rtiClearTick(void);
void gioSetDirection(uint8 port, uint8 pin, uint8 direction);
void gioSetBit(uint8 port, uint8 pin, uint8 value);
uint8 gioGetBit(uint8 port, uint8 pin);

/* ---- HALCoGen CAN init ---- */

void canInit(void);

/* ---- DCAN1 register access ---- */

uint32  dcan1_reg_read(uint32 offset);
void    dcan1_reg_write(uint32 offset, uint32 value);
boolean dcan1_get_mailbox_data(uint8 mbIndex, uint8* data, uint8* dlc);
void    dcan1_setup_mailboxes(void);
void    dcan1_transmit(uint8 mbIndex, const uint8* data, uint8 dlc);

/* ---- Debug UART (SCI) ---- */

void sc_sci_init(void);
void sc_sci_puts(const char* str);
void sc_sci_put_uint(uint32 val);
void sc_sci_put_hex32(uint32 val);

/* ---- User LEDs (GIOB[6:7]) ---- */

void sc_het_led_on(void);
void sc_het_led_off(void);
void sc_het_led_set(uint8 led2, uint8 led3);

/* ---- CCM/ESM debug snapshot ---- */

void sc_ccm_debug_get(uint32 *out);

/* ---- High-level debug functions (contain MMIO reads on target) ---- */

/**
 * @brief  Print boot-time CCM/ESM register dump (TMS570: UART, POSIX: no-op)
 */
void sc_hw_debug_boot_dump(void);

/**
 * @brief  Print periodic 5-second status (DCAN ES, CCM/ESM registers)
 */
void sc_hw_debug_periodic(void);

/* ---- Self-test hardware stubs ---- */

boolean hw_lockstep_bist(void);
boolean hw_ram_pbist(void);
boolean hw_flash_crc_check(void);
boolean hw_dcan_loopback_test(void);
boolean hw_gpio_readback_test(void);
boolean hw_lamp_test(void);
boolean hw_watchdog_test(void);
boolean hw_flash_crc_incremental(void);
boolean hw_dcan_error_check(void);

/* ---- ESM control ---- */

void    esm_enable_group1_channel(uint8 channel);
void    esm_clear_flag(uint8 group, uint8 channel);
boolean esm_is_flag_set(uint8 group, uint8 channel);

#endif /* SC_HW_H */
