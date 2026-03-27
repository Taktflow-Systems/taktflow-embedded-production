/**
 * @file    tx_user.h
 * @brief   ThreadX user configuration for STM32L552ZE (Cortex-M33)
 * @date    2026-03-26
 */
#ifndef TX_USER_H
#define TX_USER_H

/* 1000Hz tick — matches BSW 1ms resolution */
#define TX_TIMER_TICKS_PER_SECOND  1000

/* Disable unused features for minimal footprint */
#define TX_DISABLE_NOTIFY_CALLBACKS

/* Enable stack checking for safety (ASIL D requires stack monitoring) */
#define TX_ENABLE_STACK_CHECKING

#endif /* TX_USER_H */
