---
document_id: BSW-ARCH
title: "BSW Architecture"
version: "1.0"
status: draft
aspice_process: SWE.2
date: 2026-02-21
---

## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.

## Lessons Learned Rule

Every BSW element in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/software/lessons-learned/`](../lessons-learned/). One file per BSW element. File naming: `BSW-ARCH-<element>.md`.


# BSW Architecture — Taktflow Zonal Vehicle Platform

## 1. Purpose

This document specifies the detailed architecture of the AUTOSAR Classic-inspired Basic Software (BSW) layer shared across the three STM32G474RE-based zone controllers (CVC, FZC, RZC). The Safety Controller (SC, TMS570LC43x) does not use this BSW stack — it runs independent bare-metal firmware as required by ISO 26262 diverse redundancy.

The BSW layer provides:
- Hardware abstraction through MCAL drivers (platform-portable)
- Communication stack (CAN driver to signal-based Com)
- Diagnostic services (UDS via Dcm, DTC management via Dem)
- Safety supervision (watchdog management via WdgM, E2E protection)
- Mode management (BSW mode transitions via BswM)
- Runtime environment (port-based SWC communication via RTE)

The same BSW source code compiles for three platform targets:
- **STM32** (production): STM32 HAL wrappers in MCAL
- **TMS570** (SC CAN only): DCAN register-level driver (limited use)
- **POSIX** (simulation): SocketCAN stubs for Docker/SIL testing

<!-- HITL-LOCK START:COMMENT-BLOCK-BSWARCH-SEC1 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The purpose correctly scopes the BSW architecture to the three STM32-based zone controllers and explicitly excludes the SC (TMS570), which is consistent with the diverse redundancy requirement from ISO 26262 Part 9. The six bullet points accurately enumerate the BSW layer responsibilities. The three-platform compilation strategy (STM32/TMS570/POSIX) enables SIL testing on host, which is a strong architectural decision. One observation: the TMS570 entry says "SC CAN only: DCAN register-level driver (limited use)" but the SC does not use the shared BSW stack at all according to sw-architecture.md -- this may cause confusion. The TMS570 platform target in BSW context is for the Can module only, used as a standalone driver in the SC's bare-metal firmware, not as part of the BSW layered stack.
<!-- HITL-LOCK END:COMMENT-BLOCK-BSWARCH-SEC1 -->

## 2. BSW Module Dependency Graph

```
+===========================================================================+
|                                                                           |
|  APPLICATION LAYER                                                        |
|  +----------+ +----------+ +----------+ +----------+                      |
|  | Swc_Pedal| | Swc_Motor| | Swc_Steer| | Swc_Lidar|  ...               |
|  +-----+----+ +-----+----+ +-----+----+ +-----+----+                     |
|        |             |             |             |                         |
|========|=============|=============|=============|=========================|
|        v             v             v             v                         |
|  +-----+-------------+-------------+-------------+----+                   |
|  |                    RTE                              |                   |
|  |  Rte_Read_xxx() / Rte_Write_xxx()                  |                   |
|  |  Rte_Call_xxx() (client-server)                     |                   |
|  +---+---------+---------+---------+---------+---------+                   |
|      |         |         |         |         |                             |
|======|=========|=========|=========|=========|=============================|
|      v         v         v         v         v                             |
|  +---+--+  +--+---+  +--+---+  +--+---+  +--+---+  +------+             |
|  | Com  |  | Dcm  |  | Dem  |  | WdgM |  | BswM |  | E2E  |             |
|  |      |  |      |  |      |  |      |  |      |  |      |             |
|  | pack/|  | UDS  |  | DTC  |  | alive|  | mode |  | CRC  |             |
|  |unpack|  | svc  |  | store|  | super|  | mgmt |  | alive|             |
|  +--+---+  +--+---+  +--+---+  +--+---+  +--+---+  +--+---+             |
|     |         |         |         |         |           |                  |
|     |    E2E wraps      |    Dem reports    |      E2E called             |
|     |    Com signals    |    to BswM        |      by Com                 |
|     |         |         |         |         |           |                  |
|=====|=========|=========|=========|=========|===========|==================|
|     v         v         |         v         |           |                  |
|  +--+---+  +--+---+    |    +----+----+    |           |                  |
|  | PduR |  | PduR |    |    | IoHwAb  |    |           |                  |
|  | (Com)|  | (Dcm)|    |    | sensor/ |    |           |                  |
|  +--+---+  +--+---+    |    | actuator|    |           |                  |
|     |         |         |    +--+--+--+    |           |                  |
|     +----+----+         |       |  |  |    |           |                  |
|          |              |       |  |  |    |           |                  |
|==========|==============|=======|==|==|====|===========|==================|
|          v              |       v  v  v    v           |                  |
|  +-------+-------+     |  +----++ ++-+  +-+--+ +--+--+                  |
|  |    CanIf      |     |  | Spi | |Adc| | Pwm| |Dio |                   |
|  | HW-indep CAN  |     |  +--+-+ +-+--+ +--+-+ +-+--+                  |
|  +-------+-------+     |     |     |        |     |                      |
|          |              |     |     |        |     |                      |
|==========|==============|=====|=====|========|=====|======================|
|          v              |     v     v        v     v       v              |
|  +-------+-------+     |  +--+--+--+--+-----+-----+--+ +--+---+         |
|  |     Can       |     |  |     STM32 HAL            | |  Gpt  |        |
|  | FDCAN / DCAN  |     |  | HAL_SPI, HAL_ADC,        | | SysTick|       |
|  | / SocketCAN   |     |  | HAL_TIM_PWM, HAL_GPIO    | +---+---+        |
|  +-------+-------+     |  +-------------------------+      |             |
|          |              |                                    |             |
|==========|==============|====================================|=============|
|          v              v                                    v             |
|  +-------+-------+-----+------------------------------------+-------+     |
|  |                    HARDWARE                                       |    |
|  |  FDCAN1      SPI1/SPI2    ADC1      TIM2/3/4    GPIO    SysTick  |    |
|  +-------------------------------------------------------------------+    |
+===========================================================================+

Legend:
  ===  Layer boundary
  -->  Function call direction (caller to callee)
  <--  Callback / notification (callee to caller)
```

### Dependency Summary Table

| Module | Depends On | Called By |
|--------|-----------|----------|
| Can | STM32 HAL / DCAN registers / SocketCAN | CanIf |
| Spi | STM32 HAL_SPI | IoHwAb |
| Adc | STM32 HAL_ADC | IoHwAb |
| Pwm | STM32 HAL_TIM | IoHwAb |
| Dio | STM32 HAL_GPIO | IoHwAb, WdgM, BswM, Swc_EStop (via RTE) |
| Gpt | SysTick | All (provides time base) |
| CanIf | Can | PduR |
| PduR | CanIf | Com, Dcm |
| IoHwAb | Spi, Adc, Pwm, Dio | RTE (on behalf of SWCs) |
| Com | PduR, E2E | RTE (on behalf of SWCs) |
| Dcm | PduR, Dem | External UDS tester (via CAN) |
| Dem | — (standalone storage) | SWCs (via Dem_ReportErrorStatus), Dcm, BswM |
| WdgM | Dio, Gpt | RTE (checkpoint calls from SWCs) |
| BswM | Dem, Com, WdgM | main.c (mode transitions) |
| E2E | — (pure computation) | Com (wraps/checks signals) |
| Rte | Com, IoHwAb, Dem | SWCs (all port access) |

<!-- HITL-LOCK START:COMMENT-BLOCK-BSWARCH-SEC2 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The dependency graph is well-drawn with clear layer boundaries and call direction. The dependency summary table correctly identifies all 16 modules with their dependencies and callers. The layering discipline is maintained: no module calls upward except through callback/notification patterns (e.g., CanIf_RxIndication). The E2E module is correctly shown as "pure computation" with no dependencies, called by Com for signal protection. One observation: Dio is listed as called by "Swc_EStop (via RTE)" which is technically correct but could be misleading -- Swc_EStop reads a GPIO via the ISR mechanism (EXTI), which is a hardware interrupt rather than a polled Dio read. The dependency table should clarify whether the E-stop path uses Dio_ReadChannel or a direct EXTI ISR handler.
<!-- HITL-LOCK END:COMMENT-BLOCK-BSWARCH-SEC2 -->

## 3. Module Specifications

### 3.1 Can — CAN Driver (MCAL)

- **Purpose**: Low-level CAN peripheral access (transmit, receive, bus-off handling)
- **AUTOSAR reference**: AUTOSAR_SWS_CANDriver (SWS_Can)
- **Platform variants**: STM32 (FDCAN HAL in classic mode), TMS570 (DCAN register), POSIX (SocketCAN)

#### API Functions

```c
void        Can_Init(const Can_ConfigType* ConfigPtr);
void        Can_DeInit(void);
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);
void        Can_MainFunction_Read(void);      /* polled RX processing */
void        Can_MainFunction_BusOff(void);    /* bus-off recovery     */
void        Can_MainFunction_Mode(void);      /* mode transition      */
Can_StateType  Can_GetControllerMode(uint8 Controller);
```

#### Dependencies

- **Down**: STM32 HAL (`HAL_FDCAN_Start`, `HAL_FDCAN_AddMessageToTxFifoQ`, `HAL_FDCAN_GetRxMessage`) or TMS570 DCAN registers or POSIX `socket(PF_CAN, ...)` / `write()` / `read()`
- **Up**: CanIf (via `CanIf_RxIndication` callback)

#### Configuration

```c
/* firmware/{ecu}/cfg/Can_Cfg.h */
#define CAN_CONTROLLER_ID       0u
#define CAN_BAUDRATE            500000u       /* 500 kbps */
#define CAN_MAX_TX_QUEUE        8u
#define CAN_MAX_RX_FIFO         16u
#define CAN_BUSOFF_RECOVERY_MS  100u
```

---

### 3.2 Spi — SPI Driver (MCAL)

- **Purpose**: SPI peripheral access for AS5048A magnetic angle sensors and SSD1306 OLED
- **AUTOSAR reference**: AUTOSAR_SWS_SPIHandlerDriver (SWS_Spi)
- **Platform variants**: STM32 (HAL_SPI), POSIX (stub — returns simulated values)

#### API Functions

```c
void        Spi_Init(const Spi_ConfigType* ConfigPtr);
void        Spi_DeInit(void);
Std_ReturnType Spi_WriteIB(Spi_ChannelType Channel, const Spi_DataBufferType* DataBufferPtr);
Std_ReturnType Spi_ReadIB(Spi_ChannelType Channel, Spi_DataBufferType* DataBufferPointer);
Std_ReturnType Spi_SyncTransmit(Spi_SequenceType Sequence);
Std_ReturnType Spi_AsyncTransmit(Spi_SequenceType Sequence);
Spi_StatusType Spi_GetStatus(void);
```

#### Dependencies

- **Down**: STM32 HAL (`HAL_SPI_TransmitReceive`, `HAL_SPI_TransmitReceive_DMA`)
- **Up**: IoHwAb

#### Configuration

```c
/* firmware/{ecu}/cfg/Spi_Cfg.h */
#define SPI_CHANNEL_PEDAL_SENSOR1   0u    /* CVC: SPI1, CS=PA4 */
#define SPI_CHANNEL_PEDAL_SENSOR2   1u    /* CVC: SPI1, CS=PA5 */
#define SPI_CHANNEL_ANGLE_SENSOR    2u    /* FZC: SPI2, CS=PB12 */
#define SPI_BAUDRATE_AS5048A        10000000u  /* 10 MHz */
#define SPI_CPOL                    0u    /* AS5048A: mode 1 */
#define SPI_CPHA                    1u
#define SPI_FRAME_SIZE              16u   /* 16-bit frames */
```

---

### 3.3 Adc — ADC Driver (MCAL)

- **Purpose**: ADC peripheral access for analog sensor readings (current, temperature, battery voltage)
- **AUTOSAR reference**: AUTOSAR_SWS_ADCDriver (SWS_Adc)
- **Platform variants**: STM32 (HAL_ADC with DMA scan), POSIX (stub — returns configurable test values)

#### API Functions

```c
void        Adc_Init(const Adc_ConfigType* ConfigPtr);
void        Adc_DeInit(void);
void        Adc_StartGroupConversion(Adc_GroupType Group);
void        Adc_StopGroupConversion(Adc_GroupType Group);
Std_ReturnType Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType* DataBufferPtr);
Adc_StatusType Adc_GetGroupStatus(Adc_GroupType Group);
void        Adc_MainFunction(void);           /* DMA complete processing */
```

#### Dependencies

- **Down**: STM32 HAL (`HAL_ADC_Start_DMA`, `HAL_ADC_ConvCpltCallback`)
- **Up**: IoHwAb

#### Configuration

```c
/* firmware/rzc/cfg/Adc_Cfg.h */
#define ADC_GROUP_MOTOR_SENSORS     0u
#define ADC_CHANNEL_MOTOR_CURRENT   0u    /* ACS723 on PA0 (ADC1_IN1) */
#define ADC_CHANNEL_MOTOR_TEMP      1u    /* NTC on PA1 (ADC1_IN2) */
#define ADC_CHANNEL_BATTERY_VOLTAGE 2u    /* Voltage divider on PA2 (ADC1_IN3) */
#define ADC_RESOLUTION              12u   /* 12-bit: 0..4095 */
#define ADC_VREF_MV                 3300u /* 3.3V reference */
#define ADC_DMA_BUFFER_SIZE         3u    /* 3 channels in scan group */
```

---

### 3.4 Pwm — PWM Driver (MCAL)

- **Purpose**: Timer-based PWM output for motor driver (BTS7960) and servo actuators
- **AUTOSAR reference**: AUTOSAR_SWS_PWMDriver (SWS_Pwm)
- **Platform variants**: STM32 (HAL_TIM_PWM), POSIX (stub — logs duty cycle)

#### API Functions

```c
void        Pwm_Init(const Pwm_ConfigType* ConfigPtr);
void        Pwm_DeInit(void);
void        Pwm_SetDutyCycle(Pwm_ChannelType ChannelNumber, uint16 DutyCycle);
void        Pwm_SetPeriodAndDuty(Pwm_ChannelType ChannelNumber, Pwm_PeriodType Period, uint16 DutyCycle);
void        Pwm_DisableNotification(Pwm_ChannelType ChannelNumber);
void        Pwm_SetOutputToIdle(Pwm_ChannelType ChannelNumber);
```

`DutyCycle` is specified in 0.01% resolution: 0 = 0%, 10000 = 100%.

#### Dependencies

- **Down**: STM32 HAL (`HAL_TIM_PWM_Start`, `__HAL_TIM_SET_COMPARE`)
- **Up**: IoHwAb

#### Configuration

```c
/* firmware/rzc/cfg/Pwm_Cfg.h */
#define PWM_CHANNEL_MOTOR_RPWM      0u    /* TIM3_CH1, BTS7960 RPWM */
#define PWM_CHANNEL_MOTOR_LPWM      1u    /* TIM3_CH2, BTS7960 LPWM */
#define PWM_MOTOR_FREQUENCY_HZ      20000u /* 20 kHz (above audible) */

