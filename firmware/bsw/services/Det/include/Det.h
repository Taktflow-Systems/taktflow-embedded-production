/**
 * @file    Det.h
 * @brief   Default Error Tracer — development-time error reporting
 * @date    2026-03-03
 *
 * @details Provides Det_ReportError() for all BSW modules to report
 *          parameter errors, uninitialized access, and runtime faults.
 *          Errors are logged to a ring buffer and output via SIL_DIAG
 *          (fprintf(stderr)) in POSIX/SIL builds.
 *
 * @safety_req SWR-BSW-040: Development error tracing
 * @traces_to  TSR-022, TSR-038
 *
 * @standard AUTOSAR_SWS_DefaultErrorTracer, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef DET_H
#define DET_H

#include "Std_Types.h"

/* ---- DET Error IDs (common across all modules) ---- */

#define DET_E_PARAM_POINTER     0x01u  /**< NULL pointer passed           */
#define DET_E_UNINIT            0x02u  /**< Module not initialized        */
#define DET_E_PARAM_VALUE       0x03u  /**< Parameter out of range        */

/* ---- Module IDs ---- */

#define DET_MODULE_CAN          0x01u
#define DET_MODULE_CANIF        0x02u
#define DET_MODULE_PDUR         0x03u
#define DET_MODULE_COM          0x04u
#define DET_MODULE_DCM          0x05u
#define DET_MODULE_DEM          0x06u
#define DET_MODULE_WDGM         0x07u
#define DET_MODULE_BSWM         0x08u
#define DET_MODULE_E2E          0x09u
#define DET_MODULE_RTE          0x0Au
#define DET_MODULE_SPI          0x0Bu
#define DET_MODULE_ADC          0x0Cu
#define DET_MODULE_DIO          0x0Du
#define DET_MODULE_GPT          0x0Eu
#define DET_MODULE_PWM          0x0Fu
#define DET_MODULE_IOHWAB       0x10u
#define DET_MODULE_UART         0x11u
#define DET_MODULE_NVM          0x12u
#define DET_MODULE_CANTP        0x13u

/* ---- API IDs per Module ----
 * Convention: <MODULE>_API_<FUNCTION> = sequential per module
 */

/* Can API IDs */
#define CAN_API_INIT                    0x00u
#define CAN_API_DEINIT                  0x01u
#define CAN_API_SET_CONTROLLER_MODE     0x02u
#define CAN_API_WRITE                   0x03u
#define CAN_API_MAIN_FUNCTION_READ      0x04u
#define CAN_API_GET_ERROR_COUNTERS      0x05u
#define CAN_API_GET_ERROR_STATE         0x06u

/* CanIf API IDs */
#define CANIF_API_INIT                  0x00u
#define CANIF_API_TRANSMIT              0x01u
#define CANIF_API_RX_INDICATION         0x02u

/* PduR API IDs */
#define PDUR_API_INIT                   0x00u
#define PDUR_API_CANIF_RX_INDICATION    0x01u
#define PDUR_API_TRANSMIT               0x02u
#define PDUR_API_DCM_TRANSMIT           0x03u

/* Com API IDs */
#define COM_API_INIT                    0x00u
#define COM_API_SEND_SIGNAL             0x01u
#define COM_API_RECEIVE_SIGNAL          0x02u
#define COM_API_RX_INDICATION           0x03u
#define COM_API_MAIN_FUNCTION_TX        0x04u

/* Dcm API IDs */
#define DCM_API_INIT                    0x00u
#define DCM_API_MAIN_FUNCTION           0x01u
#define DCM_API_RX_INDICATION           0x02u

/* Dem API IDs */
#define DEM_API_INIT                    0x00u
#define DEM_API_REPORT_ERROR_STATUS     0x01u
#define DEM_API_MAIN_FUNCTION           0x02u
#define DEM_API_SET_ECU_ID              0x03u
#define DEM_API_SET_DTC_FILTER          0x04u
#define DEM_API_GET_NUMBER_OF_FILTERED_DTC 0x05u
#define DEM_API_GET_NEXT_FILTERED_DTC   0x06u
#define DEM_API_CLEAR_DTC               0x07u

/* WdgM API IDs */
#define WDGM_API_INIT                   0x00u
#define WDGM_API_CHECKPOINT_REACHED     0x01u
#define WDGM_API_MAIN_FUNCTION          0x02u
#define WDGM_API_GET_SE_STATUS          0x03u

/* BswM API IDs */
#define BSWM_API_INIT                   0x00u
#define BSWM_API_REQUEST_MODE           0x01u
#define BSWM_API_MAIN_FUNCTION          0x02u

