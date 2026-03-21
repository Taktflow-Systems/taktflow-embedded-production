/**
 * @file    main.c
 * @brief   FZC main entry point — BSW init, self-test, 10ms tick loop
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-025 to SWR-FZC-032
 * @traces_to  SSR-FZC-019 to SSR-FZC-024, TSR-046, TSR-047, TSR-048
 *
 * @details  Initializes system clock, MPU, all BSW modules (including UART
 *           for TFMini-S lidar), all SWCs (Steering, Brake, Lidar, Heartbeat,
 *           FzcSafety, Buzzer), runs 7-item self-test, then enters the main
 *           loop which dispatches the RTE scheduler from a 1ms SysTick.
 *
 *           Self-test items (SWR-FZC-025):
 *           1. Servo neutral — steering PWM centers, brake releases
 *           2. SPI sensor — AS5048A steering angle sensor responds
 *           3. UART lidar handshake — TFMini-S UART data arrives
 *           4. CAN loopback — CAN controller self-test
 *           5. MPU verify — MPU regions configured correctly
 *           6. Stack canary — stack overflow detection planted
 *           7. RAM pattern — memory integrity check
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Std_Types.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * BSW Module Headers
 * ================================================================== */

#include "Adc.h"
#include "Can.h"
#include "CanIf.h"
#include "Com.h"
#include "PduR.h"
#include "E2E.h"
#include "Dem.h"
#include "WdgM.h"
#include "BswM.h"
#include "Dcm.h"
#include "Rte.h"
#include "IoHwAb.h"
#include "Spi.h"
#include "Uart.h"
#include "CanSM.h"
#include "FiM.h"
#include "Xcp.h"
#include "SchM_Timing.h"

/* ==================================================================
 * SWC Headers
 * ================================================================== */

#include "Swc_Steering.h"
#include "Swc_Brake.h"
#include "Swc_Lidar.h"
#include "Swc_Heartbeat.h"
#include "Swc_FzcCom.h"
#include "Swc_FzcSafety.h"
#include "Swc_FzcCanMonitor.h"
#include "Swc_Buzzer.h"
#include "Swc_FzcSensorFeeder.h"

/* ==================================================================
 * Det-based debug tracing (replaces DBG_LOG macro)
 * ================================================================== */

#include "Det.h"

/* ==================================================================
 * CMSIS-RTOS2 (ThreadX backend via CMSIS adapter)
 * ================================================================== */

#ifdef USE_THREADX
#include "tx_api.h"
#endif

/* ==================================================================
 * External Configuration (defined in cfg/ files)
 * ================================================================== */

extern const Rte_ConfigType  fzc_rte_config;
extern const Com_ConfigType  fzc_com_config;
extern const Dcm_ConfigType  fzc_dcm_config;

/* ==================================================================
 * Hardware Abstraction Externs (implemented per platform)
 * ================================================================== */

extern void           Main_Hw_SystemClockInit(void);
extern void           Main_Hw_MpuConfig(void);
extern void           Main_Hw_SysTickInit(uint32 periodUs);
extern void           Main_Hw_Wfi(void);
extern uint32         Main_Hw_GetTick(void);

/* Self-test hardware externs */
extern Std_ReturnType Main_Hw_ServoNeutralTest(void);
extern Std_ReturnType Main_Hw_SpiSensorTest(void);
extern Std_ReturnType Main_Hw_UartLidarTest(void);
extern Std_ReturnType Main_Hw_CanLoopbackTest(void);
extern Std_ReturnType Main_Hw_MpuVerifyTest(void);
extern Std_ReturnType Main_Hw_RamPatternTest(void);
extern void           Main_Hw_PlantStackCanary(void);

/* 5s periodic debug status — UART print on STM32, no-op on POSIX */
extern void           Main_Hw_DebugPrintStatus(uint32 tick_us);

/* ==================================================================
 * Static Configuration Constants
 * ================================================================== */

