/**
 * @file    tx_user.h
 * @brief   ThreadX user configuration for Taktflow STM32 ECUs
 *
 * Configured for ST CMSIS-RTOS2 wrapper compatibility.
 * Must match x-cube-azrtos-g4 requirements.
 */

#ifndef TX_USER_H
#define TX_USER_H

/* Required by ST CMSIS-RTOS2 wrapper */
#define TX_THREAD_USER_EXTENSION    ULONG tx_thread_detached_joinable;
#define TX_MAX_PRIORITIES           64
#define USE_DYNAMIC_MEMORY_ALLOCATION

/* Timer thread stack — default 1024 is too small for Rte_MainFunction()
 * which dispatches all SWC runnables (Brake, Steering, Lidar, Safety, etc.).
 * FZC has 8+ runnables, each using ~100-200 bytes of stack. */
#define TX_TIMER_THREAD_STACK_SIZE  8192

/* Stack checking for debug builds */
#ifndef NDEBUG
#define TX_ENABLE_STACK_CHECKING
#endif

#endif /* TX_USER_H */
