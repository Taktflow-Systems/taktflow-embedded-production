/**
 * @file    main.c
 * @brief   CVC main entry point — BSW init, self-test, 10ms tick loop
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-029 to SWR-CVC-035
 * @traces_to  SSR-CVC-029 to SSR-CVC-035, TSR-046, TSR-047, TSR-048
 *
 * @details  Initializes system clock, MPU, all BSW modules, all SWCs,
 *           runs self-test, then enters the main loop which dispatches
 *           the RTE scheduler from a 1ms SysTick interrupt.
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Std_Types.h"
#include "Cvc_Cfg.h"
#include "Cvc_App.h"
#ifdef SIL_DIAG
#include <stdio.h>
#endif

/* ==================================================================
 * BSW Module Headers
 * ================================================================== */

#include "Can.h"
#include "CanIf.h"
#include "Com.h"
#include "PduR.h"
#include "E2E.h"
#include "Dem.h"
#include "WdgM.h"
#include "BswM.h"
#include "Dcm.h"
#include "CanTp.h"
#include "CanSM.h"
#include "FiM.h"
#include "Xcp.h"
#include "SchM_Timing.h"
#include "Rte.h"
#include "Spi.h"
#include "Adc.h"
#include "Pwm.h"
#include "Dio.h"
#include "IoHwAb.h"

/* ==================================================================
 * SWC Headers
 * ================================================================== */

#include "Ssd1306.h"
#include "Swc_Pedal.h"
#include "Swc_VehicleState.h"
#include "Swc_EStop.h"
#include "Swc_Heartbeat.h"
#include "Swc_Dashboard.h"
#include "Swc_CvcCom.h"

/* ==================================================================
 * Det-based debug tracing (replaces DBG_LOG macro)
 * ================================================================== */

#include "Det.h"

/* ==================================================================
 * External Configuration (defined in cfg/ files)
 * ================================================================== */

extern const Rte_ConfigType  cvc_rte_config;
extern const Com_ConfigType  cvc_com_config;
extern const Dcm_ConfigType  cvc_dcm_config;

/* ==================================================================
 * Hardware Abstraction Interface (declared in Main_Hw.h, MISRA 8.5)
 * ================================================================== */

#include "Main_Hw.h"

#ifdef USE_THREADX
#include "tx_api.h"
#endif

/* ==================================================================
 * Static Configuration Constants
 * ================================================================== */

/** CAN driver configuration — 500 kbps, controller 0 */
static const Can_ConfigType can_config = {
    .baudrate     = 500000u,
    .controllerId = 0u,
};

/* CanIf config — use GENERATED routing table from CanIf_Cfg_Cvc.c
 * DO NOT hand-write CAN ID routing here. All routing is generated from
 * DBC → ARXML → codegen. */
extern const CanIf_ConfigType cvc_canif_config;
#define canif_config cvc_canif_config

/* PduR config — use GENERATED routing table from PduR_Cfg_Cvc.c
 * DO NOT hand-write routing tables here. All routing is generated from
 * DBC → ARXML → codegen. The generated config includes XCP, UDS, and
 * all Com RX PDU routing. */
extern const PduR_ConfigType cvc_pdur_config;

/** CanTp configuration — generated from ARXML (CanTp_Cfg_Cvc.c) */
extern const CanTp_ConfigType cvc_cantp_config;

/** SPI driver configuration — AS5048A angle sensors (CPOL=0, CPHA=1, 16-bit) */
static const Spi_ConfigType spi_config = {
    .clockSpeed   = 1000000u,   /* 1 MHz SPI clock              */
    .cpol         = 0u,         /* Clock idle low                */
    .cpha         = 1u,         /* Sample on trailing edge       */
    .dataWidth    = 16u,        /* 16-bit transfers              */
    .numChannels  = 2u,         /* Ch 0: pedal, Ch 1: steering   */
};

/** ADC group configuration — motor current, motor temp, battery voltage */
static const Adc_GroupConfigType adc_groups[] = {
    { .numChannels = 1u, .triggerSource = 0u },  /* Group 0: motor current */
    { .numChannels = 1u, .triggerSource = 0u },  /* Group 1: motor temp    */
    { .numChannels = 1u, .triggerSource = 0u },  /* Group 2: battery volt  */
};

static const Adc_ConfigType adc_config = {
    .numGroups  = 3u,
    .groups     = adc_groups,
    .resolution = 12u,
};