/* firmware/fzc/cfg/Pwm_Cfg.h */
#define PWM_CHANNEL_STEERING_SERVO  0u    /* TIM2_CH1 */
#define PWM_CHANNEL_BRAKE_SERVO     1u    /* TIM2_CH2 */
#define PWM_SERVO_FREQUENCY_HZ      50u   /* 50 Hz standard servo */
#define PWM_SERVO_MIN_US            1000u /* 1 ms = min angle */
#define PWM_SERVO_MAX_US            2000u /* 2 ms = max angle */
```

---

### 3.5 Dio — Digital I/O Driver (MCAL)

- **Purpose**: GPIO read/write for discrete I/O (E-stop, relay, LEDs, buzzer, enable pins)
- **AUTOSAR reference**: AUTOSAR_SWS_DIODriver (SWS_Dio)
- **Platform variants**: STM32 (HAL_GPIO), TMS570 (GIO registers), POSIX (stub)

#### API Functions

```c
void        Dio_Init(const Dio_ConfigType* ConfigPtr);
Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId);
void        Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level);
Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId);
void        Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);
void        Dio_FlipChannel(Dio_ChannelType ChannelId);
```

#### Dependencies

- **Down**: STM32 HAL (`HAL_GPIO_ReadPin`, `HAL_GPIO_WritePin`, `HAL_GPIO_TogglePin`)
- **Up**: IoHwAb, WdgM (watchdog pin toggle), Swc_EStop (via RTE/IoHwAb)

#### Configuration

```c
/* firmware/cvc/cfg/Dio_Cfg.h */
#define DIO_CHANNEL_ESTOP_INPUT     0u    /* PC13, active low, EXTI */
#define DIO_CHANNEL_LED_FAULT       1u    /* PB0, output */
#define DIO_CHANNEL_LED_STATUS      2u    /* PB1, output */
#define DIO_CHANNEL_WDT_FEED        3u    /* PA8, TPS3823 WDI */

/* firmware/rzc/cfg/Dio_Cfg.h */
#define DIO_CHANNEL_MOTOR_ENABLE    0u    /* PB4, BTS7960 EN */
#define DIO_CHANNEL_MOTOR_DIR       1u    /* PB5, direction select */
#define DIO_CHANNEL_WDT_FEED        2u    /* PA8, TPS3823 WDI */
```

---

### 3.6 Gpt — General Purpose Timer (MCAL)

- **Purpose**: System tick generation and time base for all periodic scheduling
- **AUTOSAR reference**: AUTOSAR_SWS_GPTDriver (SWS_Gpt)
- **Platform variants**: STM32 (SysTick), TMS570 (RTI), POSIX (`clock_gettime`)

#### API Functions

```c
void        Gpt_Init(const Gpt_ConfigType* ConfigPtr);
void        Gpt_DeInit(void);
Gpt_ValueType Gpt_GetTimeElapsed(Gpt_ChannelType Channel);
uint32      Gpt_GetTickCount_Ms(void);       /* millisecond counter */
void        Gpt_MainFunction(void);           /* called from SysTick ISR */
```

#### Dependencies

- **Down**: STM32 SysTick (`SysTick_Handler`), TMS570 RTI (`rtiNotification`)
- **Up**: All modules (time base), WdgM (deadline monitoring), Com (Rx timeout)

#### Configuration

```c
/* firmware/shared/bsw/include/Gpt_Cfg.h */
#define GPT_TICK_PERIOD_US          1000u  /* 1 ms tick */
#define GPT_CHANNEL_SYSTEM          0u
```

---

### 3.7 CanIf — CAN Interface (ECU Abstraction)

- **Purpose**: Hardware-independent CAN interface; maps CAN hardware objects to upper-layer PDU IDs and routes between Can driver and PduR
- **AUTOSAR reference**: AUTOSAR_SWS_CANInterface (SWS_CanIf)

#### API Functions

```c
void        CanIf_Init(const CanIf_ConfigType* ConfigPtr);
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);
void        CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr);
void        CanIf_TxConfirmation(PduIdType TxPduId);
void        CanIf_ControllerBusOff(uint8 ControllerId);
Std_ReturnType CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode);
```

#### Dependencies

- **Down**: Can (calls `Can_Write` for TX, receives `CanIf_RxIndication` callback from Can for RX)
- **Up**: PduR (calls `PduR_CanIfRxIndication` for received PDUs)

#### Configuration

```c
/* firmware/{ecu}/cfg/CanIf_Cfg.h */
typedef struct {
    uint32      CanId;          /* CAN message ID */
    PduIdType   UpperPduId;     /* PduR PDU ID */
    uint8       Dlc;            /* Data Length Code */
    boolean     IsExtended;     /* Extended ID (FALSE for all messages) */
} CanIf_RxPduConfigType;

typedef struct {
    uint32      CanId;          /* CAN message ID */
    PduIdType   UpperPduId;     /* PduR PDU ID */
    uint8       Dlc;
    Can_HwHandleType Hth;       /* Hardware Transmit Handle */
} CanIf_TxPduConfigType;
```

#### Platform variants

CanIf is platform-independent. It calls the Can driver API, which handles platform differences.

---

### 3.8 PduR — PDU Router (ECU Abstraction)

- **Purpose**: Routes Protocol Data Units (PDUs) between lower layer (CanIf) and upper layers (Com, Dcm) based on a static routing table
- **AUTOSAR reference**: AUTOSAR_SWS_PDURouter (SWS_PduR)

#### API Functions

```c
void        PduR_Init(const PduR_ConfigType* ConfigPtr);
Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);
void        PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
void        PduR_CanIfTxConfirmation(PduIdType TxPduId);
```

#### Routing Logic

```
TX Path:
  Com_Transmit(ComPduId)
    --> PduR_Transmit(PduR_ComTxPduId)
      --> CanIf_Transmit(CanIfTxPduId)
        --> Can_Write(Hth, PduInfo)

  Dcm_Transmit(DcmPduId)
    --> PduR_Transmit(PduR_DcmTxPduId)
      --> CanIf_Transmit(CanIfTxPduId)
        --> Can_Write(Hth, PduInfo)

RX Path:
  Can_MainFunction_Read()
    --> CanIf_RxIndication(CanId, PduInfo)
      --> PduR_CanIfRxIndication(RxPduId, PduInfo)
        --> if (RxPduId is Com PDU): Com_RxIndication(ComRxPduId, PduInfo)
        --> if (RxPduId is Dcm PDU): Dcm_RxIndication(DcmRxPduId, PduInfo)
```

#### Dependencies

- **Down**: CanIf (`CanIf_Transmit`)
- **Up**: Com (`Com_RxIndication`), Dcm (`Dcm_RxIndication`)

#### Configuration

```c
/* firmware/{ecu}/cfg/PduR_Cfg.h */
typedef enum {
    PDUR_DEST_COM,
    PDUR_DEST_DCM
} PduR_DestType;

typedef struct {
    PduIdType       RxPduId;        /* CanIf RX PDU ID */
    PduR_DestType   Destination;    /* COM or DCM */
    PduIdType       UpperPduId;     /* Com or Dcm PDU ID */
} PduR_RoutingTableType;
```

#### Platform variants

PduR is fully platform-independent (no hardware access).

---

### 3.9 IoHwAb — I/O Hardware Abstraction (ECU Abstraction)

- **Purpose**: Abstracts sensor and actuator access for application SWCs; provides named I/O channels instead of raw peripheral calls
- **AUTOSAR reference**: AUTOSAR_SWS_IOHardwareAbstraction (SWS_IoHwAb)

#### API Functions

```c
void        IoHwAb_Init(void);

/* Sensor reads (called by RTE on behalf of SWCs) */
Std_ReturnType IoHwAb_ReadPedalAngle(uint8 SensorId, uint16* AngleRaw);
Std_ReturnType IoHwAb_ReadSteeringAngle(uint16* AngleRaw);
Std_ReturnType IoHwAb_ReadMotorCurrent(uint16* CurrentRaw);
Std_ReturnType IoHwAb_ReadTemperature(uint8 ChannelId, uint16* TempRaw);
Std_ReturnType IoHwAb_ReadBatteryVoltage(uint16* VoltageRaw);
Std_ReturnType IoHwAb_ReadEncoderCount(uint32* PulseCount);
Std_ReturnType IoHwAb_ReadDigitalInput(uint8 ChannelId, boolean* State);

