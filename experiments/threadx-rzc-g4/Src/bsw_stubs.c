/**
 * @file    bsw_stubs.c
 * @brief   Minimal stubs — only OS functions not used with ThreadX
 */
#include "Std_Types.h"

/* Os stubs — not using bootstrap OS */
void Os_Init(void) {}
void StartOS(uint8 mode) { (void)mode; }
