/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include <stdio.h>
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
TX_THREAD can_rx_thread;
TX_EVENT_FLAGS_GROUP can_rx_event;
static uint32_t can_tx_count = 0;
static FDCAN_RxHeaderTypeDef can_rx_header;
static uint8_t can_rx_data[8];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void ThreadOne_Entry(ULONG thread_input);
void ThreadTwo_Entry(ULONG thread_input);
void App_Delay(uint32_t Delay);
static void CAN_Periodic_Callback(ULONG arg);
static void CAN_RX_Thread_Entry(ULONG arg);
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

  /* Create periodic CAN TX timer — 100 ticks = 1 second */
  tx_timer_create(&can_periodic_timer, "CAN_TX", CAN_Periodic_Callback,
                  0, 100, 100, TX_AUTO_ACTIVATE);

  /* Create CAN RX event flag + thread */
  tx_event_flags_create(&can_rx_event, "CAN_RX_EVT");

  if (tx_byte_allocate(byte_pool, (VOID**)&pointer,
                       TX_APP_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  tx_thread_create(&can_rx_thread, "CAN_RX", CAN_RX_Thread_Entry, 0,
                   pointer, TX_APP_STACK_SIZE, 3, 3,
                   TX_NO_TIME_SLICE, TX_AUTO_START);

  /* Enable FDCAN RX FIFO0 new message notification */
  extern FDCAN_HandleTypeDef hfdcan1;
  HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
  HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
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

  /* Infinite loop */
  while(1)
  {
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);

    /* Send CAN frame */
    extern FDCAN_HandleTypeDef hfdcan1;
    extern FDCAN_TxHeaderTypeDef txHeader;
    uint32_t tick = tx_time_get();
    uint8_t txData[4] = {(uint8_t)(tick >> 24), (uint8_t)(tick >> 16),
                         (uint8_t)(tick >> 8),  (uint8_t)(tick)};
    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, txData);

    printf("ThreadX tick=%lu CAN sent\r\n", (unsigned long)tick);

    /* Thread sleep for 1s */
    tx_thread_sleep(100);
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
    App_Delay(50);
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
    App_Delay(20);
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
  */
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

/**
  * @brief  HAL FDCAN RX FIFO0 callback — called from ISR
  */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE)
  {
    HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &can_rx_header, can_rx_data);
    tx_event_flags_set(&can_rx_event, 0x1, TX_OR);
  }
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
  * @brief  Periodic CAN TX from ThreadX timer — sends heartbeat every 1s
  */
static void CAN_Periodic_Callback(ULONG arg)
{
  (void)arg;
  extern FDCAN_HandleTypeDef hfdcan1;
  FDCAN_TxHeaderTypeDef hdr = {0};
  hdr.Identifier = 0x200;
  hdr.IdType = FDCAN_STANDARD_ID;
  hdr.TxFrameType = FDCAN_DATA_FRAME;
  hdr.DataLength = FDCAN_DLC_BYTES_8;
  hdr.FDFormat = FDCAN_CLASSIC_CAN;
  hdr.BitRateSwitch = FDCAN_BRS_OFF;
  hdr.TxEventFifoControl = FDCAN_NO_TX_EVENTS;

  can_tx_count++;
  uint8_t data[8] = {0};
  data[0] = (uint8_t)(can_tx_count >> 24);
  data[1] = (uint8_t)(can_tx_count >> 16);
  data[2] = (uint8_t)(can_tx_count >> 8);
  data[3] = (uint8_t)(can_tx_count);
  data[7] = 0x01; /* heartbeat flag */

  HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &hdr, data);
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