/* Actuator writes (called by RTE on behalf of SWCs) */
Std_ReturnType IoHwAb_SetPwmDuty(uint8 ChannelId, uint16 DutyCycle_001Pct);
Std_ReturnType IoHwAb_SetServoAngle(uint8 ChannelId, int16 Angle_01Deg);
Std_ReturnType IoHwAb_SetDigitalOutput(uint8 ChannelId, boolean State);
```

#### Dependencies

- **Down**: Spi (`Spi_SyncTransmit`), Adc (`Adc_ReadGroup`), Pwm (`Pwm_SetDutyCycle`), Dio (`Dio_ReadChannel`, `Dio_WriteChannel`)
- **Up**: RTE (connects SWC ports to IoHwAb calls)

#### Configuration

Per-ECU IoHwAb maps named channels to physical MCAL channels:

```c
/* firmware/cvc/cfg/IoHwAb_Cfg.h */
#define IOHWAB_PEDAL_SENSOR_1   SPI_CHANNEL_PEDAL_SENSOR1
#define IOHWAB_PEDAL_SENSOR_2   SPI_CHANNEL_PEDAL_SENSOR2

/* firmware/fzc/cfg/IoHwAb_Cfg.h */
#define IOHWAB_STEERING_SERVO   PWM_CHANNEL_STEERING_SERVO
#define IOHWAB_BRAKE_SERVO      PWM_CHANNEL_BRAKE_SERVO
#define IOHWAB_ANGLE_SENSOR     SPI_CHANNEL_ANGLE_SENSOR

/* firmware/rzc/cfg/IoHwAb_Cfg.h */
#define IOHWAB_MOTOR_RPWM       PWM_CHANNEL_MOTOR_RPWM
#define IOHWAB_MOTOR_LPWM       PWM_CHANNEL_MOTOR_LPWM
#define IOHWAB_MOTOR_CURRENT    ADC_CHANNEL_MOTOR_CURRENT
#define IOHWAB_MOTOR_TEMP       ADC_CHANNEL_MOTOR_TEMP
#define IOHWAB_BATTERY          ADC_CHANNEL_BATTERY_VOLTAGE
```

#### Platform variants

IoHwAb is platform-independent. It calls MCAL APIs (Spi, Adc, Pwm, Dio) which handle platform differences. For POSIX builds, IoHwAb returns simulated values injected by the SIL test harness.

---

### 3.10 Com — Communication Module (Services)

- **Purpose**: Signal-based CAN communication with automatic byte-level packing/unpacking, I-PDU deadline monitoring, Rx timeout detection, and E2E integration
- **AUTOSAR reference**: AUTOSAR_SWS_COMModule (SWS_Com)

#### API Functions

```c
void        Com_Init(const Com_ConfigType* ConfigPtr);
void        Com_DeInit(void);
Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr);
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr);
void        Com_MainFunction_Rx(void);         /* process received PDUs */
void        Com_MainFunction_Tx(void);         /* transmit pending PDUs */
void        Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr);
void        Com_TxConfirmation(PduIdType ComTxPduId);
void        Com_IpduGroupStart(Com_IpduGroupIdType IpduGroupId, boolean Initialize);
void        Com_IpduGroupStop(Com_IpduGroupIdType IpduGroupId);
```

#### Signal Packing/Unpacking

Signals are packed into I-PDUs using little-endian byte order with configurable bit position and length:

```c
typedef struct {
    Com_SignalIdType    SignalId;
    uint8               BitPosition;    /* start bit in PDU */
    uint8               BitSize;        /* signal width in bits */
    Com_SignalType      Type;           /* UINT8, UINT16, SINT16, BOOLEAN */
    PduIdType           PduId;          /* parent I-PDU */
    float32             Factor;         /* physical = raw * factor + offset */
    float32             Offset;
    void*               ShadowBuffer;   /* pointer to signal RAM buffer */
} Com_SignalConfigType;
```

#### Rx Timeout Detection

For safety-critical Rx signals, Com monitors reception timing:

```c
typedef struct {
    PduIdType           RxPduId;
    uint16              TimeoutMs;      /* max time between receptions */
    Com_RxTimeoutAction Action;         /* REPLACE (use init value) or NOTIFY */
    boolean             TimedOut;       /* current timeout state */
    uint32              LastRxTimestamp; /* ms tick of last reception */
} Com_RxTimeoutConfigType;
```

When a Rx timeout is detected, Com calls `Dem_ReportErrorStatus` and optionally replaces the signal value with a safe default.

#### E2E Integration

Com calls E2E_Protect before transmitting safety-critical PDUs and E2E_Check after receiving them:

```
TX: SWC --> Rte_Write --> Com_SendSignal --> pack into PDU
    --> E2E_Protect(pdu) --> adds CRC + alive counter
    --> PduR_Transmit --> CanIf_Transmit --> CAN bus

RX: CAN bus --> Can_MainFunction_Read --> CanIf_RxIndication
    --> PduR_CanIfRxIndication --> Com_RxIndication
    --> E2E_Check(pdu) --> verify CRC + alive counter
    --> unpack signals --> Rte_Read available to SWC
```

#### Dependencies

- **Down**: PduR (`PduR_Transmit`), E2E (`E2E_Protect`, `E2E_Check`)
- **Up**: RTE (via `Com_SendSignal`, `Com_ReceiveSignal`), BswM (I-PDU group control)
- **Side**: Dem (`Dem_ReportErrorStatus` on Rx timeout), Gpt (time base for timeout)

#### Configuration

```c
/* firmware/cvc/cfg/Com_Cfg.h */

/* TX PDUs */
#define COM_TX_PDU_TORQUE_REQ       0u    /* CAN 0x120, 10ms cycle */
#define COM_TX_PDU_STEERING_CMD     1u    /* CAN 0x110, 10ms cycle */
#define COM_TX_PDU_BRAKE_CMD        2u    /* CAN 0x130, 10ms cycle */
#define COM_TX_PDU_CVC_HEARTBEAT    3u    /* CAN 0x010, 50ms cycle */
#define COM_TX_PDU_VEHICLE_STATE    4u    /* CAN 0x140, 100ms cycle */
#define COM_TX_PDU_ESTOP            5u    /* CAN 0x001, event-driven */

/* RX PDUs */
#define COM_RX_PDU_MOTOR_STATUS     0u    /* CAN 0x200, from RZC */
#define COM_RX_PDU_STEERING_FB      1u    /* CAN 0x210, from FZC */
#define COM_RX_PDU_LIDAR_DIST       2u    /* CAN 0x220, from FZC */
#define COM_RX_PDU_FZC_HEARTBEAT    3u    /* CAN 0x011, from FZC */
#define COM_RX_PDU_RZC_HEARTBEAT    4u    /* CAN 0x012, from RZC */

/* TX Signals */
#define COM_SIGNAL_TORQUE_REQUEST   0u    /* uint8, bit 0-7 in PDU 0x120 */
#define COM_SIGNAL_STEERING_CMD     1u    /* int16, bit 0-15 in PDU 0x110 */
#define COM_SIGNAL_BRAKE_CMD        2u    /* uint8, bit 0-7 in PDU 0x130 */
#define COM_SIGNAL_VEHICLE_STATE    3u    /* uint8, bit 0-7 in PDU 0x140 */
#define COM_SIGNAL_ESTOP_ACTIVE     4u    /* boolean, bit 0 in PDU 0x001 */

/* RX Timeouts */
#define COM_RX_TIMEOUT_HEARTBEAT_MS     100u
#define COM_RX_TIMEOUT_MOTOR_STATUS_MS  100u
#define COM_RX_TIMEOUT_STEERING_FB_MS   100u
```

#### Platform variants

Com is fully platform-independent (no hardware access).

---

### 3.11 Dcm — Diagnostic Communication Manager (Services)

- **Purpose**: UDS (ISO 14229) diagnostic service dispatch and session management
- **AUTOSAR reference**: AUTOSAR_SWS_DiagnosticCommunicationManager (SWS_Dcm)

#### API Functions

```c
void        Dcm_Init(const Dcm_ConfigType* ConfigPtr);
void        Dcm_MainFunction(void);           /* process pending requests */
void        Dcm_RxIndication(PduIdType DcmRxPduId, const PduInfoType* PduInfoPtr);
Std_ReturnType Dcm_GetSesCtrlType(Dcm_SesCtrlType* SesCtrlType);
```

#### Supported UDS Services

| SID | Service | Description | Sessions |
|-----|---------|-------------|----------|
| 0x10 | DiagnosticSessionControl | Switch between default, extended, programming sessions | All |
| 0x14 | ClearDiagnosticInformation | Clear all or group-specific DTCs from Dem storage | Extended |
| 0x19 | ReadDTCInformation | Read stored DTCs by status mask, count, snapshot | Default, Extended |
| 0x22 | ReadDataByIdentifier | Read live sensor data (DIDs: speed, current, temp, version) | Default, Extended |
| 0x2E | WriteDataByIdentifier | Write configuration DIDs (calibration values) | Extended |
| 0x27 | SecurityAccess | Seed-key authentication for protected services | Extended |
| 0x3E | TesterPresent | Keep diagnostic session alive (S3 timer reset) | All |

#### UDS Response Handling

```c
/* Negative Response Codes (NRC) */
#define DCM_NRC_GENERAL_REJECT                  0x10u
#define DCM_NRC_SERVICE_NOT_SUPPORTED           0x11u
#define DCM_NRC_SUBFUNCTION_NOT_SUPPORTED       0x12u
#define DCM_NRC_INCORRECT_MESSAGE_LENGTH        0x13u
#define DCM_NRC_CONDITIONS_NOT_CORRECT          0x22u
#define DCM_NRC_REQUEST_SEQUENCE_ERROR          0x24u
#define DCM_NRC_SECURITY_ACCESS_DENIED          0x33u
#define DCM_NRC_REQUEST_OUT_OF_RANGE            0x31u

/* Response pending (0x78) for long operations */
#define DCM_NRC_RESPONSE_PENDING                0x78u
```

#### Dependencies

- **Down**: PduR (`PduR_Transmit` for UDS responses)
- **Side**: Dem (`Dem_GetDTCStatusAvailabilityMask`, `Dem_ClearDTC`, `Dem_GetNextFilteredDTC`)
- **Up**: External UDS tester via CAN (functional addressing 0x7DF, physical 0x7E0-0x7E6)

#### Configuration

```c
/* firmware/{ecu}/cfg/Dcm_Cfg.h */
#define DCM_RX_PDU_FUNCTIONAL       0u    /* CAN 0x7DF */
#define DCM_RX_PDU_PHYSICAL         1u    /* CAN 0x7E0 (CVC), 0x7E1 (FZC), 0x7E2 (RZC) */
#define DCM_TX_PDU_RESPONSE         0u    /* CAN 0x7E8 (CVC), 0x7E9 (FZC), 0x7EA (RZC) */

#define DCM_S3_TIMEOUT_MS           5000u /* session timeout */
#define DCM_P2_TIMEOUT_MS           50u   /* response time */
#define DCM_P2_STAR_TIMEOUT_MS      5000u /* enhanced response time */

