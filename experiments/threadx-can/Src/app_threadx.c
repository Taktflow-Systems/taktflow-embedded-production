/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @brief   ThreadX + BSW integration experiment (Step 7)
  ******************************************************************************
  * @details Steps 1-6: ThreadX + HAL FDCAN TX/RX working.
  *          Step 7: BSW modules integrated incrementally:
  *            7a: Can.c + Can_Hw_STM32.c + SchM_ThreadX.c + Det.c
  *            7b: CanIf.c
  *            7c: Com.c + PduR.c + Com_Cfg_Fzc.c
  *            7d: Rte.c + Rte_Cfg_Fzc.c + Swc_Heartbeat.c
  *
  *          BSW modules use the production source files via relative paths.
  *          SchM uses ThreadX TX_DISABLE/TX_RESTORE instead of __disable_irq.
  *          Unused modules (Dcm, BswM, WdgM, etc.) are stubbed in bsw_stubs.c.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include <stdio.h>

/* BSW module headers */
#include "Std_Types.h"
#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"
#include "Rte.h"
#include "Det.h"
#include "Fzc_Cfg.h"
#include "Fzc_App.h"
#include "Swc_Heartbeat.h"
#include "Swc_Steering.h"
#include "Swc_Brake.h"
#include "Swc_Lidar.h"
#include "Swc_FzcSafety.h"
#include "Swc_FzcCom.h"
#include "Swc_FzcCanMonitor.h"
#include "Swc_Buzzer.h"
#include "Swc_FzcSensorFeeder.h"
#include "IoHwAb.h"
#include "Spi.h"
#include "Adc.h"
#include "Uart.h"
#include "CanTp.h"
#include "Dcm.h"
#include "E2E.h"
#include "BswM.h"
#include "Dem.h"
#include "WdgM.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TX_THREAD tx_app_thread;
/* USER CODE BEGIN PV */
TX_THREAD ThreadOne;
TX_THREAD ThreadTwo;
TX_EVENT_FLAGS_GROUP EventFlag;
TX_TIMER can_periodic_timer;
TX_TIMER bsw_1ms_timer;
static uint32_t can_tx_count = 0;

/* BSW configuration objects */
static const Can_ConfigType exp_can_config = {
    .baudrate     = 500000u,
    .controllerId = 0u,
};

/* Step 7 experiment configs removed — now using full FZC generated configs */