/** CAN driver configuration — 500 kbps, controller 0 */
static const Can_ConfigType can_config = {
    .baudrate     = 500000u,
    .controllerId = 0u,
};

/** CanIf TX PDU routing: Com TX PDU -> CAN ID */
static const CanIf_TxPduConfigType canif_tx_config[] = {
    /* canId,  upperPduId,                  dlc, hth */
    { 0x011u, FZC_COM_TX_HEARTBEAT,         8u, 0u },  /* FZC heartbeat        */
    { 0x200u, FZC_COM_TX_STEER_STATUS,      8u, 0u },  /* Steering status      */
    { 0x201u, FZC_COM_TX_BRAKE_STATUS,      8u, 0u },  /* Brake status         */
    { 0x210u, FZC_COM_TX_BRAKE_FAULT,       8u, 0u },  /* Brake fault          */
    { 0x211u, FZC_COM_TX_MOTOR_CUTOFF,      8u, 0u },  /* Motor cutoff request */
    { 0x220u, FZC_COM_TX_LIDAR,             8u, 0u },  /* Lidar data           */
    { 0x500u, FZC_COM_TX_DTC_BROADCAST,     8u, 0u },  /* DTC broadcast        */
    { 0x7E9u, FZC_COM_TX_UDS_RESP_FZC,     8u, 0u },  /* UDS response         */
};

/** CanIf RX PDU routing: CAN ID -> Com RX PDU */
static const CanIf_RxPduConfigType canif_rx_config[] = {
    /* canId,  upperPduId,                       dlc, isExtended */
    { 0x001u, FZC_COM_RX_ESTOP,                  8u, FALSE },  /* E-stop broadcast    */
    { 0x010u, FZC_COM_RX_CVC_HEARTBEAT,          8u, FALSE },  /* CVC heartbeat       */
    { 0x012u, FZC_COM_RX_RZC_HEARTBEAT,          8u, FALSE },  /* RZC heartbeat       */
    { 0x013u, FZC_COM_RX_SC_STATUS,              8u, FALSE },  /* SC status           */
    { 0x014u, FZC_COM_RX_ICU_HEARTBEAT,          8u, FALSE },  /* ICU heartbeat       */
    { 0x015u, FZC_COM_RX_TCU_HEARTBEAT,          8u, FALSE },  /* TCU heartbeat       */
    { 0x100u, FZC_COM_RX_VEHICLE_STATE,          8u, FALSE },  /* Vehicle state       */
    { 0x101u, FZC_COM_RX_TORQUE_REQUEST,         8u, FALSE },  /* Torque request      */
    { 0x102u, FZC_COM_RX_STEER_CMD,              8u, FALSE },  /* Steering command    */
    { 0x103u, FZC_COM_RX_BRAKE_CMD,              8u, FALSE },  /* Brake command       */
    { 0x300u, FZC_COM_RX_MOTOR_STATUS,           8u, FALSE },  /* Motor status        */
    { 0x301u, FZC_COM_RX_MOTOR_CURRENT,          8u, FALSE },  /* Motor current       */
    { 0x302u, FZC_COM_RX_MOTOR_TEMPERATURE,      8u, FALSE },  /* Motor temperature   */
    { 0x303u, FZC_COM_RX_BATTERY_STATUS,         8u, FALSE },  /* Battery status      */
    { 0x350u, FZC_COM_RX_BODY_CONTROL_CMD,       8u, FALSE },  /* Body control cmd    */
    { 0x400u, FZC_COM_RX_LIGHT_STATUS,           8u, FALSE },  /* Light status        */
    { 0x401u, FZC_COM_RX_INDICATOR_STATE,        8u, FALSE },  /* Indicator state     */
    { 0x402u, FZC_COM_RX_DOOR_LOCK_STATUS,       8u, FALSE },  /* Door lock status    */
    { 0x500u, FZC_COM_RX_DTC_BROADCAST,          8u, FALSE },  /* DTC broadcast       */
    { 0x600u, FZC_COM_RX_VIRT_SENSORS,           8u, FALSE },  /* Virtual sensors     */
};