/* DID Table */
#define DCM_DID_SW_VERSION          0xF195u
#define DCM_DID_HW_VERSION          0xF191u
#define DCM_DID_MOTOR_SPEED         0xF200u
#define DCM_DID_MOTOR_CURRENT       0xF201u
#define DCM_DID_MOTOR_TEMP          0xF202u
#define DCM_DID_BATTERY_VOLTAGE     0xF203u
#define DCM_DID_VEHICLE_STATE       0xF204u
#define DCM_DID_PEDAL_POSITION      0xF205u
```

#### Platform variants

Dcm is fully platform-independent (no hardware access).

---

### 3.12 Dem — Diagnostic Event Manager (Services)

- **Purpose**: DTC storage, fault debouncing, status byte management, and freeze-frame capture
- **AUTOSAR reference**: AUTOSAR_SWS_DiagnosticEventManager (SWS_Dem)

#### API Functions

```c
void        Dem_Init(const Dem_ConfigType* ConfigPtr);
void        Dem_MainFunction(void);                        /* periodic processing */
void        Dem_ReportErrorStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus);
Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus);
Std_ReturnType Dem_GetDTCStatusAvailabilityMask(uint8* DTCStatusMask);
Std_ReturnType Dem_GetDTCOfEvent(Dem_EventIdType EventId, Dem_DTCFormatType DTCFormat, uint32* DTCValue);
Std_ReturnType Dem_ClearDTC(uint32 DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin);
Std_ReturnType Dem_SetDTCFilter(uint8 DTCStatusMask, Dem_DTCKindType DTCKind,
                                 Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin);
Std_ReturnType Dem_GetNextFilteredDTC(uint32* DTC, uint8* DTCStatus);
uint16      Dem_GetNumberOfFilteredDTC(void);
```

#### DTC Storage Structure

```c
typedef struct {
    uint32          DtcNumber;          /* 3-byte DTC (SAE J2012) */
    uint8           StatusByte;         /* ISO 14229 DTC status bits */
    Dem_EventIdType EventId;            /* originating event */
    uint32          OccurrenceCounter;  /* how many times fault occurred */
    uint32          FirstFailTimestamp;  /* ms tick of first failure */
    uint32          LastFailTimestamp;   /* ms tick of most recent failure */
    uint8           FreezeFrame[DEM_FREEZE_FRAME_SIZE]; /* snapshot data */
} Dem_DtcStorageType;

/* ISO 14229 DTC Status Bits */
#define DEM_STATUS_TEST_FAILED                  0x01u
#define DEM_STATUS_TEST_FAILED_THIS_CYCLE       0x02u
#define DEM_STATUS_PENDING_DTC                  0x04u
#define DEM_STATUS_CONFIRMED_DTC                0x08u
#define DEM_STATUS_TEST_NOT_COMPLETED_SINCE_CLR 0x10u
#define DEM_STATUS_TEST_FAILED_SINCE_LAST_CLR   0x20u
#define DEM_STATUS_TEST_NOT_COMPLETED_THIS_CYCLE 0x40u
#define DEM_STATUS_WARNING_INDICATOR_REQUESTED  0x80u
```

#### Fault Debouncing

Counter-based debouncing per event:

```c
typedef struct {
    Dem_EventIdType EventId;
    int16           Counter;                /* current debounce counter */
    int16           FailThreshold;          /* counter value to confirm fault */
    int16           PassThreshold;          /* counter value to clear fault */
    int16           IncrementStep;          /* step per FAILED report */
    int16           DecrementStep;          /* step per PASSED report */
} Dem_DebounceConfigType;
```

Typical configuration: FailThreshold = 3 (3 consecutive FAILED reports to confirm), PassThreshold = -3 (3 consecutive PASSED reports to heal).

#### Dependencies

- **Down**: None (standalone storage in static RAM array)
- **Up**: SWCs (call `Dem_ReportErrorStatus`), Dcm (reads DTCs for UDS 0x19 response), BswM (notified of DTC status changes)

#### Configuration

```c
/* firmware/{ecu}/cfg/Dem_Cfg.h */
#define DEM_MAX_DTC_COUNT           32u       /* max DTCs stored */
#define DEM_FREEZE_FRAME_SIZE       16u       /* bytes per freeze frame */
#define DEM_DEBOUNCE_FAIL_THRESHOLD 3
#define DEM_DEBOUNCE_PASS_THRESHOLD (-3)

/* Event-to-DTC mapping table */
static const Dem_EventDtcMapType Dem_EventDtcMap[] = {
    { DEM_EVENT_PEDAL_PLAUSIBILITY,    0xC00100u },
    { DEM_EVENT_PEDAL_S1_COMM,         0xC00200u },
    { DEM_EVENT_PEDAL_S2_COMM,         0xC00300u },
    { DEM_EVENT_PEDAL_BOTH_FAIL,       0xC00400u },
    { DEM_EVENT_CAN_TIMEOUT_CVC,       0xC10100u },
    { DEM_EVENT_CAN_BUSOFF,            0xC10300u },
    { DEM_EVENT_E2E_CRC,               0xC10400u },
    { DEM_EVENT_E2E_ALIVE,             0xC10500u },
    { DEM_EVENT_MOTOR_OVERCURRENT,     0xC20100u },
    { DEM_EVENT_MOTOR_OVERTEMP,        0xC20200u },
    { DEM_EVENT_MOTOR_TEMP_WARN,       0xC20300u },
    { DEM_EVENT_STEER_SENSOR,          0xC30100u },
    { DEM_EVENT_STEER_JAMMED,          0xC30200u },
    { DEM_EVENT_LIDAR_FAIL,            0xC30300u },
    { DEM_EVENT_ESTOP,                 0xC40100u },
    { DEM_EVENT_WDGM_EXPIRED,          0xC50100u },
    { DEM_EVENT_BSWM_MODE_FAIL,        0xC50200u },
};
```

#### Platform variants

Dem is fully platform-independent (no hardware access). DTC persistence uses RAM-only storage (NVM stub).

---

### 3.13 WdgM — Watchdog Manager (Services)

- **Purpose**: Supervises execution of safety-critical runnables via alive counters and deadline monitoring; gates external watchdog (TPS3823) feeding
- **AUTOSAR reference**: AUTOSAR_SWS_WatchdogManager (SWS_WdgM)

#### API Functions

```c
void        WdgM_Init(const WdgM_ConfigType* ConfigPtr);
void        WdgM_DeInit(void);
void        WdgM_MainFunction(void);                      /* periodic check */
Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEId,
                                       WdgM_CheckpointIdType CPId);
Std_ReturnType WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEId,
                                    WdgM_LocalStatusType* Status);
WdgM_GlobalStatusType WdgM_GetGlobalStatus(void);
```

#### Supervision Strategy

Each safety-critical runnable is a **supervised entity (SE)** with an expected alive counter increment per supervision cycle:

```c
typedef struct {
    WdgM_SupervisedEntityIdType SEId;
    uint16      ExpectedAliveMin;       /* min alive increments per cycle */
    uint16      ExpectedAliveMax;       /* max alive increments per cycle */
    uint16      SupervisionCycle_Ms;    /* how often WdgM checks (50ms) */
    uint16      DeadlineMax_Ms;         /* max execution time per runnable */
    uint8       FailedRefCycleTol;      /* tolerated failed cycles before fault */
} WdgM_SupervisedEntityConfigType;
```

#### Watchdog Gate Logic

```
WdgM_MainFunction() (every 50 ms):
  for each supervised entity:
    check alive counter vs expected range
    check deadline (execution time < max)
    if failed: increment failed_count[SE]
    if failed_count[SE] > tolerance: SE_status = EXPIRED

  GlobalStatus = worst of all SE statuses
  if GlobalStatus == OK:
    Dio_FlipChannel(DIO_CHANNEL_WDT_FEED)    // feed TPS3823
  else:
    // do NOT feed → TPS3823 times out → MCU reset in 1.6s
    Dem_ReportErrorStatus(DEM_EVENT_WDGM_EXPIRED, DEM_EVENT_STATUS_FAILED)
```

#### Dependencies

- **Down**: Dio (`Dio_FlipChannel` for TPS3823 WDI pin), Gpt (time base for deadline)
- **Up**: SWCs (call `WdgM_CheckpointReached` at start/end of each runnable)
- **Side**: Dem (`Dem_ReportErrorStatus` on supervision failure)

#### Configuration

```c
/* firmware/cvc/cfg/WdgM_Cfg.h */
#define WDGM_SE_PEDAL               0u    /* Swc_Pedal: 10ms, alive expected 1 per 50ms cycle = 5 */
#define WDGM_SE_STATE_MACHINE       1u    /* Swc_StateMachine: 10ms, alive expected 5 per 50ms */
#define WDGM_SE_CAN_MASTER          2u    /* Swc_CanMaster: 50ms, alive expected 1 per 50ms */
#define WDGM_SE_COM_TX              3u    /* Com_MainFunction_Tx: 10ms, alive expected 5 per 50ms */

#define WDGM_SUPERVISION_CYCLE_MS   50u
#define WDGM_FAILED_CYCLE_TOL       2u    /* 2 failed cycles tolerated (150ms total) */

#define WDGM_DEADLINE_PEDAL_MS      2u    /* Swc_Pedal must complete within 2ms */
#define WDGM_DEADLINE_SM_MS         1u    /* Swc_StateMachine within 1ms */
```

#### Platform variants

WdgM is platform-independent except for the Dio call to toggle the watchdog pin. On POSIX, the Dio stub logs the toggle without hardware effect.

---

### 3.14 BswM — BSW Mode Manager (Services)

- **Purpose**: Manages ECU operating modes and triggers mode-dependent actions across BSW modules
- **AUTOSAR reference**: AUTOSAR_SWS_BSWModeManager (SWS_BswM)

#### API Functions

```c
void        BswM_Init(const BswM_ConfigType* ConfigPtr);
void        BswM_MainFunction(void);          /* periodic rule evaluation */
void        BswM_RequestMode(BswM_UserType RequestingUser, BswM_ModeType RequestedMode);
BswM_ModeType BswM_GetCurrentMode(void);
```

#### ECU Mode States

```c
typedef enum {
    BSWM_MODE_STARTUP     = 0u,    /* BSW initialization in progress */
    BSWM_MODE_RUN         = 1u,    /* Normal operation */
    BSWM_MODE_DEGRADED    = 2u,    /* Fault detected, reduced functionality */
    BSWM_MODE_SAFE_STOP   = 3u,    /* Safe state, motor/steering disabled */
    BSWM_MODE_SHUTDOWN    = 4u     /* Controlled shutdown in progress */
} BswM_ModeType;
```

#### Mode-Dependent Rules

| Rule | Condition | Action |
|------|-----------|--------|
| R1 | STARTUP → all BSW modules initialized | Transition to RUN, enable Com TX/RX I-PDU groups |
| R2 | RUN → Dem reports safety-critical DTC confirmed | Transition to DEGRADED |
| R3 | RUN → WdgM global status != OK | Transition to SAFE_STOP |
| R4 | RUN → E-stop received via Com | Transition to SAFE_STOP |
| R5 | DEGRADED → all DTCs cleared and WdgM OK | Transition to RUN |
| R6 | DEGRADED → additional DTC or timeout | Transition to SAFE_STOP |
| R7 | SAFE_STOP entered | Stop Com TX (I-PDU group), set motor/steer to safe values, feed heartbeat (allow SC to see the ECU is still alive but in safe state) |
| R8 | Any → CAN bus-off detected | Attempt recovery, if failed → SAFE_STOP |

#### Dependencies

- **Side**: Dem (DTC status notifications), WdgM (global status), Com (I-PDU group control)
- **Up**: main.c (calls `BswM_RequestMode`), Swc_StateMachine (reacts to mode changes via RTE)

#### Configuration

```c
/* firmware/{ecu}/cfg/BswM_Cfg.h */
#define BSWM_INITIAL_MODE           BSWM_MODE_STARTUP
#define BSWM_DEGRADED_TIMEOUT_MS    30000u  /* 30s in DEGRADED → SAFE_STOP */
#define BSWM_RULE_COUNT             8u
```

#### Platform variants

BswM is fully platform-independent (no hardware access).

---

### 3.15 E2E — End-to-End Protection (Services)

- **Purpose**: Protects safety-critical CAN messages against communication errors (corruption, repetition, loss, delay) using CRC, alive counter, and data ID
- **AUTOSAR reference**: AUTOSAR_SWS_E2ELibrary (SWS_E2E), Profile P01

#### API Functions

```c
void        E2E_Init(void);
Std_ReturnType E2E_Protect(const E2E_ConfigType* Config, E2E_StateType* State,
                            uint8* DataPtr, uint16 Length);