/* E2E API IDs */
#define E2E_API_PROTECT                 0x00u
#define E2E_API_CHECK                   0x01u

/* Rte API IDs */
#define RTE_API_INIT                    0x00u
#define RTE_API_WRITE                   0x01u
#define RTE_API_READ                    0x02u
#define RTE_API_MAIN_FUNCTION           0x03u

/* Spi API IDs */
#define SPI_API_INIT                    0x00u
#define SPI_API_WRITE_IB                0x01u
#define SPI_API_READ_IB                 0x02u
#define SPI_API_ASYNC_TRANSMIT          0x03u

/* Adc API IDs */
#define ADC_API_INIT                    0x00u
#define ADC_API_START_GROUP_CONVERSION  0x01u
#define ADC_API_READ_GROUP              0x02u

/* Dio API IDs */
#define DIO_API_INIT                    0x00u
#define DIO_API_READ_CHANNEL            0x01u
#define DIO_API_WRITE_CHANNEL           0x02u

/* Gpt API IDs */
#define GPT_API_INIT                    0x00u
#define GPT_API_START_TIMER             0x01u
#define GPT_API_STOP_TIMER              0x02u
#define GPT_API_GET_TIME_ELAPSED        0x03u

/* Pwm API IDs */
#define PWM_API_INIT                    0x00u
#define PWM_API_SET_DUTY_CYCLE          0x01u
#define PWM_API_SET_PERIOD_AND_DUTY     0x02u

/* IoHwAb API IDs */
#define IOHWAB_API_INIT                 0x00u
#define IOHWAB_API_READ_SENSOR          0x01u
#define IOHWAB_API_SET_ACTUATOR         0x02u

/* Uart API IDs */
#define UART_API_INIT                   0x00u
#define UART_API_TRANSMIT               0x01u
#define UART_API_RECEIVE                0x02u

/* NvM API IDs */
#define NVM_API_INIT                    0x00u
#define NVM_API_READ_BLOCK              0x01u
#define NVM_API_WRITE_BLOCK             0x02u
#define NVM_API_MAIN_FUNCTION           0x03u

/* ---- Ring Buffer Configuration ---- */

#define DET_LOG_SIZE    32u  /**< Ring buffer capacity (entries) */

/* ---- Types ---- */

/** Single DET error log entry */
typedef struct {
    uint16  ModuleId;       /**< Module that reported the error  */
    uint8   InstanceId;     /**< Module instance (0 for single)  */
    uint8   ApiId;          /**< API function that failed        */
    uint8   ErrorId;        /**< Error classification            */
} Det_ErrorEntryType;

/** Optional user callback for DET errors */
typedef void (*Det_CallbackType)(uint16 ModuleId, uint8 InstanceId,
                                 uint8 ApiId, uint8 ErrorId);

/* ---- API Functions ---- */

/**
 * @brief  Initialize DET module — clears ring buffer
 */
void Det_Init(void);

/**
 * @brief  Report a development error
 * @param  ModuleId    Module ID (DET_MODULE_xxx)
 * @param  InstanceId  Module instance (0 for single-instance)
 * @param  ApiId       API function ID (xxx_API_yyy)
 * @param  ErrorId     Error classification (DET_E_xxx)
 */
void Det_ReportError(uint16 ModuleId, uint8 InstanceId,
                     uint8 ApiId, uint8 ErrorId);

/**
 * @brief  Report a runtime error (same signature, separate counter)
 * @param  ModuleId    Module ID
 * @param  InstanceId  Module instance
 * @param  ApiId       API function ID
 * @param  ErrorId     Error classification
 */
void Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId,
                            uint8 ApiId, uint8 ErrorId);

/**
 * @brief  Get total number of reported errors since init
 * @return Error count (saturates at 0xFFFF)
 */
uint16 Det_GetErrorCount(void);

/**
 * @brief  Get a log entry from the ring buffer
 * @param  Index       0-based index (0 = oldest available)
 * @param  EntryPtr    Output: log entry
 * @return E_OK if entry exists, E_NOT_OK if index out of range
 */
Std_ReturnType Det_GetLogEntry(uint8 Index, Det_ErrorEntryType* EntryPtr);

/**
 * @brief  Register a user callback for DET errors
 * @param  Callback    Function pointer (NULL to disable)
 */
void Det_SetCallback(Det_CallbackType Callback);

/* ---- Debug Runtime Error IDs (ECU main.c) ---- */
#include "Det_ErrIds.h"

#endif /* DET_H */
