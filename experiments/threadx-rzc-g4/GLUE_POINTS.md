# ThreadX + BSW Integration: 78 Touch Points

## CRITICAL (will break if not handled)

| # | Point | Fix |
|---|---|---|
| 2 | SchM `__disable_irq` freezes ThreadX | Replace with `TX_DISABLE`/`TX_RESTORE` |
| 4 | ThreadX tick rate default 100Hz, BSW expects 1000Hz | `TX_TIMER_TICKS_PER_SECOND=1000` |
| 7 | SysTick reload must match tick rate | `SYSTICK_CYCLES=(170MHz/1000)-1` |
| 6 | `stm32g4xx_it.c` PendSV/SysTick conflicts with ThreadX | Exclude for ThreadX builds |
| 43 | NVIC priority grouping must be GROUP_4 | `HAL_Init()` sets this — verify |
| 59 | `HAL_InitTick` must use TIM6, not SysTick | Link TIM6 timebase OR keep `HAL_IncTick` in SysTick handler |
| 75 | `TIM6_DAC_IRQHandler` missing if TIM6 timebase linked | Provide handler or don't use TIM6 |
| 76 | Double `HAL_IncTick` if both SysTick AND TIM6 | Pick ONE — not both |
| 28 | E2E alive counter rate-dependent | Fixed by correct tick rate (#4) |
| 29 | Com RX timeout counter rate-dependent | Fixed by correct tick rate (#4) |
| 30 | Com TX period rate-dependent | Fixed by correct tick rate (#4) |
| 38 | Rte `tick % periodMs` assumes 1ms/call | Fixed by 1ms timer callback |
| 46 | CanTp `CANTP_MAIN_CYCLE_MS=10` hardcoded | Timer must call CanTp every 10ms |
| 52-54 | `CANTP/DCM/COM_MAIN_CYCLE_MS` constants | Timer rates must match these |

## HIGH (may cause problems)

| # | Point | Fix |
|---|---|---|
| 17 | WdgM deadline miss → safe state | Match call rate |
| 18 | TPS3823 external watchdog wrong feed rate → reset | Match WdgM rate |
| 24 | Self-test `HAL_Delay` before ThreadX starts | Keep SysTick running during init |
| 41 | `Can_Init` → `HAL_FDCAN_Init` uses `HAL_Delay` | HAL tick must work before `tx_kernel_enter` |
| 48 | `Error_Handler` calls `__disable_irq` → freezes ThreadX | Guard with `#ifdef` |
| 61 | NvM flash write blocks 10ms in timer callback | Accept jitter or move to thread |
| 63 | Motor cutoff sends 10 frames across 10 cycles | OK — counter-based, not blocking |
| 73 | Timer thread stack 1024 — BSW chain ~800 bytes | May need 2048 |

## MEDIUM (should verify)

| # | Point | Fix |
|---|---|---|
| 27 | MPU regions vs ThreadX thread stacks | Verify regions cover ThreadX RAM |
| 31 | Dcm S3 timer rate-dependent | Fixed by correct Dcm call rate |
| 42 | HAL_FDCAN_MspInit NVIC priority | Must not conflict with PendSV |
| 45 | Com_MainFunction_Tx/Rx must both run at 10ms | Same timer callback |
| 47 | CanTp STmin decremented per call | Fixed by correct CanTp call rate |
| 60 | Multiple `Error_Handler` definitions | Linker picks correct one |
| 70 | TIM6 NVIC priority vs ThreadX | Check priority value |
| 71 | `SYSTEM_CLOCK` in `.S` must match PLL config | 170MHz — matches |

## LOW / SAFE (no change needed)

| # | Point | Why safe |
|---|---|---|
| 1 | Timer callbacks | Proven in Step 5 |
| 3 | HAL tick | `HAL_IncTick` already in SysTick handler |
| 5 | `Main_Hw_Wfi` | Not called in ThreadX path |
| 8-9 | Timer callback rates | Match BSW constants |
| 10-16 | Various peripheral interactions | Stubs or polling |
| 19-26 | GPIO, PWM, SPI, ADC, startup | All before ThreadX |
| 32-40 | BswM, Det, IoHwAb, SensorFeeder, etc. | Single context |
| 44 | 22 shared RTE signals | Single writer/reader |
| 49 | PWM/Dio stubs | Not implemented yet |
| 50 | FPU context save | ThreadX handles it |
| 51 | Static config tables | Read-only in flash |
| 55-58 | C startup, FPU, alignment | Standard ARM |
| 62 | CAN state | Single context |
| 64-67 | Stack VLA, motor cutoff, malloc, recursion | Verified safe |
| 72 | FDCAN during CPSID | FIFO holds, polled later |
| 74 | Voltage scaling | Before ThreadX |
| 77-78 | CanTp/Dem buffers | Static, fits in RAM |

## Decision: HAL Tick Source

**Choose ONE:**
- **Option A (ST recommended):** TIM6 for HAL tick. Remove `HAL_IncTick` from SysTick `.S`. Provide `TIM6_DAC_IRQHandler`. Link `stm32g4xx_hal_timebase_tim.c`.
- **Option B (simpler):** SysTick for HAL tick via `HAL_IncTick` in `.S`. Do NOT link `stm32g4xx_hal_timebase_tim.c`. No TIM6 needed.

Our experiment Steps 1-6 used Option B and it worked. Option A is more correct long-term.