static const CanIf_ConfigType canif_config = {
    .txPduConfig = canif_tx_config,
    .txPduCount  = (uint8)(sizeof(canif_tx_config) / sizeof(canif_tx_config[0])),
    .rxPduConfig = canif_rx_config,
    .rxPduCount  = (uint8)(sizeof(canif_rx_config) / sizeof(canif_rx_config[0])),
    .e2eRxCheck  = NULL_PTR,
};

/** PduR RX routing: CanIf RX PDU ID -> Com */
static const PduR_RoutingTableType fzc_pdur_routing[] = {
    { FZC_COM_RX_ESTOP,              PDUR_DEST_COM, FZC_COM_RX_ESTOP              },
    { FZC_COM_RX_CVC_HEARTBEAT,      PDUR_DEST_COM, FZC_COM_RX_CVC_HEARTBEAT      },
    { FZC_COM_RX_RZC_HEARTBEAT,      PDUR_DEST_COM, FZC_COM_RX_RZC_HEARTBEAT      },
    { FZC_COM_RX_SC_STATUS,          PDUR_DEST_COM, FZC_COM_RX_SC_STATUS          },
    { FZC_COM_RX_ICU_HEARTBEAT,      PDUR_DEST_COM, FZC_COM_RX_ICU_HEARTBEAT      },
    { FZC_COM_RX_TCU_HEARTBEAT,      PDUR_DEST_COM, FZC_COM_RX_TCU_HEARTBEAT      },
    { FZC_COM_RX_VEHICLE_STATE,      PDUR_DEST_COM, FZC_COM_RX_VEHICLE_STATE      },
    { FZC_COM_RX_TORQUE_REQUEST,     PDUR_DEST_COM, FZC_COM_RX_TORQUE_REQUEST     },
    { FZC_COM_RX_STEER_CMD,          PDUR_DEST_COM, FZC_COM_RX_STEER_CMD          },
    { FZC_COM_RX_BRAKE_CMD,          PDUR_DEST_COM, FZC_COM_RX_BRAKE_CMD          },
    { FZC_COM_RX_MOTOR_STATUS,       PDUR_DEST_COM, FZC_COM_RX_MOTOR_STATUS       },
    { FZC_COM_RX_MOTOR_CURRENT,      PDUR_DEST_COM, FZC_COM_RX_MOTOR_CURRENT      },
    { FZC_COM_RX_MOTOR_TEMPERATURE,  PDUR_DEST_COM, FZC_COM_RX_MOTOR_TEMPERATURE  },
    { FZC_COM_RX_BATTERY_STATUS,     PDUR_DEST_COM, FZC_COM_RX_BATTERY_STATUS     },
    { FZC_COM_RX_BODY_CONTROL_CMD,   PDUR_DEST_COM, FZC_COM_RX_BODY_CONTROL_CMD   },
    { FZC_COM_RX_LIGHT_STATUS,       PDUR_DEST_COM, FZC_COM_RX_LIGHT_STATUS       },
    { FZC_COM_RX_INDICATOR_STATE,    PDUR_DEST_COM, FZC_COM_RX_INDICATOR_STATE    },
    { FZC_COM_RX_DOOR_LOCK_STATUS,   PDUR_DEST_COM, FZC_COM_RX_DOOR_LOCK_STATUS   },
    { FZC_COM_RX_DTC_BROADCAST,      PDUR_DEST_COM, FZC_COM_RX_DTC_BROADCAST      },
    { FZC_COM_RX_VIRT_SENSORS,       PDUR_DEST_COM, FZC_COM_RX_VIRT_SENSORS       },
};