E2E_CheckStatusType E2E_Check(const E2E_ConfigType* Config, E2E_StateType* State,
                               const uint8* DataPtr, uint16 Length);
uint8       E2E_CalcCRC8(const uint8* DataPtr, uint16 Length, uint8 StartValue);
```

#### Protection Mechanism

Each protected I-PDU contains:

| Field | Position | Size | Description |
|-------|----------|------|-------------|
| CRC-8 | Byte 0 | 8 bits | CRC-8/SAE-J1850 (polynomial 0x1D) over bytes 1..N + Data ID |
| Alive Counter | Byte 1, bits 0-3 | 4 bits | Increments 0..15 per transmission, wraps |
| Data ID | Not transmitted | 8 bits | Per-message constant, included in CRC calculation |
| Payload | Bytes 1 (bits 4-7) through N | Variable | Application data |

#### CRC-8 Calculation

```c
/* CRC-8/SAE-J1850: polynomial 0x1D, init 0xFF, XOR-out 0xFF */
static const uint8 E2E_Crc8Table[256] = {
    0x00, 0x1D, 0x3A, 0x27, 0x74, 0x69, 0x4E, 0x53,
    /* ... 256-entry lookup table ... */
};

uint8 E2E_CalcCRC8(const uint8* DataPtr, uint16 Length, uint8 StartValue) {
    uint8 crc = StartValue;
    for (uint16 i = 0u; i < Length; i++) {
        crc = E2E_Crc8Table[crc ^ DataPtr[i]];
    }
    return crc ^ 0xFFu;
}
```

#### E2E Check Status

```c
typedef enum {
    E2E_STATUS_OK           = 0u,    /* CRC valid, alive counter valid */
    E2E_STATUS_REPEATED     = 1u,    /* Same alive counter as previous */
    E2E_STATUS_WRONG_SEQ    = 2u,    /* Alive counter skipped (gap > 1) */
    E2E_STATUS_ERROR        = 3u,    /* CRC mismatch */
    E2E_STATUS_NO_NEW_DATA  = 4u     /* No new message received */
} E2E_CheckStatusType;
```

On `E2E_STATUS_ERROR` or `E2E_STATUS_WRONG_SEQ` (after configured tolerance), Com discards the message and reports to Dem.

#### Dependencies

- **Down**: None (pure computation, no hardware access)
- **Up**: Com (calls `E2E_Protect` before TX, `E2E_Check` after RX)

#### Configuration

```c
/* firmware/{ecu}/cfg/E2E_Cfg.h */
typedef struct {
    PduIdType   PduId;          /* associated Com PDU */
    uint8       DataId;         /* unique per-message constant */
    uint8       MaxDeltaCounter;/* max acceptable alive counter gap */
    uint8       MaxNoNewData;   /* max cycles with no new reception */
    uint16      DataLength;     /* PDU length in bytes */
} E2E_ConfigType;

static const E2E_ConfigType E2E_Config[] = {
    /* PduId,                  DataId, MaxDelta, MaxNoNew, DataLen */
    { COM_TX_PDU_TORQUE_REQ,     0x01,   2u,       5u,      8u },
    { COM_TX_PDU_STEERING_CMD,   0x02,   2u,       5u,      8u },
    { COM_TX_PDU_BRAKE_CMD,      0x03,   2u,       5u,      8u },
    { COM_TX_PDU_CVC_HEARTBEAT,  0x04,   2u,       3u,      8u },
    { COM_TX_PDU_ESTOP,          0x05,   2u,       1u,      8u },
    { COM_RX_PDU_MOTOR_STATUS,   0x10,   2u,       5u,      8u },
    { COM_RX_PDU_STEERING_FB,    0x11,   2u,       5u,      8u },
    { COM_RX_PDU_FZC_HEARTBEAT,  0x12,   2u,       3u,      8u },
    { COM_RX_PDU_RZC_HEARTBEAT,  0x13,   2u,       3u,      8u },
};
```

#### Platform variants

E2E is fully platform-independent (pure C computation).

---

### 3.16 Rte — Runtime Environment (RTE)

- **Purpose**: Port-based communication between SWCs and BSW services; provides typed Rte_Read/Rte_Write functions and runnable scheduling
- **AUTOSAR reference**: AUTOSAR_SWS_RTE (SWS_Rte)

#### API Functions (generated per-ECU)

```c
void        Rte_Init(void);
void        Rte_Start(void);                   /* enable port connections */

/* Sender-Receiver ports (generated per signal) */
Std_ReturnType Rte_Read_<Port>_<Signal>(DataType* data);
Std_ReturnType Rte_Write_<Port>_<Signal>(DataType data);

/* Client-Server ports (generated per operation) */
Std_ReturnType Rte_Call_<Port>_<Operation>(/* params */);

/* Runnable scheduling */
void        Rte_MainFunction(void);            /* dispatch runnables per tick */
```

#### Port-Based Communication Model

```
+----------+                                          +----------+
| Swc_Pedal|                                          | Swc_Motor|
|          |                                          |          |
| PpPedal  |---[Rte_Write]---> Signal Buffer          |          |
| Position |                   (static RAM)           |          |
|          |                       |                   |          |
+----------+                       |                   +----------+
                                   |
                          Com_SendSignal()
                                   |
                                   v
                            +------+------+
                            | Com module  |
                            | pack to PDU |
                            +------+------+
                                   |
                            PduR_Transmit()
                                   |
                                   v
                              CAN bus TX
                                   |
                                   v
                              CAN bus RX
                                   |
                            Com_RxIndication()
                                   |
                                   v
                            +------+------+
                            | Com module  |
                            | unpack PDU  |
                            +------+------+
                                   |
                          Com_ReceiveSignal()
                                   |
                                   v
                            Signal Buffer ----[Rte_Read]---> Swc_Motor
                            (static RAM)                     RpTorqueReq
```

#### Signal Buffer Implementation

All signals are stored in statically allocated RAM buffers, double-buffered for consistency (write to shadow, then swap):

```c
/* Generated per ECU */
typedef struct {
    /* CVC signals */
    uint16  PedalAngle1_Raw;
    uint16  PedalAngle2_Raw;
    uint8   PedalPosition_Pct;
    uint8   PedalFault_Status;
    uint8   VehicleState;
    uint8   TorqueRequest_Pct;
    int16   SteeringCommand_Deg;
    uint8   BrakeCommand_Pct;
    boolean EStopActive;
    uint8   CvcAliveCounter;
    uint8   FzcCommStatus;
    uint8   RzcCommStatus;
    /* ... RX signals from other ECUs ... */
    uint16  MotorSpeed_Rpm;
    uint16  MotorCurrent_Filtered;
    int16   MotorTemp_DegC;
    int16   SteeringAngle_Actual;
    uint16  LidarDistance_Cm;
} Rte_SignalBufferType;

static Rte_SignalBufferType Rte_SignalBuffer;  /* single instance, static */
```

#### Runnable Entity Mapping

Per-ECU runnable schedule (configured in `Rte_Cfg_{ecu}.c`):

```c
typedef struct {
    void        (*Runnable)(void);      /* function pointer */
    uint16      PeriodMs;               /* execution period */
    uint8       Priority;               /* task priority assignment */
    const char* Name;                   /* for debugging/tracing */
} Rte_RunnableConfigType;

/* CVC runnable table */
static const Rte_RunnableConfigType Rte_Cvc_Runnables[] = {
    { Swc_Pedal_10ms,           10u,  4u, "Swc_Pedal"         },
    { Swc_StateMachine_10ms,    10u,  4u, "Swc_StateMachine"  },
    { Swc_CanMaster_50ms,       50u,  3u, "Swc_CanMaster"     },
    { Swc_Display_100ms,        100u, 2u, "Swc_Display"       },
    /* Swc_EStop is ISR-driven, not in periodic table */
};

/* FZC runnable table */
static const Rte_RunnableConfigType Rte_Fzc_Runnables[] = {
    { Swc_Steering_10ms,        10u,  4u, "Swc_Steering"      },
    { Swc_Brake_10ms,           10u,  4u, "Swc_Brake"         },
    { Swc_Lidar_10ms,           10u,  4u, "Swc_Lidar"         },
    { Swc_Buzzer_100ms,         100u, 2u, "Swc_Buzzer"        },
};

/* RZC runnable table */
static const Rte_RunnableConfigType Rte_Rzc_Runnables[] = {
    { Swc_Motor_10ms,           10u,  4u, "Swc_Motor"         },
    { Swc_CurrentMonitor_10ms,  10u,  4u, "Swc_CurrentMonitor"},
    { Swc_Encoder_10ms,         10u,  4u, "Swc_Encoder"       },
    { Swc_TempMonitor_100ms,    100u, 2u, "Swc_TempMonitor"   },
};
```

#### Dependencies

- **Down**: Com (`Com_SendSignal`, `Com_ReceiveSignal`), IoHwAb (sensor/actuator access), Dem (`Dem_ReportErrorStatus`)
- **Up**: All SWCs (provide `Rte_Read_xxx`, `Rte_Write_xxx`, `Rte_Call_xxx`)

#### Configuration

Per-ECU RTE configuration files:

| ECU | Configuration File | Content |
|-----|-------------------|---------|
| CVC | `firmware/cvc/cfg/Rte_Cfg_Cvc.c` | CVC signal buffer, runnable table, port mappings |
| FZC | `firmware/fzc/cfg/Rte_Cfg_Fzc.c` | FZC signal buffer, runnable table, port mappings |
| RZC | `firmware/rzc/cfg/Rte_Cfg_Rzc.c` | RZC signal buffer, runnable table, port mappings |

#### Platform variants

RTE is fully platform-independent (no hardware access). The same RTE code compiles for STM32 and POSIX targets.

<!-- HITL-LOCK START:COMMENT-BLOCK-BSWARCH-SEC3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 3 provides comprehensive module specifications for all 16 BSW modules (6 MCAL, 3 EcuAL, 6 Services, 1 RTE). Each module includes purpose, AUTOSAR reference, API functions, dependencies (up/down), and configuration. The API function signatures follow AUTOSAR naming conventions correctly. The dependency direction is strictly enforced: MCAL modules only depend downward on HAL/hardware, Services only call through EcuAL/MCAL. The RTE section correctly shows per-ECU configuration tables with runnable scheduling parameters (period, priority). The Rte module specification shows compile-time port binding which avoids runtime overhead -- appropriate for resource-constrained ASIL D targets. One gap: the Gpt module specification is not shown in Section 3, though Gpt appears in the dependency graph as providing the time base for all modules. Gpt should have its own specification block in Section 3.
<!-- HITL-LOCK END:COMMENT-BLOCK-BSWARCH-SEC3 -->

## 4. MCAL Layer Detail

### 4.1 Can Module — Platform Variants

#### STM32 (FDCAN in Classic Mode)

```c
/* Can_Stm32.c — wraps STM32 HAL FDCAN */

#include "stm32g4xx_hal.h"

static FDCAN_HandleTypeDef hfdcan1;