/* Full FZC configs from generated code */
extern const Com_ConfigType   fzc_com_config;
extern const Rte_ConfigType   fzc_rte_config;
extern const Dcm_ConfigType   fzc_dcm_config;
extern const CanIf_ConfigType fzc_canif_config;
extern const PduR_ConfigType  fzc_pdur_config;
extern const CanTp_ConfigType fzc_cantp_config;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void ThreadOne_Entry(ULONG thread_input);
void ThreadTwo_Entry(ULONG thread_input);
void App_Delay(uint32_t Delay);
static void CAN_Periodic_Callback(ULONG arg);
static void BSW_1ms_Callback(ULONG arg);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */

  /* USER CODE END App_ThreadX_MEM_POOL */
  CHAR *pointer;

  /* Allocate the stack for Main Thread  */
  if (tx_byte_allocate(byte_pool, (VOID**) &pointer,
                       TX_APP_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  /* Create Main Thread.  */
  if (tx_thread_create(&tx_app_thread, "Main Thread", MainThread_Entry, 0, pointer,
                       TX_APP_STACK_SIZE, TX_APP_THREAD_PRIO, TX_APP_THREAD_PREEMPTION_THRESHOLD,
                       TX_APP_THREAD_TIME_SLICE, TX_APP_THREAD_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* USER CODE BEGIN App_ThreadX_Init */
  /* Allocate the stack for ThreadOne.  */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,
                       TX_APP_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create ThreadOne.  */
  if (tx_thread_create(&ThreadOne, "Thread One", ThreadOne_Entry, 0, pointer,
                       TX_APP_STACK_SIZE, THREAD_ONE_PRIO, THREAD_ONE_PREEMPTION_THRESHOLD,
                       TX_APP_THREAD_TIME_SLICE, TX_APP_THREAD_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* Allocate the stack for ThreadTwo.  */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,
                       TX_APP_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create ThreadTwo.  */
  if (tx_thread_create(&ThreadTwo, "Thread Two", ThreadTwo_Entry, 0, pointer,
                       TX_APP_STACK_SIZE, THREAD_TWO_PRIO, THREAD_TWO_PREEMPTION_THRESHOLD,
                       TX_APP_THREAD_TIME_SLICE, TX_APP_THREAD_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* Create the event flags group.  */
  if (tx_event_flags_create(&EventFlag, "Event Flag") != TX_SUCCESS)
  {
    return TX_GROUP_ERROR;
  }

  /* ====================================================================
   * Step 7: BSW Module Initialization (order matters)
   * ==================================================================== */

  /* 7a: Det + Can MCAL + SchM */
  /* Step 7a: Det + Can */
  Det_Init();
  Can_Init(&exp_can_config);

  /* Step 7b: CanIf (full FZC config) */
  CanIf_Init(&fzc_canif_config);

  /* Step 7c: PduR + Com (full FZC configs) */
  PduR_Init(&fzc_pdur_config);
  Com_Init(&fzc_com_config);
  E2E_Init();

  /* Step 8: UDS diagnostics */
  Dem_Init(NULL_PTR);
  CanTp_Init(&fzc_cantp_config);
  Dcm_Init(&fzc_dcm_config);
  BswM_Init(NULL_PTR);
  WdgM_Init(NULL_PTR);

  /* Step 7d: Rte */
  Rte_Init(&fzc_rte_config);

  /* Step 9: All FZC SWCs + MCAL peripherals */
  Spi_Init(NULL_PTR);
  Adc_Init(NULL_PTR);
  {
    /* Minimal IoHwAb config — marks module as initialized so reads don't fail.
     * All channel values are 0 — reads will return ADC/SPI zeros but E_OK. */
    static const IoHwAb_ConfigType iohwab_bench_cfg = {0};
    IoHwAb_Init(&iohwab_bench_cfg);
  }
  Uart_Init(NULL_PTR);
  Swc_Heartbeat_Init();
  Swc_Steering_Init(NULL_PTR);
  Swc_Brake_Init(NULL_PTR);
  Swc_Lidar_Init(NULL_PTR);
  Swc_FzcCom_Init();
  Swc_FzcCanMonitor_Init();
  Swc_FzcSafety_Init();
  Swc_Buzzer_Init();
  Swc_FzcSensorFeeder_Init();

  /* Start CAN controller via BSW API */
  (void)Can_SetControllerMode(0u, CAN_CS_STARTED);

  /* 10ms periodic timer for BSW cyclic services */
  tx_timer_create(&can_periodic_timer, "BSW_10MS", CAN_Periodic_Callback,
                  0, 10, 10, TX_AUTO_ACTIVATE);

  /* 1ms periodic timer for RTE scheduler */
  tx_timer_create(&bsw_1ms_timer, "BSW_1MS", BSW_1ms_Callback,
                  0, 1, 1, TX_AUTO_ACTIVATE);

  /* ISR-based CAN RX disabled — BSW polling (Can_MainFunction_Read) handles all RX.
   * The ISR was consuming frames from FIFO before BSW could poll them. */
#if 0  /* Disabled for Step 9 — BSW polling mode */
  tx_event_flags_create(&can_rx_event, "CAN_RX_EVT");

  if (tx_byte_allocate(byte_pool, (VOID**)&pointer,
                       TX_APP_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  tx_thread_create(&can_rx_thread, "CAN_RX", CAN_RX_Thread_Entry, 0,
                   pointer, TX_APP_STACK_SIZE, 3, 3,
                   TX_NO_TIME_SLICE, TX_AUTO_START);

  /* ISR-based RX disabled — BSW polling mode */
#endif  /* Disabled for Step 9 */
  /* USER CODE END App_ThreadX_Init */

  return ret;
}
/**
  * @brief  Function implementing the MainThread_Entry thread.
  * @param  thread_input: Hardcoded to 0.
  * @retval None
  */
void MainThread_Entry(ULONG thread_input)
{
  /* USER CODE BEGIN MainThread_Entry */
  UINT old_prio = 0;
  UINT old_pre_threshold = 0;
  ULONG   actual_flags = 0;
  uint8_t count = 0;
  (void) thread_input;

  while (count < 3)
  {
    count++;
    if (tx_event_flags_get(&EventFlag, THREAD_ONE_EVT, TX_OR_CLEAR,
                           &actual_flags, TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      Error_Handler();
    }
    else
    {
      /* Update the priority and preemption threshold of ThreadTwo
      to allow the preemption of ThreadOne */
      tx_thread_priority_change(&ThreadTwo, NEW_THREAD_TWO_PRIO, &old_prio);
      tx_thread_preemption_change(&ThreadTwo, NEW_THREAD_TWO_PREEMPTION_THRESHOLD, &old_pre_threshold);

      if (tx_event_flags_get(&EventFlag, THREAD_TWO_EVT, TX_OR_CLEAR,
                             &actual_flags, TX_WAIT_FOREVER) != TX_SUCCESS)
      {
        Error_Handler();
      }
      else
      {
        /* Reset the priority and preemption threshold of ThreadTwo */
        tx_thread_priority_change(&ThreadTwo, THREAD_TWO_PRIO, &old_prio);
        tx_thread_preemption_change(&ThreadTwo, THREAD_TWO_PREEMPTION_THRESHOLD, &old_pre_threshold);
      }
    }
  }

  /* Destroy ThreadOne and ThreadTwo */
  tx_thread_terminate(&ThreadOne);
  tx_thread_terminate(&ThreadTwo);

  /* Infinite loop — periodic status print */
  while(1)
  {
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);

    {
      extern volatile uint32 g_dbg_com_e2e_rx_fail[];
      extern volatile uint32 g_can_rx_count;
      uint32 vs = 99u;
      (void)Rte_Read(FZC_SIG_VEHICLE_STATE, &vs);
      printf("t=%lu rx=%lu vs=%lu e2eF=%lu\r\n",
             (unsigned long)tx_time_get(),
             (unsigned long)g_can_rx_count,
             (unsigned long)vs,
             (unsigned long)g_dbg_com_e2e_rx_fail[FZC_COM_RX_VEHICLE_STATE]);
    }

    /* Thread sleep for 5s (5000 ticks at 1000Hz) */
    tx_thread_sleep(5000);
  }
  /* USER CODE END MainThread_Entry */
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */

  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
/**
  * @brief  Function implementing the ThreadOne thread.
  * @param  thread_input: Not used
  * @retval None
  */
void ThreadOne_Entry(ULONG thread_input)
{
  (void) thread_input;
  uint8_t count = 0;
  /* Infinite loop */
  while(1)
  {
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);

    /* Delay for 500ms (App_Delay is used to avoid context change). */
    App_Delay(500);
    count ++;
    if (count == 10)
    {
      count = 0;
      if (tx_event_flags_set(&EventFlag, THREAD_ONE_EVT, TX_OR) != TX_SUCCESS)
      {
        Error_Handler();
      }
    }
  }
}

/**
  * @brief  Function implementing the ThreadTwo thread.
  * @param  thread_input: Not used
  * @retval None
  */
void ThreadTwo_Entry(ULONG thread_input)
{
  (void) thread_input;
  uint8_t count = 0;
  /* Infinite loop */
  while (1)
  {
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
    /* Delay for 200ms (App_Delay is used to avoid context change). */
    App_Delay(200);
    count ++;
    if (count == 25)
    {
      count = 0;
      if (tx_event_flags_set(&EventFlag, THREAD_TWO_EVT, TX_OR) != TX_SUCCESS)
      {
        Error_Handler();
      }
    }
  }
}

/**
  * @brief  CAN RX thread — waits for ISR event, prints received frame
  *         Disabled for Step 9 — BSW polling mode
  */
#if 0
static void CAN_RX_Thread_Entry(ULONG arg)
{
  (void)arg;
  ULONG actual;
  while (1)
  {
    if (tx_event_flags_get(&can_rx_event, 0x1, TX_AND_CLEAR,
                           &actual, TX_WAIT_FOREVER) == TX_SUCCESS)
    {
      printf("CAN RX: ID=0x%03lX DLC=%lu Data=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
             (unsigned long)can_rx_header.Identifier,
             (unsigned long)(can_rx_header.DataLength >> 16),
             can_rx_data[0], can_rx_data[1], can_rx_data[2], can_rx_data[3],
             can_rx_data[4], can_rx_data[5], can_rx_data[6], can_rx_data[7]);
    }
  }
}

#endif /* ISR RX disabled */

/**
  * @brief  HAL FDCAN RX FIFO0 callback — disabled, BSW polling mode
  */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  /* Disabled — BSW polling via Can_MainFunction_Read handles all RX.
   * Do NOT read from FIFO here — it would consume frames before BSW polls. */
  (void)hfdcan;
  (void)RxFifo0ITs;
}

/**
  * @brief  FDCAN1 IT0 IRQ handler
  */
void FDCAN1_IT0_IRQHandler(void)
{
  extern FDCAN_HandleTypeDef hfdcan1;
  HAL_FDCAN_IRQHandler(&hfdcan1);
}

/**
  * @brief  Periodic CAN TX from ThreadX timer — 1s heartbeat via BSW stack
  * @note   Step 7a+: Uses Can_MainFunction_Read + Com_MainFunction_Tx
  *         instead of direct HAL calls. Also sends a raw BSW heartbeat
  *         as a diagnostic marker.
  */
static void CAN_Periodic_Callback(ULONG arg)
{
  (void)arg;

  can_tx_count++;

  /* Bus-off auto-recovery — only re-init, DON'T run Com (avoids extra TX) */
  if ((FDCAN1->PSR & 0x80u) != 0u)
  {
    extern FDCAN_HandleTypeDef hfdcan1;
    HAL_FDCAN_Stop(&hfdcan1);
    HAL_FDCAN_DeInit(&hfdcan1);
    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_FDCAN_CONFIG(RCC_FDCANCLKSOURCE_PCLK1);
    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = ENABLE;
    hfdcan1.Init.TransmitPause = DISABLE;
    hfdcan1.Init.ProtocolException = DISABLE;
    hfdcan1.Init.NominalPrescaler = 34;
    hfdcan1.Init.NominalSyncJumpWidth = 1;
    hfdcan1.Init.NominalTimeSeg1 = 7;
    hfdcan1.Init.NominalTimeSeg2 = 2;
    hfdcan1.Init.DataPrescaler = 34;
    hfdcan1.Init.DataSyncJumpWidth = 1;
    hfdcan1.Init.DataTimeSeg1 = 7;
    hfdcan1.Init.DataTimeSeg2 = 2;
    hfdcan1.Init.StdFiltersNbr = 0;
    hfdcan1.Init.ExtFiltersNbr = 0;
    hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    HAL_FDCAN_Init(&hfdcan1);
    HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);
    HAL_FDCAN_Start(&hfdcan1);
    return;  /* Skip Com/BSW this cycle — just recovered */
  }

  /* Can_MainFunction_Read, Com_MainFunction_Tx/Rx, Can_MainFunction_BusOff
   * are dispatched by Rte_MainFunction via the runnable table.
   * Do NOT call them here — double-calling doubles the TX rate. */
  CanTp_MainFunction();
  Dcm_MainFunction();
  BswM_MainFunction();
}

/**
  * @brief  1ms BSW timer callback — drives RTE scheduler
  * @note   Step 7d: Calls Rte_MainFunction which dispatches all configured
  *         runnables (including Swc_Heartbeat_MainFunction every 50ms).
  */
static void BSW_1ms_Callback(ULONG arg)
{
  (void)arg;
  Rte_MainFunction();
}

/**
  * @brief  Application Delay function.
  * @param  Delay : number of ticks to wait
  * @retval None
  */
void App_Delay(uint32_t Delay)
{
  UINT initial_time = tx_time_get();
  while ((tx_time_get() - initial_time) < Delay);
}
/* USER CODE END 1 */
