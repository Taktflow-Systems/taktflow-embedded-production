# Plan: ThreadX + CAN Integration (Step-by-Step)

## Base: ST's Tx_Thread_Creation example (PROVEN WORKING on our G474RE — LED blinks)

## Approach: Add ONE thing at a time. Verify after each step. Never skip verification.

---

### Step 1: Build the base with our Makefile
- Create `Makefile` for this project (standalone, no reference to production firmware)
- `make` → `fzc_threadx.elf`
- Flash → LED blinks
- **PASS criteria**: LED toggles every 500ms

### Step 2: Add UART printf
- Enable LPUART1 (already in ST example's `main.c`)
- Add `printf("ThreadX running, tick=%lu\n", tx_time_get())` in thread
- Flash → check COM3 at 115200
- **PASS criteria**: UART prints tick count every second

### Step 3: Add FDCAN init (HAL)
- Add `HAL_FDCAN_Init()` in `main.c` after `SystemClock_Config()`
- Configure: PA11/PA12 AF9, 500kbps, classic CAN
- Add `HAL_FDCAN_Start()` + `HAL_FDCAN_ActivateNotification()`
- Flash → no crash
- **PASS criteria**: No HardFault, LED still blinks, UART still prints

### Step 4: Send one CAN frame from ThreadX thread
- In MainThread: `HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, txData)`
- Send ID=0x558, data=[0xDE, 0xAD, 0xBE, 0xEF]
- Flash → check `candump can0` on Pi
- **PASS criteria**: `558#DEADBEEF` appears on CAN bus

### Step 5: Send periodic CAN frames from ThreadX timer
- Create `tx_timer_create(&can_timer, "CAN", can_tx_callback, 0, 100, 100, TX_AUTO_ACTIVATE)`
- Callback sends CAN frame with incrementing counter
- Flash → verify periodic frames on Pi
- **PASS criteria**: `candump` shows frames every 1 second (100 ticks × 10ms)

### Step 6: Receive CAN frame via ISR + ThreadX event flag
- Enable FDCAN RX FIFO0 interrupt
- In `FDCAN1_IT0_IRQHandler`: read frame, `tx_event_flags_set(&can_event, 0x1, TX_OR)`
- In a CAN RX thread: `tx_event_flags_get(&can_event, 0x1, TX_AND_CLEAR, &actual, TX_WAIT_FOREVER)`
- Send `cansend can0 123#AABBCCDD` from Pi → verify UART prints received frame
- **PASS criteria**: CAN RX works from ISR through ThreadX to thread

### Step 7: Add BSW modules (one by one)
- 7a: Add `Can.c` (our MCAL wrapper) — verify CAN still works
- 7b: Add `CanIf.c` + routing table — verify TX PDU routing
- 7c: Add `Com.c` + signal packing — verify cyclic frames (0x200, 0x201)
- 7d: Add `Rte.c` + `Rte_MainFunction()` in timer callback
- 7e: Add SWCs (Swc_Heartbeat first) — verify heartbeat frame on CAN
- Each sub-step: flash, verify CAN frames on Pi, no HardFault

### Step 8: Add UDS/Diagnostics
- Add `CanTp.c`, `Dcm.c`, `PduR.c`
- Configure DoCAN on 0x7E0/0x7E8
- Send TesterPresent from Pi → verify response
- **PASS criteria**: `cansend can0 7E0#023E00...` → `7E8#027E00...`

### Step 9: Full FZC application
- Add all SWCs (Steering, Brake, Lidar, Safety, Buzzer)
- Add self-test
- Add WdgM, Dem, BswM
- Run full HIL test suite
- **PASS criteria**: Same behavior as bare-metal FZC, but on ThreadX

### Step 10: Port to RZC and HSM
- Copy the working pattern to F413ZH (bxCAN, different HAL)
- Copy to L552ZE (TrustZone, Cortex-M33 ThreadX port)

---

## Rules
1. **One change per step** — never add two things at once
2. **Verify immediately** — flash and test after every change
3. **If it breaks, revert** — don't debug forward, go back to last working state
4. **No production firmware references** — this is standalone until Step 9
5. **All CAN verification via Pi** — `candump can0` on Raspberry Pi
6. **UART debug always on** — printf the state at every step
