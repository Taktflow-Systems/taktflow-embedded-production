/**
 * @file    IoHwAb_Hil.h
 * @brief   IoHwAb HIL override API — hardware-in-the-loop sensor override
 * @author  Claude + andao
 * @date    2026-03-10
 *
 * @details Provides per-channel override API for HIL testing. When an override
 *          is active, IoHwAb_Hil reads return the override value instead of
 *          calling MCAL. When inactive, reads fall through to real hardware.
 *          Only compiled/linked in HIL builds (Makefile.hil). On target, these
 *          functions do not exist — calling them is a link error.
 *
 * @safety_req SWR-BSW-014
 * @traces_to  TSR-030, TSR-031
 *
 * @standard AUTOSAR_SWS_IOHardwareAbstraction (platform-variant pattern)
 * @copyright Taktflow Systems 2026
 */
#ifndef IOHWAB_HIL_H
#define IOHWAB_HIL_H

#include "Std_Types.h"

/* ---- HIL Override Channel IDs ---- */

#define IOHWAB_HIL_CH_PEDAL_0         0u
#define IOHWAB_HIL_CH_PEDAL_1         1u
#define IOHWAB_HIL_CH_STEERING        2u
#define IOHWAB_HIL_CH_MOTOR_CURRENT   3u
#define IOHWAB_HIL_CH_MOTOR_TEMP      4u
#define IOHWAB_HIL_CH_BATTERY         5u
#define IOHWAB_HIL_CH_BRAKE           6u
#define IOHWAB_HIL_CH_ENCODER_COUNT   7u
#define IOHWAB_HIL_CH_ENCODER_DIR     8u
#define IOHWAB_HIL_CH_ESTOP           9u
#define IOHWAB_HIL_CH_COUNT          10u  /**< Total override channels */

/* ---- Override API (HIL-only) ---- */

/**
 * @brief  Set override value for a sensor/encoder channel
 * @param  Channel   IOHWAB_HIL_CH_* identifier
 * @param  Value     Override value (engineering units)
 * @note   Invalid Channel is silently ignored. Override stays active
 *         until cleared with IoHwAb_Hil_ClearOverride().
 */
void IoHwAb_Hil_SetOverride(uint8 Channel, uint32 Value);

/**
 * @brief  Clear override for a channel — reads fall back to real MCAL
 * @param  Channel   IOHWAB_HIL_CH_* identifier
 * @note   Invalid Channel is silently ignored.
 */
void IoHwAb_Hil_ClearOverride(uint8 Channel);

/**
 * @brief  Clear all overrides — all channels fall back to real MCAL
 */
void IoHwAb_Hil_ClearAllOverrides(void);

#endif /* IOHWAB_HIL_H */