static const PduR_ConfigType fzc_pdur_config = {
    .routingTable = fzc_pdur_routing,
    .routingCount = (uint8)(sizeof(fzc_pdur_routing) / sizeof(fzc_pdur_routing[0])),
};

/** SPI driver configuration — AS5048A steering angle sensor */
static const Spi_ConfigType spi_config = {
    .clockSpeed   = 1000000u,   /* 1 MHz SPI clock              */
    .cpol         = 0u,         /* Clock idle low                */
    .cpha         = 1u,         /* Sample on trailing edge       */
    .dataWidth    = 16u,        /* 16-bit transfers              */
    .numChannels  = 1u,         /* Ch 0: steering                */
};

/** ADC group configuration — brake position feedback */
static const Adc_GroupConfigType adc_groups[] = {
    { .numChannels = 1u, .triggerSource = 0u },  /* Group 0: (unused)        */
    { .numChannels = 1u, .triggerSource = 0u },  /* Group 1: (unused)        */
    { .numChannels = 1u, .triggerSource = 0u },  /* Group 2: (unused)        */
    { .numChannels = 1u, .triggerSource = 0u },  /* Group 3: brake position  */
};

static const Adc_ConfigType adc_config = {
    .numGroups  = 4u,
    .groups     = adc_groups,
    .resolution = 12u,
};

/** IoHwAb channel mapping for FZC */
static const IoHwAb_ConfigType iohwab_config = {
    .SteeringSpiChannel   = 0u,   /* SPI1 for AS5048A angle sensor */
    .SteeringCsChannel    = 0u,   /* CS pin for AS5048A            */
    .SteeringSpiSequence  = 0u,
    .SteeringServoPwmCh   = 0u,   /* TIM2_CH1 (PA0) steering servo */
    .BrakeServoPwmCh      = 1u,   /* TIM2_CH2 (PA1) brake servo    */
    .BrakePositionAdcGroup = 3u,  /* ADC group 3: brake potentiometer feedback */
    .EStopDioChannel      = 2u,
    .BuzzerDioChannel     = 8u,   /* PB8 buzzer output             */
    .WdiDioChannel        = 0u,   /* PB0 TPS3823 WDI pin           */
};

/** UART configuration for TFMini-S lidar (115200 baud, 8N1) */
static const Uart_ConfigType uart_config = {
    .baudRate  = 115200u,
    .dataBits  = 8u,
    .stopBits  = 1u,
    .parity    = 0u,   /* none */
    .timeoutMs = FZC_LIDAR_TIMEOUT_MS,
};

/** Steering SWC configuration */
static const Swc_Steering_ConfigType steering_config = {
    .plausThreshold   = FZC_STEER_PLAUS_THRESHOLD_DEG,
    .plausDebounce    = FZC_STEER_PLAUS_DEBOUNCE,
    .rateLimitDeg10ms = FZC_STEER_RATE_LIMIT_DEG_10MS,
    .cmdTimeoutMs     = FZC_STEER_CMD_TIMEOUT_MS,
    .rtcRateDegS      = FZC_STEER_RTC_RATE_DEG_S,
    .latchClearCycles = FZC_STEER_LATCH_CLEAR_CYCLES,
};

/** Brake SWC configuration */
static const Swc_Brake_ConfigType brake_config = {
    .autoTimeoutMs     = FZC_BRAKE_AUTO_TIMEOUT_MS,
    .pwmFaultThreshold = FZC_BRAKE_PWM_FAULT_THRESH,
    .faultDebounce     = FZC_BRAKE_FAULT_DEBOUNCE,
    .latchClearCycles  = FZC_BRAKE_LATCH_CLEAR_CYCLES,
    .cutoffRepeatCount = FZC_BRAKE_CUTOFF_REPEAT_COUNT,
};

