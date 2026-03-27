/**
 * @file    bsw_stubs.c
 * @brief   Minimal stubs — Step 1 (LED + UART only)
 * @date    2026-03-26
 *
 * All BSW modules stubbed until CAN is integrated (Step 3+).
 */
#include "Std_Types.h"

/* Os stubs — not using bootstrap OS */
void Os_Init(void) {}
void StartOS(uint8 mode) { (void)mode; }
