/**
 * @file    tx_user.h
 * @brief   ThreadX user configuration for Taktflow STM32 ECUs
 *
 * Matches ST's x-cube-azrtos-g4 defaults.
 */

#ifndef TX_USER_H
#define TX_USER_H

/* Stack checking for debug builds */
#ifndef NDEBUG
#define TX_ENABLE_STACK_CHECKING
#endif

#endif /* TX_USER_H */