/** Lidar SWC configuration */
static const Swc_Lidar_ConfigType lidar_config = {
    .warnDistCm      = FZC_LIDAR_WARN_CM,
    .brakeDistCm     = FZC_LIDAR_BRAKE_CM,
    .emergencyDistCm = FZC_LIDAR_EMERGENCY_CM,
    .timeoutMs       = FZC_LIDAR_TIMEOUT_MS,
    .stuckCycles     = FZC_LIDAR_STUCK_CYCLES,
    .rangeMinCm      = FZC_LIDAR_RANGE_MIN_CM,
    .rangeMaxCm      = FZC_LIDAR_RANGE_MAX_CM,
    .signalMin       = FZC_LIDAR_SIGNAL_MIN,
    .degradeCycles   = FZC_LIDAR_DEGRADE_CYCLES,
};

/** WdgM supervised entity configuration */
static const WdgM_SupervisedEntityConfigType wdgm_se_config[] = {
    { 0u, 1u, 1u, 3u },   /* SE 0: Swc_Steering    — ASIL D, 3 failures tolerated */
    { 1u, 1u, 1u, 3u },   /* SE 1: Swc_Brake       — ASIL D */
    { 2u, 1u, 1u, 3u },   /* SE 2: Swc_Lidar       — ASIL C */
    { 3u, 1u, 1u, 3u },   /* SE 3: Swc_Heartbeat   — ASIL C */
    { 4u, 1u, 1u, 3u },   /* SE 4: Swc_FzcSafety   — ASIL D */
    { 5u, 1u, 1u, 5u },   /* SE 5: Swc_Buzzer      — QM/ASIL B, more tolerant */
};

static const WdgM_ConfigType wdgm_config = {
    .seConfig      = wdgm_se_config,
    .seCount       = (uint8)(sizeof(wdgm_se_config) / sizeof(wdgm_se_config[0])),
    .wdtDioChannel = 0u,   /* PB0 — TPS3823 WDI (also driven by Swc_FzcSafety) */
};

/* ==================================================================
 * BswM Mode Actions (placeholder callbacks)
 * ================================================================== */

static void BswM_Action_Run(void)
{
    /* Enable steering servo, brake servo outputs */
}

static void BswM_Action_SafeStop(void)
{
    /* Center steering, apply max brake, disable motor cutoff */
}

static void BswM_Action_Shutdown(void)
{
    /* Disable all servo outputs, stop watchdog feed */
}

static const BswM_ModeActionType bswm_actions[] = {
    { BSWM_RUN,       BswM_Action_Run      },
    { BSWM_SAFE_STOP, BswM_Action_SafeStop },
    { BSWM_SHUTDOWN,  BswM_Action_Shutdown  },
};

static const BswM_ConfigType bswm_config = {
    .ModeActions = bswm_actions,
    .ActionCount = (uint8)(sizeof(bswm_actions) / sizeof(bswm_actions[0])),
};

/* ==================================================================
 * Self-Test Sequence (SWR-FZC-025)
 * ================================================================== */

/**
 * @brief  Run FZC power-on self-test sequence (7 items)
 * @return FZC_SELF_TEST_PASS if all tests pass, FZC_SELF_TEST_FAIL otherwise
 *
 * @safety_req SWR-FZC-025
 * @traces_to  SSR-FZC-019
 */
