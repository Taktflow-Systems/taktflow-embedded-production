/**
 * @file    tx_user.h
 * @brief   ThreadX user configuration for Taktflow STM32 ECUs
 *
 * Configured for CMSIS-RTOS2 wrapper compatibility.
 */

#ifndef TX_USER_H
#define TX_USER_H

/* Required by ST CMSIS-RTOS2 wrapper (cmsis_os2.c) */
#define TX_THREAD_USER_EXTENSION    ULONG tx_thread_detached_joinable;

/* Stack checking for debug builds */
#ifndef NDEBUG
#define TX_ENABLE_STACK_CHECKING
#endif

#endif /* TX_USER_H */