/** PWM driver configuration — motor, steering servo, brake servo */
static const Pwm_ChannelConfigType pwm_channels[] = {
    { .frequency = 20000u, .defaultDuty = 0u, .polarity = PWM_HIGH, .idleState = PWM_LOW },  /* Ch 0: motor     */
    { .frequency = 50u,    .defaultDuty = 0u, .polarity = PWM_HIGH, .idleState = PWM_LOW },  /* Ch 1: steer srv  */
    { .frequency = 50u,    .defaultDuty = 0u, .polarity = PWM_HIGH, .idleState = PWM_LOW },  /* Ch 2: brake srv  */
};

static const Pwm_ConfigType pwm_config = {
    .numChannels = 3u,
    .channels    = pwm_channels,
};

/** IoHwAb channel mapping for CVC */
static const IoHwAb_ConfigType iohwab_config = {
    .PedalSpiChannel     = 0u,
    .PedalCsChannel0     = 0u,
    .PedalCsChannel1     = 1u,
    .PedalSpiSequence    = 0u,
    .SteeringSpiChannel  = 1u,
    .SteeringCsChannel   = 2u,
    .SteeringSpiSequence = 1u,
    .MotorCurrentAdcGroup = 0u,
    .MotorTempAdcGroup    = 1u,
    .BatteryVoltAdcGroup  = 2u,
    .MotorPwmChannel      = 0u,
    .SteeringServoPwmCh   = 1u,
    .BrakeServoPwmCh      = 2u,
    .MotorDirAChannel     = 3u,
    .MotorDirBChannel     = 4u,
    .EStopDioChannel      = 5u,
};

/** CanSM configuration — bus-off recovery L1 (fast) + L2 (slow) */
static const CanSM_ConfigType cansm_config = {
    .L1_RecoveryTimeMs = 10u,    /* L1: retry every 10ms */
    .L1_MaxAttempts    = 5u,     /* L1: up to 5 fast retries */
    .L2_RecoveryTimeMs = 1000u,  /* L2: retry every 1s */
    .L2_MaxAttempts    = 10u,    /* L2: up to 10 slow retries */
};

/** FiM configuration — function inhibition rules
 *  FID 0 = Brake control, FID 1 = Motor control, FID 2 = Steering */
static const FiM_InhibitionConfigType cvc_fim_inhibitions[] = {
    /* FID,  DemEventId, DemStatusMask (0x01=TestFailed) */
    { 0u,   3u,         0x01u },  /* Brake fault (DEM event 3) → inhibit brake FID */
    { 1u,   4u,         0x01u },  /* Motor fault (DEM event 4) → inhibit motor FID */
    { 2u,   5u,         0x01u },  /* Steer fault (DEM event 5) → inhibit steering FID */
};

static const FiM_ConfigType cvc_fim_config = {
    .inhibitions     = cvc_fim_inhibitions,
    .inhibitionCount = (uint8)(sizeof(cvc_fim_inhibitions) / sizeof(cvc_fim_inhibitions[0])),
    .functionCount   = 3u,
};

/** XCP configuration — CAN ID pair for measurement/calibration */
static const Xcp_ConfigType cvc_xcp_config = {
    .RxPduId = CVC_COM_RX_XCP_REQ_CVC,  /* CAN 0x550 */
    .TxPduId = CVC_COM_TX_XCP_RESP_CVC,  /* CAN 0x551 */
};

/** Pedal SWC configuration */
static const Swc_Pedal_ConfigType pedal_config = {
    .plausThreshold   = CVC_PEDAL_PLAUS_THRESHOLD,
    .plausDebounce    = CVC_PEDAL_PLAUS_DEBOUNCE,
    .stuckThreshold   = CVC_PEDAL_STUCK_THRESHOLD,
    .stuckCycles      = CVC_PEDAL_STUCK_CYCLES,
    .latchClearCycles = CVC_PEDAL_LATCH_CLEAR_CYCLES,
    .rampLimit        = CVC_PEDAL_RAMP_LIMIT,
};

/** WdgM supervised entity configuration */
static const WdgM_SupervisedEntityConfigType wdgm_se_config[] = {
    { 0u, 1u, 1u, 3u },   /* SE 0: Swc_Pedal         — 1 checkpoint/cycle, 3 failures tolerated */
    { 1u, 1u, 1u, 3u },   /* SE 1: Swc_VehicleState  */
    { 2u, 1u, 1u, 3u },   /* SE 2: Swc_EStop         */
    { 3u, 1u, 3u, 3u },   /* SE 3: Swc_Heartbeat     — 1-3 checkpoints per 100ms WdgM cycle */
    { 4u, 1u, 1u, 5u },   /* SE 4: Swc_Dashboard     — more tolerant (QM) */
};

