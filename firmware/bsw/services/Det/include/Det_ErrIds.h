/**
 * @file    Det_ErrIds.h
 * @brief   Debug runtime error IDs for Det_ReportRuntimeError
 * @author  Claude + andao
 * @date    2026-03-10
 *
 * @details ECU-level module IDs, API IDs, and debug error IDs used by
 *          main.c files to report init progress, self-test results, and
 *          state transitions via the structured Det API. The Det callout
 *          (Det_Callout_Sil.c) maps these to human-readable text on POSIX.
 *          On target, they are stored in the Det ring buffer.
 *
 * @standard AUTOSAR_SWS_DefaultErrorTracer (extended)
 * @copyright Taktflow Systems 2026
 */
#ifndef DET_ERRIDS_H
#define DET_ERRIDS_H

/* ---- ECU Application Module IDs (0x80+ to avoid BSW module ID collision) ---- */

#define DET_MODULE_CVC_MAIN     0x80u  /**< CVC main application */
#define DET_MODULE_FZC_MAIN     0x81u  /**< FZC main application */
#define DET_MODULE_RZC_MAIN     0x82u  /**< RZC main application */

/* ---- Main Application API IDs ---- */

#define MAIN_API_INIT           0x00u  /**< Initialization sequence      */
#define MAIN_API_SELF_TEST      0x01u  /**< Self-test sequence           */
#define MAIN_API_RUN            0x02u  /**< Main loop / state transition */

/* ---- Debug Runtime Error IDs (0x80+ to distinguish from development errors) ---- */

#define DET_E_DBG_CAN_INIT_OK       0x80u  /**< CAN controller initialized      */
#define DET_E_DBG_BSW_INIT_OK       0x81u  /**< All BSW modules initialized     */
#define DET_E_DBG_SWC_INIT_OK       0x82u  /**< All SWC modules initialized     */
#define DET_E_DBG_SELF_TEST_PASS    0x83u  /**< Self-test passed all items      */
#define DET_E_DBG_SELF_TEST_FAIL    0x84u  /**< Self-test failed (item-specific DTC via Dem) */
#define DET_E_DBG_STATE_RUN         0x85u  /**< BswM transitioned to RUN        */
#define DET_E_DBG_SYSTICK_START     0x86u  /**< SysTick started, entering loop  */
#define DET_E_DBG_BSW_INIT_START    0x87u  /**< BSW init sequence starting      */
#define DET_E_DBG_SELF_TEST_START   0x88u  /**< Self-test sequence starting     */
#define DET_E_DBG_CAN_STARTED       0x89u  /**< CAN controller mode = STARTED   */

/* ---- DEM Debug Runtime Error IDs ---- */

#define DET_E_DBG_DTC_CONFIRMED     0x90u  /**< DTC debounce reached confirm threshold */
#define DET_E_DBG_DTC_BROADCAST     0x91u  /**< DTC broadcast transmitted on CAN 0x500 */

#endif /* DET_ERRIDS_H */