void Can_Init(const Can_ConfigType* ConfigPtr) {
    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.NominalPrescaler = ConfigPtr->Prescaler;
    hfdcan1.Init.NominalTimeSeg1 = ConfigPtr->TimeSeg1;
    hfdcan1.Init.NominalTimeSeg2 = ConfigPtr->TimeSeg2;
    hfdcan1.Init.NominalSyncJumpWidth = ConfigPtr->SJW;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;    /* Classic CAN only */
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    HAL_FDCAN_Init(&hfdcan1);
    /* Configure RX filters per ConfigPtr->RxFilterTable */
    HAL_FDCAN_Start(&hfdcan1);
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo) {
    FDCAN_TxHeaderTypeDef txHeader;
    txHeader.Identifier = PduInfo->id;
    txHeader.IdType = FDCAN_STANDARD_ID;
    txHeader.TxFrameType = FDCAN_DATA_FRAME;
    txHeader.DataLength = FDCAN_DLC_BYTES_8;
    return (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, PduInfo->sdu) == HAL_OK)
           ? E_OK : E_NOT_OK;
}
```

#### TMS570 (DCAN Register-Level)

```c
/* Can_Tms570.c — direct DCAN1 register access (SC only) */

#include "can.h"  /* HALCoGen-generated */

void Can_Init(const Can_ConfigType* ConfigPtr) {
    canInit();  /* HALCoGen init */
    /* Set TEST.Silent bit for listen-only mode */
    canREG1->TEST |= (1u << 3u);  /* Silent mode (no TX) */
    /* Configure 3 RX message objects for CVC/FZC/RZC heartbeats */
    canUpdateID(canREG1, canMESSAGE_BOX1, 0x010u);  /* CVC heartbeat */
    canUpdateID(canREG1, canMESSAGE_BOX2, 0x011u);  /* FZC heartbeat */
    canUpdateID(canREG1, canMESSAGE_BOX3, 0x012u);  /* RZC heartbeat */
}
```

#### POSIX (SocketCAN)

```c
/* Can_Posix.c — Linux SocketCAN for Docker/SIL */

#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

static int can_socket = -1;

void Can_Init(const Can_ConfigType* ConfigPtr) {
    can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    struct ifreq ifr;
    strncpy(ifr.ifr_name, ConfigPtr->InterfaceName, IFNAMSIZ);  /* "vcan0" or "can0" */
    ioctl(can_socket, SIOCGIFINDEX, &ifr);
    struct sockaddr_can addr = { .can_family = AF_CAN, .can_ifindex = ifr.ifr_ifindex };
    bind(can_socket, (struct sockaddr*)&addr, sizeof(addr));
    /* Set non-blocking for polling mode */
    fcntl(can_socket, F_SETFL, O_NONBLOCK);
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo) {
    struct can_frame frame;
    frame.can_id = PduInfo->id;
    frame.can_dlc = PduInfo->length;
    memcpy(frame.data, PduInfo->sdu, PduInfo->length);
    ssize_t nbytes = write(can_socket, &frame, sizeof(frame));
    return (nbytes == sizeof(frame)) ? E_OK : E_NOT_OK;
}
```

### 4.2 Spi Module — AS5048A Communication

The AS5048A magnetic angle sensor communicates via SPI mode 1 (CPOL=0, CPHA=1) at up to 10 MHz:

```
CVC: SPI1 bus, 2 AS5048A sensors on separate CS pins (PA4, PA5)
FZC: SPI2 bus, 1 AS5048A sensor on CS pin (PB12)

Read sequence:
  CS low → TX 0xFFFF (NOP) → RX 16 bits → CS high
  Received data: [1 bit parity | 1 bit error flag | 14 bits angle]
  Angle range: 0..16383 = 0..360 degrees
  Resolution: 0.022 degrees/LSB
```

### 4.3 Adc Module — ACS723 and NTC Readings

RZC ADC1 runs in continuous DMA scan mode, converting 3 channels per scan:

```
Channel 0 (PA0): ACS723 motor current sensor
  - Output: 2.5V at 0A, 0.4V/A sensitivity
  - ADC formula: I(mA) = ((raw * 3300 / 4096) - 2500) * 1000 / 400
  - Range: -6.25A to +6.25A (for 0..3.3V ADC input)

Channel 1 (PA1): NTC motor temperature
  - 10K NTC with 10K pull-up to 3.3V
  - Steinhart-Hart lookup table (16 entries, -40C to +150C)
  - ADC formula: R_ntc = 10000 * raw / (4095 - raw), then table lookup

Channel 2 (PA2): Battery voltage
  - Voltage divider: R1=10K, R2=3.3K (ratio 4.03:1)
  - ADC formula: V_bat(mV) = raw * 3300 * 403 / (4096 * 100)
  - Range: 0..13.3V for 0..3.3V ADC input
```

### 4.4 Pwm Module — BTS7960 and Servo Control

```
RZC: BTS7960 H-bridge motor driver
  - TIM3_CH1 (PA6): RPWM (forward)
  - TIM3_CH2 (PA7): LPWM (reverse)
  - Frequency: 20 kHz (above audible range)
  - Direction: RPWM active + LPWM=0 = forward, LPWM active + RPWM=0 = reverse
  - Braking: both RPWM=0 and LPWM=0

FZC: Standard hobby servos
  - TIM2_CH1 (PA0): Steering servo
  - TIM2_CH2 (PA1): Brake servo
  - Frequency: 50 Hz
  - Pulse width: 1000us (min) to 2000us (max), 1500us = center
  - Angle range: -45 to +45 degrees (steering), 0 to 100% (brake)
```

<!-- HITL-LOCK START:COMMENT-BLOCK-BSWARCH-SEC4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The MCAL Layer Detail section provides concrete platform-specific implementation code for Can (STM32 FDCAN, TMS570 DCAN, POSIX SocketCAN), Spi (AS5048A communication protocol), Adc (ACS723 current sensor and NTC temperature with conversion formulas), and Pwm (BTS7960 motor driver and servo control). The three Can platform variants share the same API (Can_Init, Can_Write) with different implementations, which validates the portability claim. The TMS570 Can_Init correctly sets TEST.Silent bit for listen-only mode, consistent with the SC requirements. The ADC conversion formulas for ACS723 and NTC include units and ranges which aids verification. One concern: the Pwm section specifies 20 kHz for motor PWM (above audible) and 50 Hz for servo PWM, but does not mention the dead-time or anti-shoot-through protection for the BTS7960 H-bridge. At ASIL D, simultaneous high-side and low-side switching must be explicitly prevented either in hardware or software.
<!-- HITL-LOCK END:COMMENT-BLOCK-BSWARCH-SEC4 -->

## 5. ECU Abstraction Layer Detail

### 5.1 CanIf — CAN Interface Routing

CanIf maps CAN hardware objects (CanId) to upper-layer PDU IDs. The routing is static, configured at compile time per ECU.

```
CAN Bus                              Upper Layer
========                              ===========

RX:
  CAN ID 0x120 (TorqueReq)    --> CanIf --> PduR --> Com (signal unpack)
  CAN ID 0x7DF (UDS func req) --> CanIf --> PduR --> Dcm (UDS processing)
  CAN ID 0x011 (FZC heartbeat)--> CanIf --> PduR --> Com (signal unpack)

TX:
  Com_SendSignal(TorqueReq)   --> PduR --> CanIf --> CAN ID 0x120
  Dcm response                --> PduR --> CanIf --> CAN ID 0x7E8
```

CanIf manages CAN controller bus-off recovery: on bus-off detection, CanIf notifies BswM and attempts automatic recovery after `CAN_BUSOFF_RECOVERY_MS`.

### 5.2 PduR — PDU Routing Table

PduR routes PDUs between CanIf (lower) and Com/Dcm (upper) using a static routing table:

```c
/* CVC PduR routing table */
static const PduR_RoutingTableType PduR_RoutingTable[] = {
    /* RX routes: CanIf PDU → Upper layer */
    { CANIF_RX_PDU_0x200,  PDUR_DEST_COM,  COM_RX_PDU_MOTOR_STATUS   },
    { CANIF_RX_PDU_0x210,  PDUR_DEST_COM,  COM_RX_PDU_STEERING_FB    },
    { CANIF_RX_PDU_0x220,  PDUR_DEST_COM,  COM_RX_PDU_LIDAR_DIST     },
    { CANIF_RX_PDU_0x011,  PDUR_DEST_COM,  COM_RX_PDU_FZC_HEARTBEAT  },
    { CANIF_RX_PDU_0x012,  PDUR_DEST_COM,  COM_RX_PDU_RZC_HEARTBEAT  },
    { CANIF_RX_PDU_0x7DF,  PDUR_DEST_DCM,  DCM_RX_PDU_FUNCTIONAL     },
    { CANIF_RX_PDU_0x7E0,  PDUR_DEST_DCM,  DCM_RX_PDU_PHYSICAL       },

    /* TX routes: Upper layer PDU → CanIf */
    { COM_TX_PDU_TORQUE_REQ,     PDUR_DEST_CANIF, CANIF_TX_PDU_0x120  },
    { COM_TX_PDU_STEERING_CMD,   PDUR_DEST_CANIF, CANIF_TX_PDU_0x110  },
    { COM_TX_PDU_BRAKE_CMD,      PDUR_DEST_CANIF, CANIF_TX_PDU_0x130  },
    { COM_TX_PDU_CVC_HEARTBEAT,  PDUR_DEST_CANIF, CANIF_TX_PDU_0x010  },
    { COM_TX_PDU_VEHICLE_STATE,  PDUR_DEST_CANIF, CANIF_TX_PDU_0x140  },
    { COM_TX_PDU_ESTOP,          PDUR_DEST_CANIF, CANIF_TX_PDU_0x001  },
    { DCM_TX_PDU_RESPONSE,       PDUR_DEST_CANIF, CANIF_TX_PDU_0x7E8  },
};
```

### 5.3 IoHwAb — Sensor/Actuator Abstraction

IoHwAb is the bridge between application SWCs (via RTE) and hardware (via MCAL). It converts between application-meaningful values and raw hardware values:

```c
/* Example: IoHwAb_ReadPedalAngle wraps SPI transaction */
Std_ReturnType IoHwAb_ReadPedalAngle(uint8 SensorId, uint16* AngleRaw) {
    uint8 txBuf[2] = { 0xFF, 0xFF };  /* NOP command */
    uint8 rxBuf[2] = { 0x00, 0x00 };
    Spi_ChannelType ch;

    if (SensorId == 0u) {
        ch = SPI_CHANNEL_PEDAL_SENSOR1;
    } else if (SensorId == 1u) {
        ch = SPI_CHANNEL_PEDAL_SENSOR2;
    } else {
        return E_NOT_OK;
    }

    Spi_WriteIB(ch, txBuf);
    Std_ReturnType ret = Spi_SyncTransmit(ch);
    if (ret != E_OK) { return E_NOT_OK; }

    Spi_ReadIB(ch, rxBuf);

    /* Extract 14-bit angle: bits 13..0, ignore parity (bit 15) and error (bit 14) */
    uint16 raw = ((uint16)rxBuf[0] << 8u) | rxBuf[1];
    boolean errorFlag = (boolean)((raw >> 14u) & 0x01u);
    if (errorFlag) { return E_NOT_OK; }

    *AngleRaw = raw & 0x3FFFu;  /* 14-bit angle: 0..16383 */
    return E_OK;
}
```

```c
/* Example: IoHwAb_SetPwmDuty wraps PWM timer */
Std_ReturnType IoHwAb_SetPwmDuty(uint8 ChannelId, uint16 DutyCycle_001Pct) {
    if (DutyCycle_001Pct > 10000u) {
        return E_NOT_OK;  /* range check: 0..10000 = 0..100.00% */
    }
    Pwm_SetDutyCycle((Pwm_ChannelType)ChannelId, DutyCycle_001Pct);
    return E_OK;
}
```

<!-- HITL-LOCK START:COMMENT-BLOCK-BSWARCH-SEC5 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The ECU Abstraction Layer detail covers CanIf (routing CAN IDs to PDU IDs), PduR (static PDU routing table between CanIf and Com/Dcm), and IoHwAb (sensor/actuator abstraction bridging SWCs to MCAL). The CanIf routing example correctly shows bidirectional mapping (RX: CAN ID to PDU, TX: PDU to CAN ID) with bus-off recovery notification to BswM. The PduR routing table code snippet for CVC shows all expected message mappings consistent with the CAN message matrix. The IoHwAb code examples demonstrate proper input validation (SensorId range check, DutyCycle range check) and error propagation (return E_NOT_OK on invalid input), which aligns with ASIL D defensive programming requirements. The IoHwAb_ReadPedalAngle function correctly extracts the 14-bit angle from the AS5048A SPI response and checks the error flag bit -- this is a good example of sensor data validation at the abstraction boundary.
<!-- HITL-LOCK END:COMMENT-BLOCK-BSWARCH-SEC5 -->

## 6. Services Layer Detail

### 6.1 Com — Signal-Based CAN Communication

Com provides signal-level access for SWCs. Each signal is mapped to a specific bit position within an I-PDU (CAN message).

**Signal Packing Example** (TorqueRequest in PDU 0x120):

```
PDU 0x120 (8 bytes):
  Byte 0: CRC-8 (E2E)
  Byte 1: [3:0] Alive counter, [7:4] Vehicle state
  Byte 2: TorqueRequest_Pct (uint8, 0..100)
  Byte 3: SteeringCommand_Deg high byte (int16)
  Byte 4: SteeringCommand_Deg low byte
  Byte 5: BrakeCommand_Pct (uint8, 0..100)
  Byte 6: PedalFault_Status (uint8, enum 0..4)
  Byte 7: Reserved (0x00)