static const WdgM_ConfigType wdgm_config = {
    .seConfig      = wdgm_se_config,
    .seCount       = (uint8)(sizeof(wdgm_se_config) / sizeof(wdgm_se_config[0])),
    .wdtDioChannel = 6u,
};

/* ==================================================================
 * BswM Mode Actions (placeholder callbacks)
 * ================================================================== */

static void BswM_Action_Run(void)
{
    /* Enable motor, servo outputs */
}

static void BswM_Action_SafeStop(void)
{
    /* Safe-state actuation: CvcCom_TransmitSchedule handles cyclic
     * brake=100% TX via state-gate. No one-shot action needed here.
     * TODO:POST-BETA — add DTC reporting when CVC_DTC_SAFE_STOP_ENTRY
     * is defined in Cvc_Cfg.h. */
}

static void BswM_Action_Shutdown(void)
{
    /* Disable all outputs, stop watchdog feed */
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
 * Self-Test Sequence (SWR-CVC-029)
 * ================================================================== */

#ifndef PLATFORM_HIL
/**
 * @brief  Run CVC power-on self-test sequence
 * @return CVC_SELF_TEST_PASS if all tests pass, CVC_SELF_TEST_FAIL otherwise
 *
 * @safety_req SWR-CVC-029
 * @traces_to  SSR-CVC-029
 */
static uint8 Main_RunSelfTest(void)
{
    /* Plant stack canary for stack overflow detection */
    Main_Hw_PlantStackCanary();

    /* SPI loopback test */
    if (Main_Hw_SpiLoopbackTest() != E_OK)
    {
        Det_ReportRuntimeError(DET_MODULE_CVC_MAIN, 0u, MAIN_API_SELF_TEST, DET_E_DBG_SELF_TEST_FAIL);
        Dem_ReportErrorStatus(CVC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        return CVC_SELF_TEST_FAIL;
    }

    /* CAN loopback test */
    if (Main_Hw_CanLoopbackTest() != E_OK)
    {
        Det_ReportRuntimeError(DET_MODULE_CVC_MAIN, 0u, MAIN_API_SELF_TEST, DET_E_DBG_SELF_TEST_FAIL);
        Dem_ReportErrorStatus(CVC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        return CVC_SELF_TEST_FAIL;
    }

    /* OLED I2C ACK test */
    if (Main_Hw_OledAckTest() != E_OK)
    {
        Dem_ReportErrorStatus(CVC_DTC_DISPLAY_COMM, DEM_EVENT_STATUS_FAILED);
        /* Display fault is QM — do not fail self-test for it */
    }

    /* RAM pattern test */
    if (Main_Hw_RamPatternTest() != E_OK)
    {
        Det_ReportRuntimeError(DET_MODULE_CVC_MAIN, 0u, MAIN_API_SELF_TEST, DET_E_DBG_SELF_TEST_FAIL);
        Dem_ReportErrorStatus(CVC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        return CVC_SELF_TEST_FAIL;
    }

    Det_ReportRuntimeError(DET_MODULE_CVC_MAIN, 0u, MAIN_API_SELF_TEST, DET_E_DBG_SELF_TEST_PASS);
    return CVC_SELF_TEST_PASS;
}
#endif /* !PLATFORM_HIL */

/* ==================================================================
 * Tick Counters
 * ================================================================== */

static volatile uint32 tick_us;

/* ==================================================================
 * ThreadX Timer Callbacks (USE_THREADX only)
 * ================================================================== */

#ifdef USE_THREADX

void Timer_1ms_Callback(ULONG arg)
{
    (void)arg;
    Rte_MainFunction();
}

void Timer_10ms_Callback(ULONG arg)
{
    (void)arg;
    Swc_CvcCom_TransmitSchedule(Main_Hw_GetTick() / 1000u);
    Dcm_MainFunction();
    BswM_MainFunction();
    CanSM_MainFunction();
}

void Timer_100ms_Callback(ULONG arg)
{
    (void)arg;
    WdgM_MainFunction();
    Dem_MainFunction();
}

void Timer_5s_Callback(ULONG arg)
{
    (void)arg;
    Main_Hw_DebugPrintStatus(Main_Hw_GetTick());
}

#endif

/* ==================================================================
 * Main Entry Point
 * ================================================================== */

/**
 * @brief  CVC main function — init, self-test, main loop
 *
 * @safety_req SWR-CVC-029 to SWR-CVC-035
 * @traces_to  SSR-CVC-029 to SSR-CVC-035, TSR-046, TSR-047, TSR-048
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
    Det_ReportRuntimeError(DET_MODULE_CVC_MAIN, 0u, MAIN_API_INIT, DET_E_DBG_CAN_INIT_OK);
    CanIf_Init(&canif_config);
    PduR_Init(&cvc_pdur_config);
    CanTp_Init(&cvc_cantp_config);
    Com_Init(&cvc_com_config);
    E2E_Init();
    Dem_Init(NULL_PTR);
    Dem_SetEcuId(0x10u);                    /* CVC ECU ID for DTC broadcasts */
    Dem_SetBroadcastPduId(CVC_COM_TX_DTC);  /* CanIf TX for CAN 0x500 */
    /* Align CVC DTC numbering with the repo architecture families, then use
     * local extension codes where the current CVC event model is more detailed
     * than the shared architecture table. */
    Dem_SetDtcCode(CVC_DTC_PEDAL_PLAUSIBILITY, 0xC00100u);
    Dem_SetDtcCode(CVC_DTC_PEDAL_STUCK,        0xC00400u);
    Dem_SetDtcCode(CVC_DTC_PEDAL_RANGE,        0xC00500u);
    Dem_SetDtcCode(CVC_DTC_COMM_FZC_TIMEOUT,   0xC10100u);
    Dem_SetDtcCode(CVC_DTC_COMM_RZC_TIMEOUT,   0xC10200u);
    Dem_SetDtcCode(CVC_DTC_COMM_SC_TIMEOUT,    0xC10600u);
    Dem_SetDtcCode(CVC_DTC_VEHICLE_STATE,      0xC50200u);
    Dem_SetDtcCode(CVC_DTC_ESTOP_FAULT,        0xC40100u);
    Dem_SetDtcCode(CVC_DTC_MOTOR_OVERCURRENT,  0xC20100u);
    Dem_SetDtcCode(CVC_DTC_MOTOR_OVERSPEED,    0xC20200u);
    Dem_SetDtcCode(CVC_DTC_BATTERY_LOW,        0xC20400u);
    Dem_SetDtcCode(CVC_DTC_BATTERY_CRITICAL,   0xC20500u);
    Dem_SetDtcCode(CVC_DTC_STEERING_FAULT,     0xC30100u);
    Dem_SetDtcCode(CVC_DTC_BRAKE_FAULT,        0xC30400u);
    Dem_SetDtcCode(CVC_DTC_LIDAR_FAULT,        0xC30300u);
    Dem_SetDtcCode(CVC_DTC_CAN_BUS_OFF,        0xC10300u);
    Dem_SetDtcCode(CVC_DTC_SELF_TEST_FAIL,     0xC60100u);
    Dem_SetDtcCode(CVC_DTC_WATCHDOG_FAULT,     0xC50100u);
    Dem_SetDtcCode(CVC_DTC_SC_RELAY_KILL,      0xC40200u);
    Dem_SetDtcCode(CVC_DTC_DISPLAY_COMM,       0xC70100u);
    Dem_SetDtcCode(CVC_DTC_CREEP_FAULT,        0xC20300u);
    Dem_SetDtcCode(CVC_DTC_PEDAL_SENSOR1_FAIL, 0xC00200u);
    Dem_SetDtcCode(CVC_DTC_PEDAL_SENSOR2_FAIL, 0xC00300u);
    Dem_SetDtcCode(CVC_DTC_SAFE_STOP_ENTRY,    0xC50300u);
    Dem_SetDtcCode(CVC_DTC_MOTOR_CUTOFF_RX,    0xC10700u);
    Dem_SetDtcCode(CVC_DTC_BRAKE_FAULT_RX,     0xC30500u);
    Dem_SetDtcCode(CVC_DTC_STEERING_FAULT_RX,  0xC30200u);
    Dem_SetDtcCode(CVC_DTC_NVM_CRC_FAIL,       0xC60200u);
    CanSM_Init(&cansm_config);
    FiM_Init(&cvc_fim_config);
    Xcp_Init(&cvc_xcp_config);
    SchM_TimingInit();
    WdgM_Init(&wdgm_config);
    BswM_Init(&bswm_config);
    Dcm_Init(&cvc_dcm_config);
    Spi_Init(&spi_config);
    Adc_Init(&adc_config);
    Pwm_Init(&pwm_config);
    Dio_Init();
    IoHwAb_Init(&iohwab_config);
    Rte_Init(&cvc_rte_config);
    Det_ReportRuntimeError(DET_MODULE_CVC_MAIN, 0u, MAIN_API_INIT, DET_E_DBG_BSW_INIT_OK);

    /* ---- Step 3: SWC initialization ---- */
    (void)Ssd1306_Init();
    Swc_Pedal_Init(&pedal_config);
    Swc_VehicleState_Init();
    Swc_EStop_Init();
    Swc_Heartbeat_Init();
    Swc_Dashboard_Init();
    Swc_CvcCom_Init();
    Det_ReportRuntimeError(DET_MODULE_CVC_MAIN, 0u, MAIN_API_INIT, DET_E_DBG_SWC_INIT_OK);

    /* ---- Step 4: Self-test sequence ---- */
#ifdef PLATFORM_HIL
    /* HIL: skip self-test — CAN loopback can fail if bus has traffic
     * from other ECUs during boot. Force PASS. */
    self_test_result = CVC_SELF_TEST_PASS;
#else
    self_test_result = Main_RunSelfTest();
#endif

    if (self_test_result == CVC_SELF_TEST_PASS)
    {
        Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_PASS);
    }
    else
    {
        Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_FAIL);
    }

    /* ---- Step 5: Start CAN controller ---- */
    (void)Can_SetControllerMode(0u, CAN_CS_STARTED);

    /* ---- Step 6: Request BSW RUN mode (if self-test passed) ---- */
    if (self_test_result == CVC_SELF_TEST_PASS)
    {
        (void)BswM_RequestMode(0u, BSWM_RUN);
        Det_ReportRuntimeError(DET_MODULE_CVC_MAIN, 0u, MAIN_API_RUN, DET_E_DBG_STATE_RUN);
    }

    /* ---- Step 7: Start SysTick (1ms period = 1000us) ---- */
    Main_Hw_SysTickInit(1000u);
    Det_ReportRuntimeError(DET_MODULE_CVC_MAIN, 0u, MAIN_API_RUN, DET_E_DBG_SYSTICK_START);

    /* ---- Step 8: Main loop / RTOS kernel ---- */

#ifdef USE_THREADX
    tx_kernel_enter();
#else
    for (;;)
    {
        Main_Hw_Wfi();

        tick_us = Main_Hw_GetTick();

        /* 10ms tasks: bridge signals BEFORE RTE dispatches Com_MainFunction_Tx.
         * CvcCom_TransmitSchedule writes signal values to Com PDU buffers.
         * Com_MainFunction_Tx (dispatched by RTE at 10ms) reads those buffers.
         * If these ran after RTE, PERIODIC TX would send stale/zero values. */
        if ((tick_us - last_10ms_us) >= 10000u)
        {
            last_10ms_us = tick_us;
            Spi_Hw_PollUdp();  /* Drain UDP pedal/E-Stop override socket */
            Swc_CvcCom_BridgeRxToRte();
            Swc_CvcCom_TransmitSchedule(tick_us / 1000u);
            CanTp_MainFunction();
            Dcm_MainFunction();
            BswM_MainFunction();
            CanSM_MainFunction();
        }

        /* 1ms task: RTE scheduler (dispatches runnables internally)
         * Main_Hw_GetTick() returns microseconds; 1ms = 1000us */
        if ((tick_us - last_1ms_us) >= 1000u)
        {
            last_1ms_us = tick_us;
            Rte_MainFunction();
        }

        /* 100ms tasks: WdgM, Dem (DTC broadcast) */
        if ((tick_us - last_100ms_us) >= 100000u)
        {
            last_100ms_us = tick_us;
            WdgM_MainFunction();
            Dem_MainFunction();
            FiM_MainFunction();
        }

        /* 5s debug task */
        if ((tick_us - last_5s_us) >= 5000000u)
        {
            last_5s_us = tick_us;
            Main_Hw_DebugPrintStatus(tick_us);
#ifdef SIL_DIAG
            fprintf(stderr, "[MAIN] t=%us com_tx_calls=%u hb_sends=%u vs_sends=%u\n",
                (unsigned)(tick_us / 1000000u),
                (unsigned)g_dbg_com_tx_calls,
                (unsigned)com_tx_send_count[1],  /* heartbeat PDU */
                (unsigned)com_tx_send_count[2]);  /* vehicle state PDU */
#endif
        }
    }
#endif /* USE_THREADX else */

    /* MISRA: unreachable but satisfies compiler */
    return 0;
}
