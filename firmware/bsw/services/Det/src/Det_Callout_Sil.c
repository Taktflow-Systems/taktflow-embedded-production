/**
 * @file    Det_Callout_Sil.c
 * @brief   Det callout for SIL builds — human-readable stderr output
 * @author  Claude + andao
 * @date    2026-03-10
 *
 * @details AUTOSAR pattern: Det stores structured (ModuleId, InstanceId,
 *          ApiId, ErrorId) in the ring buffer. This callout function is
 *          registered via Det_SetCallback() in POSIX/SIL builds only and
 *          translates structured IDs into human-readable fprintf output.
 *
 *          This file is linked ONLY in Makefile.posix — on target builds
 *          it does not exist in the binary. No #ifdef needed.
 *
 * @safety_req SWR-BSW-040: Development error tracing
 * @traces_to  TSR-022, TSR-038
 *
 * @standard AUTOSAR_SWS_DefaultErrorTracer (callout extension)
 * @copyright Taktflow Systems 2026
 */

/* cppcheck-suppress misra-c2012-21.6 ; SIL_DIAG stderr output only */
#include <stdio.h>
#include "Det.h"

/* ---- Module Name Lookup ---- */

static const char* det_callout_module_name(uint16 id)
{
    switch (id) {
    case DET_MODULE_CAN:    return "Can";
    case DET_MODULE_CANIF:  return "CanIf";
    case DET_MODULE_PDUR:   return "PduR";
    case DET_MODULE_COM:    return "Com";
    case DET_MODULE_DCM:    return "Dcm";
    case DET_MODULE_DEM:    return "Dem";
    case DET_MODULE_WDGM:   return "WdgM";
    case DET_MODULE_BSWM:   return "BswM";
    case DET_MODULE_E2E:    return "E2E";
    case DET_MODULE_RTE:    return "Rte";
    case DET_MODULE_SPI:    return "Spi";
    case DET_MODULE_ADC:    return "Adc";
    case DET_MODULE_DIO:    return "Dio";
    case DET_MODULE_GPT:    return "Gpt";
    case DET_MODULE_PWM:    return "Pwm";
    case DET_MODULE_IOHWAB: return "IoHwAb";
    case DET_MODULE_UART:   return "Uart";
    case DET_MODULE_NVM:    return "NvM";
    case DET_MODULE_CANTP:    return "CanTp";
    case DET_MODULE_CVC_MAIN: return "CVC";
    case DET_MODULE_FZC_MAIN: return "FZC";
    case DET_MODULE_RZC_MAIN: return "RZC";
    default:                  return "Unknown";
    }
}

static const char* det_callout_error_name(uint8 id)
{
    switch (id) {
    case DET_E_PARAM_POINTER: return "PARAM_POINTER";
    case DET_E_UNINIT:        return "UNINIT";
    case DET_E_PARAM_VALUE:        return "PARAM_VALUE";
    case DET_E_DBG_CAN_INIT_OK:    return "CAN_INIT_OK";
    case DET_E_DBG_BSW_INIT_OK:    return "BSW_INIT_OK";
    case DET_E_DBG_SWC_INIT_OK:    return "SWC_INIT_OK";
    case DET_E_DBG_SELF_TEST_PASS: return "SELF_TEST_PASS";
    case DET_E_DBG_SELF_TEST_FAIL: return "SELF_TEST_FAIL";
    case DET_E_DBG_STATE_RUN:      return "STATE_RUN";
    case DET_E_DBG_SYSTICK_START:  return "SYSTICK_START";
    case DET_E_DBG_BSW_INIT_START: return "BSW_INIT_START";
    case DET_E_DBG_SELF_TEST_START: return "SELF_TEST_START";
    case DET_E_DBG_CAN_STARTED:    return "CAN_STARTED";
    case DET_E_DBG_DTC_CONFIRMED:  return "DTC_CONFIRMED";
    case DET_E_DBG_DTC_BROADCAST:  return "DTC_BROADCAST";
    default:                       return "UNKNOWN";
    }
}

/* ---- Public Callout Function ---- */

/**
 * @brief  SIL debug callout — prints Det errors to stderr
 * @param  ModuleId    Module that reported the error
 * @param  InstanceId  Module instance (0 for single-instance)
 * @param  ApiId       API function that failed
 * @param  ErrorId     Error classification
 *
 * @note   Register with Det_SetCallback(Det_Callout_SilPrint) at ECU init.
 *         Only linked in POSIX builds — no debug strings on target flash.
 */
void Det_Callout_SilPrint(uint16 ModuleId, uint8 InstanceId,
                           uint8 ApiId, uint8 ErrorId)
{
    fprintf(stderr, "[DET] %s(Inst=%u) Api=0x%02X Err=%s(0x%02X)\n",
            det_callout_module_name(ModuleId), InstanceId,
            ApiId, det_callout_error_name(ErrorId), ErrorId);
}
