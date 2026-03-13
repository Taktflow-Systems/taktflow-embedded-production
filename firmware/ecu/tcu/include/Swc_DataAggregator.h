/**
 * @file    Swc_DataAggregator.h
 * @brief   TCU CAN data aggregation — cache latest values with timeout
 * @date    2026-02-24
 *
 * @safety_req SWR-TCU-014
 * @traces_to  SSR-TCU-014, TSR-038
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_DATAAGGREGATOR_H
#define SWC_DATAAGGREGATOR_H

#include "Std_Types.h"

/** CAN data source indices */
#define TCU_DATA_VEHICLE_STATE   0u   /**< CAN 0x100, timeout 500ms */
#define TCU_DATA_MOTOR_CURRENT   1u   /**< CAN 0x301, timeout 500ms */
#define TCU_DATA_MOTOR_TEMP      2u   /**< CAN 0x302, timeout 2000ms */
#define TCU_DATA_BATTERY         3u   /**< CAN 0x303, timeout 2000ms */
#define TCU_DATA_DTC_BCAST       4u   /**< CAN 0x500, timeout 2000ms */
#define TCU_DATA_HB_CVC          5u   /**< CAN 0x010, timeout 200ms */
#define TCU_DATA_SOURCE_COUNT    6u

/**
 * @brief  Initialize data aggregator — clear all cached values
 */
void TCU_Data_Init(void);

/**
 * @brief  Update cache for a given data source with new CAN data
 * @param  sourceId  Data source index (TCU_DATA_xxx)
 * @param  data      Pointer to 8-byte CAN data
 * @param  dlc       Data length code
 * @return E_OK on success, E_NOT_OK on invalid source
 *
 * @safety_req SWR-TCU-014
 */
Std_ReturnType TCU_Data_Update(uint8 sourceId, const uint8* data, uint8 dlc);

/**
 * @brief  Check timeout for all data sources and flag stale data
 *
 * @safety_req SWR-TCU-014
 */
void TCU_Data_CheckTimeouts(void);

/**
 * @brief  Get cached data for a data source
 * @param  sourceId  Data source index
 * @param  data      Output buffer (8 bytes)
 * @param  isStale   Output flag: TRUE if data has timed out
 * @return E_OK if data available, E_NOT_OK if no data ever received
 */
Std_ReturnType TCU_Data_Get(uint8 sourceId, uint8* data, boolean* isStale);

#endif /* SWC_DATAAGGREGATOR_H */