static uint8 Main_RunSelfTest(void)
{
    /* Item 1: Plant stack canary for stack overflow detection */
    Main_Hw_PlantStackCanary();

    /* Item 2: Servo neutral — steering centers, brake releases */
    if (Main_Hw_ServoNeutralTest() != E_OK)
    {
        Dem_ReportErrorStatus(FZC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        return FZC_SELF_TEST_FAIL;
    }

    /* Item 3: SPI sensor — AS5048A steering angle sensor responds */
    if (Main_Hw_SpiSensorTest() != E_OK)
    {
        Dem_ReportErrorStatus(FZC_DTC_STEER_SPI_FAIL, DEM_EVENT_STATUS_FAILED);
        return FZC_SELF_TEST_FAIL;
    }

    /* Item 4: UART lidar handshake — TFMini-S data arrives */
    if (Main_Hw_UartLidarTest() != E_OK)
    {
        Dem_ReportErrorStatus(FZC_DTC_LIDAR_TIMEOUT, DEM_EVENT_STATUS_FAILED);
        return FZC_SELF_TEST_FAIL;
    }

    /* Item 5: CAN loopback — CAN controller self-test */
    if (Main_Hw_CanLoopbackTest() != E_OK)
    {
        Dem_ReportErrorStatus(FZC_DTC_CAN_BUS_OFF, DEM_EVENT_STATUS_FAILED);
        return FZC_SELF_TEST_FAIL;
    }

    /* Item 6: MPU verify — memory protection regions configured */
    if (Main_Hw_MpuVerifyTest() != E_OK)
    {
        Dem_ReportErrorStatus(FZC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        return FZC_SELF_TEST_FAIL;
    }

    /* Item 7: RAM pattern — memory integrity check */
    if (Main_Hw_RamPatternTest() != E_OK)
    {
        Det_ReportRuntimeError(DET_MODULE_FZC_MAIN, 0u, MAIN_API_SELF_TEST, DET_E_DBG_SELF_TEST_FAIL);
        Dem_ReportErrorStatus(FZC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        return FZC_SELF_TEST_FAIL;
    }

    Det_ReportRuntimeError(DET_MODULE_FZC_MAIN, 0u, MAIN_API_SELF_TEST, DET_E_DBG_SELF_TEST_PASS);
    return FZC_SELF_TEST_PASS;
}

/* ==================================================================
 * Tick Counters
 * ================================================================== */

static volatile uint32 tick_us;

/* ==================================================================
 * ThreadX Timer Callbacks (USE_THREADX only)
 * ================================================================== */

#ifdef USE_THREADX

/**
 * @brief  1ms periodic timer callback — RTE scheduler dispatch
 * @param  arg  unused (ThreadX timer callback signature: ULONG)
 *
 * @note   Executes in ThreadX timer thread context.
 *         Dispatches all RTE runnables configured at 1ms period.
 */
void Timer_1ms_Callback(ULONG arg)
{
    (void)arg;
    Rte_MainFunction();
}

/**
 * @brief  10ms periodic timer callback — Dcm, BswM, UART
 * @param  arg  unused
 *
 * @note   Executes in ThreadX timer thread context.
 */
void Timer_10ms_Callback(ULONG arg)
{
    (void)arg;
    Dcm_MainFunction();
    BswM_MainFunction();
    CanSM_MainFunction();
    Uart_MainFunction();
}

/**
 * @brief  100ms periodic timer callback — WdgM, Dem, FiM
 * @param  arg  unused
 *
 * @note   Executes in timer service thread context.
 */
void Timer_100ms_Callback(ULONG arg)
{
    (void)arg;
    WdgM_MainFunction();
    Dem_MainFunction();
}

/**
 * @brief  5s periodic timer callback — debug status print
 * @param  arg  unused
 *
 * @note   Calls Main_Hw_DebugPrintStatus with current kernel tick.
 *         UART print on STM32, no-op on POSIX.
 */
void Timer_5s_Callback(ULONG arg)
{
    (void)arg;
    Main_Hw_DebugPrintStatus(Main_Hw_GetTick());
}

#endif /* USE_THREADX */

/* ==================================================================
 * Main Entry Point
 * ================================================================== */

/**
 * @brief  FZC main function — init, self-test, main loop
 *
 * @safety_req SWR-FZC-025 to SWR-FZC-032
 * @traces_to  SSR-FZC-019 to SSR-FZC-024, TSR-046, TSR-047, TSR-048
 */
int main(void)
{
#ifndef USE_THREADX
    uint32 last_1ms_us   = 0u;
    uint32 last_10ms_us  = 0u;
    uint32 last_100ms_us = 0u;
    uint32 last_5s_us    = 0u;
#endif
    uint8  self_test_result;

    /* ---- Step 1: Hardware initialization ---- */
    Main_Hw_SystemClockInit();
    Main_Hw_MpuConfig();

    /* ---- Step 2: BSW module initialization (order matters) ---- */
    Can_Init(&can_config);
    Det_ReportRuntimeError(DET_MODULE_FZC_MAIN, 0u, MAIN_API_INIT, DET_E_DBG_CAN_INIT_OK);
    CanIf_Init(&canif_config);
    PduR_Init(&fzc_pdur_config);
    Com_Init(&fzc_com_config);
    E2E_Init();
    Dem_Init(NULL_PTR);
    Dem_SetEcuId(FZC_ECU_ID);                              /* 0x02 — FZC ECU ID */
    Dem_SetBroadcastPduId(FZC_COM_TX_DTC_BROADCAST);       /* CanIf TX for 0x500 */

    /* New BSW modules (Phase 4) */
    {
        static const CanSM_ConfigType fzc_cansm_cfg = { 10u, 5u, 1000u, 10u };
        static const Xcp_ConfigType fzc_xcp_cfg = {
            .RxPduId = FZC_COM_RX_XCP_REQ_FZC,
            .TxPduId = FZC_COM_TX_XCP_RESP_FZC,
        };
        CanSM_Init(&fzc_cansm_cfg);
        Xcp_Init(&fzc_xcp_cfg);
        SchM_TimingInit();
    }

    /* Remap DTC codes from CVC-centric defaults to FZC-specific codes */
    Dem_SetDtcCode(FZC_DTC_STEER_PLAUSIBILITY, 0x00D001u); /* Steering plausibility */
    Dem_SetDtcCode(FZC_DTC_STEER_RANGE,        0x00D002u); /* Steering range */
    Dem_SetDtcCode(FZC_DTC_STEER_RATE,         0x00D003u); /* Steering rate */
    Dem_SetDtcCode(FZC_DTC_STEER_TIMEOUT,      0x00D004u); /* Steering timeout */
    Dem_SetDtcCode(FZC_DTC_STEER_SPI_FAIL,     0x00D005u); /* SPI sensor fail */
    Dem_SetDtcCode(FZC_DTC_BRAKE_FAULT,        0x00D101u); /* Brake fault */
    Dem_SetDtcCode(FZC_DTC_BRAKE_TIMEOUT,      0x00D102u); /* Brake timeout */
    Dem_SetDtcCode(FZC_DTC_BRAKE_PWM_FAIL,     0x00D103u); /* Brake PWM fail */
    Dem_SetDtcCode(FZC_DTC_LIDAR_TIMEOUT,      0x00D201u); /* Lidar timeout */
    Dem_SetDtcCode(FZC_DTC_LIDAR_CHECKSUM,     0x00D202u); /* Lidar checksum */
    Dem_SetDtcCode(FZC_DTC_LIDAR_STUCK,        0x00D203u); /* Lidar stuck */
    Dem_SetDtcCode(FZC_DTC_LIDAR_SIGNAL_LOW,   0x00D204u); /* Lidar signal low */
    Dem_SetDtcCode(FZC_DTC_CAN_BUS_OFF,        0x00D301u); /* CAN bus-off */
    Dem_SetDtcCode(FZC_DTC_SELF_TEST_FAIL,     0x00D401u); /* Self-test fail */
    Dem_SetDtcCode(FZC_DTC_WATCHDOG_FAIL,      0x00D402u); /* Watchdog fail */
    Dem_SetDtcCode(FZC_DTC_BRAKE_OSCILLATION,  0x00D104u); /* Brake oscillation */

    WdgM_Init(&wdgm_config);
    BswM_Init(&bswm_config);
    Dcm_Init(&fzc_dcm_config);
    Spi_Init(&spi_config);
    Adc_Init(&adc_config);
    IoHwAb_Init(&iohwab_config);
    Uart_Init(&uart_config);   /* UART for TFMini-S lidar */
    Rte_Init(&fzc_rte_config);
    Det_ReportRuntimeError(DET_MODULE_FZC_MAIN, 0u, MAIN_API_INIT, DET_E_DBG_BSW_INIT_OK);

    /* ---- Step 3: SWC initialization ---- */
    Swc_Steering_Init(&steering_config);
    Swc_Brake_Init(&brake_config);
    Swc_Lidar_Init(&lidar_config);
    Swc_Heartbeat_Init();
    Swc_FzcCom_Init();
    Swc_FzcCanMonitor_Init();
    Swc_FzcSafety_Init();
    Swc_Buzzer_Init();
    Swc_FzcSensorFeeder_Init();
    Det_ReportRuntimeError(DET_MODULE_FZC_MAIN, 0u, MAIN_API_INIT, DET_E_DBG_SWC_INIT_OK);

    /* ---- Step 4: Self-test sequence (7 items, SWR-FZC-025) ---- */
    self_test_result = Main_RunSelfTest();

    /* Write self-test result to RTE for Swc_FzcSafety */
    (void)Rte_Write(FZC_SIG_SELF_TEST_RESULT, (uint32)self_test_result);

    /* ---- Step 5: Start CAN controller ---- */
    (void)Can_SetControllerMode(0u, CAN_CS_STARTED);

    /* ---- Step 6: Request BSW RUN mode (if self-test passed) ---- */
    if (self_test_result == FZC_SELF_TEST_PASS)
    {
        (void)BswM_RequestMode(0u, BSWM_RUN);
        Det_ReportRuntimeError(DET_MODULE_FZC_MAIN, 0u, MAIN_API_RUN, DET_E_DBG_STATE_RUN);
    }

    /* ---- Step 7: Start SysTick (1ms period = 1000us) ---- */
    /* SysTick needed for HAL timeouts during init.
     * ThreadX will reconfigure SysTick in _tx_initialize_low_level.S later. */
    Main_Hw_SysTickInit(1000u);
    Det_ReportRuntimeError(DET_MODULE_FZC_MAIN, 0u, MAIN_API_RUN, DET_E_DBG_SYSTICK_START);

    /* ---- Step 8: Main loop / RTOS kernel ---- */

#ifdef USE_THREADX
    /* Start ThreadX kernel — never returns.
     * Timers created in tx_application_define() (tx_stubs.c). */
    tx_kernel_enter();
#else
    /* Original bare-metal polling loop */
    for (;;)
    {
        Main_Hw_Wfi();

        tick_us = Main_Hw_GetTick();

        /* 1ms task: RTE scheduler (dispatches runnables internally)
         * Main_Hw_GetTick() returns microseconds; 1ms = 1000us */
        if ((tick_us - last_1ms_us) >= 1000u)
        {
            last_1ms_us = tick_us;
            Rte_MainFunction();
        }

        /* 10ms tasks: Dcm, BswM, UART timeout monitoring */
        if ((tick_us - last_10ms_us) >= 10000u)
        {
            last_10ms_us = tick_us;
            Dcm_MainFunction();
            BswM_MainFunction();
            CanSM_MainFunction();
            Uart_MainFunction();
        }

        /* 100ms tasks: WdgM, Dem (DTC broadcast) */
        if ((tick_us - last_100ms_us) >= 100000u)
        {
            last_100ms_us = tick_us;
            WdgM_MainFunction();
            Dem_MainFunction();
        }

        /* 5s debug task: platform-specific status print (UART on STM32, no-op on POSIX) */
        if ((tick_us - last_5s_us) >= 5000000u)
        {
            last_5s_us = tick_us;
            Main_Hw_DebugPrintStatus(tick_us);
        }
    }
#endif /* USE_THREADX */

    /* MISRA: unreachable but satisfies compiler */
    return 0;
}
