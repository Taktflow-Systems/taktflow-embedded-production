# Lessons Learned — POSIX/vCAN Porting (Firmware to SIL)

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-23 (Phases 11-12)
**Scope:** Porting 7 ECU firmware from bare-metal STM32/TMS570 to POSIX Linux in Docker containers with virtual CAN
**Result:** All 7 ECUs running on host, communicating over vcan0

---

## 1. HAL Abstraction Makes Porting Possible

Every hardware access goes through a HAL layer. Porting = rewriting `*_hw_posix.c` files, not touching application logic.

| HAL function | STM32 implementation | POSIX implementation |
|-------------|---------------------|---------------------|
| `Can_Write()` | bxCAN register write | `write(can_socket, &frame, sizeof(frame))` |
| `Spi_Transfer()` | SPI1 DMA | simulated response array |
| `Adc_Read()` | ADC1 channel read | plant simulator value |
| `Pwm_SetDuty()` | TIMx_CCRx register | logged to stdout |
| `Gpt_GetTime()` | SysTick | `clock_gettime(CLOCK_MONOTONIC)` |

**Lesson:** The HAL abstraction designed for testability (Phase 5) was the same abstraction that enabled SIL porting. Design for test = design for simulation.

---

## 2. SocketCAN Is the Right Linux CAN API

Linux SocketCAN provides a standard socket interface for CAN:
```c
int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
struct sockaddr_can addr = { .can_ifindex = if_nametoindex("vcan0") };
bind(s, (struct sockaddr*)&addr, sizeof(addr));
```

**Why SocketCAN:** Same API for virtual CAN (vcan) and real CAN hardware (can0). Swap by changing the interface name.

**Lesson:** SocketCAN makes CAN simulation trivial. No special simulator software needed — just `ip link add vcan0 type vcan`.

---

## 3. POSIX Backend Files Are MISRA-Exempt

`*_hw_posix.c` files use POSIX APIs (`signal.h`, `stdio.h`, `exit()`, `time.h`, `ioctl`) that violate MISRA by design. They are simulation backends, never deployed to hardware.

**Pattern:** Suppress MISRA rules 21.5, 21.6, 21.8, 21.10, 17.7 for `*_hw_posix.c` files only.

**Lesson:** POSIX backends need blanket MISRA exemption. Add the suppressions when you create the first POSIX file, not after CI fails.

---

## 4. Plant Simulator Replaces Real Physics

Each POSIX ECU gets its sensor values from a shared plant simulator instead of real hardware:
- Motor: RPM from torque/inertia model, temperature from thermal model
- Steering: angle from servo dynamics model
- Lidar: distance from obstacle scenario
- Battery: voltage from discharge model

**Lesson:** The plant simulator IS the test environment. Its accuracy determines the fidelity of the SIL demo.

---

## 5. DBC File as CAN Signal Contract

A DBC file (Vector CANdb format) defines every CAN message and signal. The plant simulator, gateway, and dashboard all parse the DBC to decode CAN frames.

**Problem:** Hand-written DBC files drift from the CAN message matrix.

**Lesson:** Auto-generate DBC from the CAN matrix, or at minimum validate DBC against matrix in CI.

---

## 6. Docker per ECU = Clean Isolation

Each ECU runs in its own Docker container with its own process. They communicate only via vcan0 (CAN bus).

**Benefits:**
- Restart one ECU without affecting others
- Each ECU has its own log stream
- Resource limits prevent one ECU from starving others
- Matches the physical isolation of real ECUs

**Lesson:** One container per ECU mirrors the real architecture. Don't put multiple ECUs in one container — you lose the isolation that makes the simulation realistic.

---

## 7. PduR Init Must Happen Before CAN TX

CAN transmission failed silently because PduR (Protocol Data Unit Router) wasn't initialized. Messages were queued but never routed.

**Fix:** Add `PduR_Init()` call in all ECU `main()` functions before `Com_Init()`.

**Lesson:** AUTOSAR BSW init order matters: `Can_Init() → CanIf_Init() → PduR_Init() → Com_Init() → Dcm_Init()`. Skipping a step = silent failure.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| HAL abstraction | Design for test = design for simulation — same abstraction |
| SocketCAN | Standard socket API for both vcan and real CAN — trivial swap |
| POSIX MISRA | Blanket MISRA exemption for `*_hw_posix.c` — add it day one |
| Plant simulator | The simulator IS the test environment — accuracy matters |
| DBC files | Auto-generate or validate against CAN matrix in CI |
| Docker per ECU | One container per ECU mirrors physical isolation |
| BSW init order | Can → CanIf → PduR → Com → Dcm — skipping = silent failure |