```

**Rx Timeout Flow**:

```
Com_MainFunction_Rx() called every 10 ms:
  for each Rx PDU with timeout configured:
    elapsed = Gpt_GetTickCount_Ms() - lastRxTimestamp[pdu]
    if elapsed > timeoutMs[pdu]:
      if !timedOut[pdu]:
        timedOut[pdu] = TRUE
        Dem_ReportErrorStatus(timeoutEventId[pdu], DEM_EVENT_STATUS_FAILED)
        if timeoutAction[pdu] == COM_REPLACE:
          replace signal values with configured defaults (safe values)
    else:
      if timedOut[pdu]:
        timedOut[pdu] = FALSE
        Dem_ReportErrorStatus(timeoutEventId[pdu], DEM_EVENT_STATUS_PASSED)
```

### 6.2 Dcm — UDS Diagnostic Handler

Dcm dispatches incoming UDS requests to service-specific handlers:

```c
static const Dcm_ServiceTableType Dcm_ServiceTable[] = {
    { 0x10u, Dcm_ProcessDiagnosticSessionControl },
    { 0x14u, Dcm_ProcessClearDiagnosticInformation },
    { 0x19u, Dcm_ProcessReadDTCInformation },
    { 0x22u, Dcm_ProcessReadDataByIdentifier },
    { 0x2Eu, Dcm_ProcessWriteDataByIdentifier },
    { 0x27u, Dcm_ProcessSecurityAccess },
    { 0x3Eu, Dcm_ProcessTesterPresent },
};
```

**Session Management**:
- Default session: basic services (0x10, 0x19, 0x22, 0x3E)
- Extended session: full services (all SIDs)
- Programming session: (reserved for future OTA)
- S3 server timer: 5 seconds inactivity reverts to default session

### 6.3 Dem — DTC Event Management

DTC lifecycle:

```
Event reported (FAILED)
    |
    v
Debounce counter incremented (+1)
    |
    v (counter >= FailThreshold)
    |
DTC status: TEST_FAILED = 1, PENDING_DTC = 1
    |
    v (next driving cycle with fault still present)
    |
DTC status: CONFIRMED_DTC = 1
    |
    v (fault cleared via UDS 0x14 or healed)
    |
Debounce counter decremented (-1)
    |
    v (counter <= PassThreshold)
    |
DTC status: TEST_FAILED = 0
```

Freeze frame capture: on first DTC confirmation, Dem snapshots current sensor values (current, temp, speed, pedal position) into the DTC's freeze frame buffer for later diagnostic retrieval.

### 6.4 WdgM — Watchdog Supervision

Alive supervision flow:

```
SWC runnable executes:
  WdgM_CheckpointReached(SE_PEDAL, CP_START)
  ... Swc_Pedal_10ms() ...
  WdgM_CheckpointReached(SE_PEDAL, CP_END)

WdgM_MainFunction() (every 50 ms):
  alive_count[SE_PEDAL] should be 5 (one per 10ms tick)
  if alive_count < ExpectedMin or alive_count > ExpectedMax:
    SE_status[SE_PEDAL] = EXPIRED
  reset alive_count

  if ALL SE statuses == OK:
    Dio_FlipChannel(WDT_PIN)   // feed TPS3823
  else:
    stop feeding → TPS3823 timeout → MCU reset
```

### 6.5 BswM — Mode Management

BswM state machine:

```
STARTUP ──[all Init done]──> RUN
                               |
               [safety DTC]────+────[E-stop]
                   |                    |
                   v                    v
              DEGRADED             SAFE_STOP
                   |                    ^
          [additional fault]────────────+
                   |
          [fault cleared]──> RUN
```

Mode-dependent actions:

| Mode | Com TX | Motor/Steer | Heartbeat | WdgM |
|------|--------|------------|-----------|------|
| STARTUP | Disabled | Disabled | Disabled | Active |
| RUN | Enabled | Enabled | Enabled | Active |
| DEGRADED | Enabled (reduced) | Limited (limp) | Enabled | Active |
| SAFE_STOP | Disabled (except HB) | Disabled (safe values) | Enabled | Active |
| SHUTDOWN | Disabled | Disabled | Disabled | Disabled |

### 6.6 E2E — CRC-8 + Alive Counter + Data ID

Per-message protection:

```
TX side (E2E_Protect):
  1. Increment alive counter (0..15 wrap)
  2. Place alive counter in byte 1, bits 0..3
  3. Calculate CRC-8 over bytes 1..N + DataID byte
  4. Place CRC in byte 0

RX side (E2E_Check):
  1. Extract received CRC from byte 0
  2. Calculate expected CRC over bytes 1..N + DataID byte
  3. Compare: if mismatch → E2E_STATUS_ERROR
  4. Extract alive counter from byte 1, bits 0..3
  5. Compare with last received: if same → E2E_STATUS_REPEATED
  6. If gap > MaxDeltaCounter → E2E_STATUS_WRONG_SEQ
  7. If all OK → E2E_STATUS_OK, update last received counter
```

<!-- HITL-LOCK START:COMMENT-BLOCK-BSWARCH-SEC6 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The Services Layer Detail covers all six service modules (Com, Dcm, Dem, WdgM, BswM, E2E) with implementation-level detail. The Com signal packing example for PDU 0x120 shows exact byte layout with E2E CRC and alive counter placement. The Dcm UDS service table covers the essential diagnostic services (0x10, 0x14, 0x19, 0x22, 0x2E, 0x27, 0x3E) with session management. The Dem DTC lifecycle correctly shows the debounce-to-confirmation-to-clear flow with freeze frame capture. The WdgM alive supervision flow correctly demonstrates the checkpoint-based monitoring with TPS3823 external watchdog. The BswM state machine (STARTUP/RUN/DEGRADED/SAFE_STOP/SHUTDOWN) with mode-dependent actions is consistent with the sw-architecture.md state machine design. The E2E protection detail covers CRC-8, alive counter, and Data ID with all seven fault detection types. One concern: the BswM DEGRADED state allows "reduced" Com TX but does not specify which messages are suppressed versus retained -- this should be defined to ensure safety-critical heartbeat and E-stop messages are never suppressed.
<!-- HITL-LOCK END:COMMENT-BLOCK-BSWARCH-SEC6 -->

## 7. RTE Detail

### 7.1 Port-Based Communication

The RTE enforces a strict separation between SWCs and BSW. SWCs never call BSW APIs directly — all communication goes through typed Rte_Read/Rte_Write functions.

**Sender-Receiver Communication** (data flow):

```c
/* CVC: Swc_Pedal writes pedal position */
Std_ReturnType Rte_Write_PpPedalPosition_PedalPosition_Pct(uint8 data) {
    Rte_SignalBuffer.PedalPosition_Pct = data;
    return Com_SendSignal(COM_SIGNAL_TORQUE_REQUEST, &data);
}

/* RZC: Swc_Motor reads torque request (received via CAN from CVC) */
Std_ReturnType Rte_Read_RpTorqueRequest_TorqueRequest_Pct(uint8* data) {
    return Com_ReceiveSignal(COM_SIGNAL_TORQUE_REQUEST, data);
}
```

**Client-Server Communication** (function call):

```c
/* SWC calls Dem to report error */
Std_ReturnType Rte_Call_RpDem_ReportError(Dem_EventIdType EventId, Dem_EventStatusType Status) {
    return Dem_ReportErrorStatus(EventId, Status);
}
```

### 7.2 Runnable Entity Scheduling

The RTE dispatches runnables from a tick counter in `Rte_MainFunction`, called from the 1 ms SysTick ISR:

```c
static uint32 Rte_TickCounter = 0u;

void Rte_MainFunction(void) {
    Rte_TickCounter++;

    /* 10 ms tasks */
    if ((Rte_TickCounter % 10u) == 0u) {
        for (i = 0; i < Rte_RunnableCount; i++) {
            if (Rte_Runnables[i].PeriodMs == 10u) {
                WdgM_CheckpointReached(i, CP_START);
                Rte_Runnables[i].Runnable();
                WdgM_CheckpointReached(i, CP_END);
            }
        }
    }

    /* 50 ms tasks */
    if ((Rte_TickCounter % 50u) == 0u) {
        for (i = 0; i < Rte_RunnableCount; i++) {
            if (Rte_Runnables[i].PeriodMs == 50u) {
                WdgM_CheckpointReached(i, CP_START);
                Rte_Runnables[i].Runnable();
                WdgM_CheckpointReached(i, CP_END);
            }
        }
        WdgM_MainFunction();
    }

    /* 100 ms tasks */
    if ((Rte_TickCounter % 100u) == 0u) {
        for (i = 0; i < Rte_RunnableCount; i++) {
            if (Rte_Runnables[i].PeriodMs == 100u) {
                Rte_Runnables[i].Runnable();
            }
        }
        Dem_MainFunction();
        BswM_MainFunction();
    }
}
```

<!-- HITL-LOCK START:COMMENT-BLOCK-BSWARCH-SEC7 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The RTE Detail section shows the port-based communication model (Sender-Receiver for data flow, Client-Server for function calls) with concrete code examples. The Rte_MainFunction scheduling code demonstrates tick-driven runnable dispatch at 10 ms, 50 ms, and 100 ms periods with WdgM checkpoint instrumentation around each safety-critical runnable. The scheduling approach is deterministic and avoids dynamic task creation. One concern: the Rte_MainFunction runs all runnables of the same period sequentially in a single tick context. If a runnable overruns its budget, subsequent runnables in the same period group are delayed. For ASIL D, consider documenting the execution order guarantee and the consequence of a single runnable overrun on the group's overall WCET budget. Additionally, 100 ms tasks call Dem_MainFunction and BswM_MainFunction in the same tick -- if NVM write operations in Dem_MainFunction are slow, this could delay BswM mode transitions.
<!-- HITL-LOCK END:COMMENT-BLOCK-BSWARCH-SEC7 -->

## 8. Configuration Strategy

### 8.1 Per-ECU Configuration Files

Each ECU has a `cfg/` directory containing compile-time configuration headers:

```
firmware/
  cvc/cfg/
    Can_Cfg.h         — CAN filters, TX handles for CVC messages
    Com_Cfg.h         — Signal/PDU definitions for CVC TX/RX
    Dcm_Cfg.h         — UDS addressing (0x7E0 physical, 0x7E8 response)
    Dem_Cfg.h         — Event-to-DTC mapping for CVC faults
    E2E_Cfg.h         — E2E config per CVC message
    WdgM_Cfg.h        — Supervised entities for CVC runnables
    BswM_Cfg.h        — Mode rules for CVC
    Rte_Cfg_Cvc.c     — Signal buffer, runnable table, port mappings
    IoHwAb_Cfg.h      — Channel mappings (pedal SPI, OLED I2C)
    Dio_Cfg.h         — GPIO pin assignments (E-stop, LEDs, WDT)
    Spi_Cfg.h         — SPI channels (AS5048A x2)

  fzc/cfg/
    Can_Cfg.h         — CAN filters for FZC messages
    Com_Cfg.h         — Signal/PDU definitions for FZC
    Pwm_Cfg.h         — Servo PWM channels and frequencies
    Spi_Cfg.h         — SPI channel (steering angle sensor)
    Rte_Cfg_Fzc.c     — FZC runnable table, port mappings
    ...

  rzc/cfg/
    Can_Cfg.h         — CAN filters for RZC messages
    Com_Cfg.h         — Signal/PDU definitions for RZC
    Adc_Cfg.h         — ADC channels (current, temp, battery)
    Pwm_Cfg.h         — Motor PWM channels and frequency
    Rte_Cfg_Rzc.c     — RZC runnable table, port mappings
    ...
```

### 8.2 Compile-Time Defines

```c
/* firmware/{ecu}/cfg/Ecu_Cfg.h — master ECU identity header */

/* ECU identity — exactly one defined per build */
#define ECU_TYPE_CVC    1u
#define ECU_TYPE_FZC    2u
#define ECU_TYPE_RZC    3u
#define ECU_TYPE        ECU_TYPE_CVC  /* changes per ECU build */

/* CAN node identity */
#define CAN_NODE_ID             0x01u  /* CVC=0x01, FZC=0x02, RZC=0x03 */

/* Heartbeat message ID */
#define HEARTBEAT_CAN_ID        0x010u /* CVC=0x010, FZC=0x011, RZC=0x012 */

/* UDS physical addressing */
#define DCM_PHYSICAL_CAN_ID     0x7E0u /* CVC=0x7E0, FZC=0x7E1, RZC=0x7E2 */
#define DCM_RESPONSE_CAN_ID     0x7E8u /* CVC=0x7E8, FZC=0x7E9, RZC=0x7EA */

/* Platform selection */
#define PLATFORM_STM32  1u
#define PLATFORM_TMS570 2u
#define PLATFORM_POSIX  3u
#define PLATFORM        PLATFORM_STM32
```

### 8.3 Configuration Tables

Key configuration tables used at compile time:

| Table | Module | Content | Per-ECU |
|-------|--------|---------|---------|
| `Can_FilterConfig[]` | Can | RX filter CAN IDs and masks | Yes |
| `CanIf_RxPduConfig[]` | CanIf | CAN ID to PDU ID mapping | Yes |
| `CanIf_TxPduConfig[]` | CanIf | PDU ID to CAN ID + HTH mapping | Yes |
| `PduR_RoutingTable[]` | PduR | PDU routing (CanIf to Com/Dcm) | Yes |
| `Com_SignalConfig[]` | Com | Signal bit position, size, type, PDU | Yes |
| `Com_RxTimeoutConfig[]` | Com | Rx timeout per PDU | Yes |
| `E2E_Config[]` | E2E | Data ID, counter limits per PDU | Yes |
| `Dem_EventDtcMap[]` | Dem | Event ID to DTC number mapping | Yes |
| `WdgM_SupervisedEntityConfig[]` | WdgM | Alive bounds per supervised entity | Yes |
| `BswM_RuleConfig[]` | BswM | Mode transition rules | Yes |
| `Dcm_ServiceTable[]` | Dcm | UDS service dispatch table | Common |
| `Dcm_DidTable[]` | Dcm | DID to read function mapping | Yes |
| `Rte_Runnables[]` | Rte | Runnable function pointers and periods | Yes |

<!-- HITL-LOCK START:COMMENT-BLOCK-BSWARCH-SEC8 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The Configuration Strategy section documents the per-ECU configuration files (Section 8.1), compile-time defines (Section 8.2), and configuration tables (Section 8.3). The approach of using per-ECU cfg/ directories with compile-time headers is clean and avoids runtime configuration overhead. The Ecu_Cfg.h master header with ECU_TYPE, CAN_NODE_ID, HEARTBEAT_CAN_ID, and DCM addressing provides clear per-build identity. The configuration tables summary (Section 8.3) lists 14 key tables with their module, content, and per-ECU status. All configuration is static and compile-time, which is correct for ASIL D (no dynamic configuration changes at runtime). One observation: the configuration table list does not include NVM configuration (persistence of DTC data across power cycles) -- if Dem DTCs are only stored in RAM, they will be lost on reset, which may be acceptable for SIL but not for production.
<!-- HITL-LOCK END:COMMENT-BLOCK-BSWARCH-SEC8 -->

## 9. Platform Abstraction

### 9.1 Three Platform Targets

| Target | Macro | Use Case | MCAL Implementation |
|--------|-------|----------|-------------------|
| STM32 | `PLATFORM_STM32` | Production firmware on CVC, FZC, RZC | STM32 HAL wrappers |
| TMS570 | `PLATFORM_TMS570` | Safety Controller (SC) — CAN only | HALCoGen register-level |
| POSIX | `PLATFORM_POSIX` | Simulated ECUs (BCM, ICU, TCU), SIL testing | SocketCAN, stubs |

### 9.2 Build System Selection

```makefile
# Makefile platform selection
PLATFORM ?= stm32

ifeq ($(PLATFORM),stm32)
    CC = arm-none-eabi-gcc
    CFLAGS += -DPLATFORM=PLATFORM_STM32
    MCAL_SRC = Can_Stm32.c Spi_Stm32.c Adc_Stm32.c Pwm_Stm32.c Dio_Stm32.c Gpt_Stm32.c
endif

ifeq ($(PLATFORM),tms570)
    CC = armcl  # TI ARM compiler
    CFLAGS += -DPLATFORM=PLATFORM_TMS570
    MCAL_SRC = Can_Tms570.c Dio_Tms570.c Gpt_Tms570.c
endif

ifeq ($(PLATFORM),posix)
    CC = gcc
    CFLAGS += -DPLATFORM=PLATFORM_POSIX
    MCAL_SRC = Can_Posix.c Spi_Stub.c Adc_Stub.c Pwm_Stub.c Dio_Stub.c Gpt_Posix.c
endif
```

### 9.3 Layer Portability

| Layer | STM32 | TMS570 | POSIX | Notes |
|-------|-------|--------|-------|-------|
| MCAL | Platform-specific | Platform-specific | Stubs/SocketCAN | Only layer that changes |
| ECU Abstraction | Common | Common | Common | Platform-independent |
| Services | Common | Common | Common | Platform-independent |
| RTE | Common | N/A (no RTE on SC) | Common | Platform-independent |
| Application SWCs | Common | N/A (bare-metal) | Common | Platform-independent |

The architecture guarantees that all code above MCAL is pure C99 with no platform-specific headers or system calls. This enables:
- Single source for physical and simulated ECUs
- SIL testing on developer PC without hardware
- CI/CD pipeline using POSIX builds on Linux

<!-- HITL-LOCK START:COMMENT-BLOCK-BSWARCH-SEC9 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The Platform Abstraction section cleanly demonstrates the three-target build strategy (STM32, TMS570, POSIX) with Makefile platform selection. The Layer Portability table confirms that only the MCAL layer changes between platforms -- ECU Abstraction, Services, RTE, and Application SWCs are all common. The "pure C99 with no platform-specific headers" guarantee above MCAL is a strong architectural constraint that enables SIL testing and CI/CD pipelines on Linux. The build system uses standard compiler selection (arm-none-eabi-gcc for STM32, armcl for TMS570, gcc for POSIX) with platform-specific MCAL source file lists. This is well-designed for maintainability and consistent with vendor-independence principles. No significant concerns.
<!-- HITL-LOCK END:COMMENT-BLOCK-BSWARCH-SEC9 -->

## 10. Traceability and Revision History

### 10.1 BSW Module to SSR Traceability

| BSW Module | SSRs Served | ECUs |
|------------|------------|------|
| Can | SSR-CVC-013/014/015, SSR-FZC-021, SSR-RZC-016, SSR-SC-001 | All |
| Spi | SSR-CVC-001, SSR-FZC-003 | CVC, FZC |
| Adc | SSR-RZC-007/011 | RZC |
| Pwm | SSR-FZC-002/010, SSR-RZC-002 | FZC, RZC |
| Dio | SSR-CVC-011/022, SSR-FZC-020/024, SSR-RZC-017, SSR-SC-009..014 | All |
| Gpt | All scheduling-related SSRs | All |
| CanIf | All CAN communication SSRs | CVC, FZC, RZC |
| PduR | All CAN communication SSRs | CVC, FZC, RZC |
| IoHwAb | SSR-CVC-001, SSR-FZC-002/003/010, SSR-RZC-002/007/011/014 | CVC, FZC, RZC |
| Com | SSR-CVC-003/009/010/012..016/019/020, SSR-FZC-001/007/009/014/019/021/022, SSR-RZC-001/015/016 | CVC, FZC, RZC |
| Dcm | UDS-related (TCU primary, all ECUs support basic diagnostics) | All |
| Dem | SSR-CVC-004, SSR-FZC-008, SSR-RZC-010/013, all fault reporting SSRs | CVC, FZC, RZC |
| WdgM | SSR-CVC-021/022, SSR-FZC-023/024, SSR-RZC-017, SSR-SC-014 | All |
| BswM | SSR-CVC-023 | CVC, FZC, RZC |
| E2E | SSR-CVC-019/020, SSR-FZC-022, SSR-RZC-015/016 | CVC, FZC, RZC |
| Rte | All SWC communication SSRs | CVC, FZC, RZC |

<!-- HITL-LOCK START:COMMENT-BLOCK-BSWARCH-SEC10 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The BSW Module to SSR Traceability table maps all 16 BSW modules to the SSRs they serve across all ECUs. This provides the reverse traceability direction required by ASPICE SWE.2 BP5. The Can module correctly maps to SSRs across all four ECUs (including SC), while IoHwAb maps only to CVC/FZC/RZC (SC has no IoHwAb). The Com module has the largest SSR footprint, which is expected since it handles all signal-based CAN communication. The traceability is consistent with the SSR allocation tables in sw-architecture.md Section 11. One observation: the Gpt module is not listed in the traceability table but provides the time base for all scheduling-related SSRs -- it should be included for completeness.
<!-- HITL-LOCK END:COMMENT-BLOCK-BSWARCH-SEC10 -->

### 10.2 Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | — | Initial stub |
| 1.0 | 2026-02-21 | Taktflow Team | Complete SWE.2 BSW architecture: full dependency graph, 16 module specifications with APIs and configuration, MCAL platform variants (STM32/TMS570/POSIX), ECU abstraction detail, services layer detail, RTE port-based communication model, configuration strategy, platform abstraction strategy, traceability to 81 SSRs |

