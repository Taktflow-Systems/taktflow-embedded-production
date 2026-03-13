# Plan: Fix Heartbeat PDU Layout + Add E2E Protection to FZC/RZC

> **SUPERSEDED** by [`plan-e2e-pdu-layout-fix.md`](plan-e2e-pdu-layout-fix.md) which covers all 12 affected messages, not just heartbeats.

## Context

All 3 ECU heartbeats (CVC 0x010, FZC 0x011, RZC 0x012) have a firmware-vs-DBC PDU layout mismatch. The firmware writes `ECU_ID` to byte 1, but E2E protection uses bytes 0-1 for its overhead (counter+dataid, CRC). On CVC, E2E_Protect destroys the ECU_ID. On FZC/RZC, E2E is not applied at all (safety gap). The CAN monitor shows garbage for `ECU_ID` because it decodes byte 2 per DBC but the firmware puts `vehicle_state` there.

## Root Cause

Firmware PDU layout:    `[Alive | ECU_ID | State | Fault_Lo | Fault_Hi | 0 | 0 | 0]`
DBC expected layout:    `[E2E_DataID+Counter | E2E_CRC | ECU_ID | OpMode+FaultStatus]`

E2E_Protect writes byte 0 = `(counter<<4)|dataid`, byte 1 = CRC. Application data must start at byte 2.

## Fix: New Heartbeat PDU Layout (all 3 ECUs)

```
Byte 0: [E2E_AliveCounter:4 | E2E_DataId:4]   ← E2E_Protect writes
Byte 1: E2E_CRC8                                ← E2E_Protect writes
Byte 2: ECU_ID (8 bits)                         ← Application (matches DBC)
Byte 3: [FaultStatus:4 | OperatingMode:4]       ← Application (matches DBC)
Byte 4-7: reserved (zero)
```

## E2E DataId Alignment

CVC RX table (Swc_CvcCom.c:50-51) expects FZC DataId=0x02, RZC DataId=0x03.
Current defines don't match:
- `FZC_E2E_HEARTBEAT_DATA_ID = 0x11` (nibble = 0x01) → change to `0x02`
- `RZC_E2E_HEARTBEAT_DATA_ID = 0x04` → change to `0x03`
- `CVC_E2E_HEARTBEAT_DATA_ID = 0x02` → keep (already correct)

## Files to Modify

### Phase 1: CVC Heartbeat TX — fix PDU byte positions

**`firmware/cvc/src/Swc_Heartbeat.c`** (lines 155-169)

Current (buggy):
```c
pdu[0] = alive_counter;        // E2E overwrites
pdu[1] = CVC_ECU_ID_CVC;       // E2E overwrites — DESTROYED!
pdu[2] = (uint8)vehicle_state;  // DBC expects ECU_ID here
```

Fix:
```c
/* Bytes 0-1 reserved for E2E (counter+dataid, CRC) */
pdu[2] = CVC_ECU_ID_CVC;                          /* ECU_ID — byte 2 per DBC */
pdu[3] = (uint8)(vehicle_state & 0x0Fu);           /* OperatingMode — byte 3 low nibble */
/* FaultStatus (byte 3 high nibble) not used in CVC heartbeat — stays 0 */
```

Remove the firmware's `alive_counter` variable and related increment/wrap logic — E2E_Protect manages its own counter internally. Keep the `Rte_Write(CVC_SIG_HEARTBEAT_ALIVE, ...)` diagnostic using the E2E state counter instead.

Wait — CVC's alive_counter is also used as the pre-fill for pdu[0] and for the Rte_Write diagnostic. Since E2E_Protect has its own internal counter (`hb_e2e_state.Counter`), we can read that for diagnostics. But the alive_counter is also compared vs CVC_HB_ALIVE_MAX (15). This is separate from E2E. For simplicity: keep the alive_counter for Rte_Write diagnostic but stop writing it to the PDU.

### Phase 2: FZC Heartbeat TX — add E2E, fix layout

**`firmware/fzc/src/Swc_Heartbeat.c`**

Changes:
1. Add `#include "E2E.h"`
2. Add E2E config + state (static const, like CVC pattern):
   ```c
   static const E2E_ConfigType hb_e2e_config = {
       FZC_E2E_HEARTBEAT_DATA_ID,  /* DataId = 0x02 */
       15u,                         /* MaxDeltaCounter */
       8u                           /* DataLength */
   };
   static E2E_StateType hb_e2e_state;
   ```
3. Init: add `hb_e2e_state.Counter = 0u;`
4. Fix PDU construction:
   ```c
   for (i = 0u; i < 8u; i++) { tx_data[i] = 0u; }
   /* Bytes 0-1: reserved for E2E */
   tx_data[2] = FZC_ECU_ID;                              /* ECU_ID */
   tx_data[3] = (uint8)(((fault_mask & 0x0Fu) << 4u)     /* FaultStatus high nibble */
                       | (vehicle_state & 0x0Fu));         /* OperatingMode low nibble */

   (void)E2E_Protect(&hb_e2e_config, &hb_e2e_state, tx_data, 8u);
   ```
5. Keep PduR_Transmit for the actual send (no need to switch to Com_SendSignal)
6. Keep Hb_AliveCounter for Rte_Write diagnostic (don't write to PDU)

**`firmware/fzc/include/Fzc_Cfg.h`** (line 114)
- Change `FZC_E2E_HEARTBEAT_DATA_ID` from `0x11u` to `0x02u`

### Phase 3: RZC Heartbeat TX — add E2E, fix layout

**`firmware/rzc/src/Swc_Heartbeat.c`** — same pattern as FZC

**`firmware/rzc/include/Rzc_Cfg.h`** (line 107)
- Change `RZC_E2E_HEARTBEAT_DATA_ID` from `0x04u` to `0x03u`

### Phase 4: CVC Heartbeat RX — fix Com signal bit positions

**`firmware/cvc/cfg/Com_Cfg_Cvc.c`** (lines 62-66)

Current (wrong — matches old firmware layout):
```c
/* RX signals — heartbeat PDUs built by PduR_Transmit: byte 0=alive, 1=ecu_id */
{  9u,    0u,     8u, COM_UINT8, CVC_COM_RX_FZC_HB, &sig_rx_fzc_hb_alive  },
{ 10u,    8u,     8u, COM_UINT8, CVC_COM_RX_FZC_HB, &sig_rx_fzc_hb_ecu_id },
{ 11u,    0u,     8u, COM_UINT8, CVC_COM_RX_RZC_HB, &sig_rx_rzc_hb_alive  },
{ 12u,    8u,     8u, COM_UINT8, CVC_COM_RX_RZC_HB, &sig_rx_rzc_hb_ecu_id },
```

Fix — extract E2E alive counter from byte 0 upper nibble (bitPos=4, 4 bits), ECU_ID from byte 2 (bitPos=16):
```c
/* RX signals — E2E-protected heartbeats: byte 0=[counter|dataid], 1=CRC, 2=ECU_ID */
{  9u,    4u,     4u, COM_UINT8, CVC_COM_RX_FZC_HB, &sig_rx_fzc_hb_alive  },
{ 10u,   16u,     8u, COM_UINT8, CVC_COM_RX_FZC_HB, &sig_rx_fzc_hb_ecu_id },
{ 11u,    4u,     4u, COM_UINT8, CVC_COM_RX_RZC_HB, &sig_rx_rzc_hb_alive  },
{ 12u,   16u,     8u, COM_UINT8, CVC_COM_RX_RZC_HB, &sig_rx_rzc_hb_ecu_id },
```

### Phase 5: Update Unit Tests (TDD — write expectations FIRST)

**`firmware/cvc/test/test_Swc_Heartbeat_asilc.c`**
- Line 383: change `mock_com_send_data[1]` → `mock_com_send_data[2]` for ECU_ID check
- Line 384: change `mock_com_send_data[2]` → `mock_com_send_data[3]` for state check (now lower nibble of byte 3)
- Lines 347, 351: remove alive counter byte[0] assertions (E2E handles byte 0 now)

**`firmware/fzc/test/test_Swc_Heartbeat_asilc.c`**
- Update byte position defines:
  ```c
  #define HB_BYTE_ECU_ID      2u   /* was 1u */
  #define HB_BYTE_STATE_FAULT 3u   /* was: HB_BYTE_STATE=2u, HB_BYTE_FAULT_LO=3u, HB_BYTE_FAULT_HI=4u */
  ```
- Update all assertions that check ecu_id, state, fault bytes
- Add E2E mock expectations (E2E_Protect called, config DataId = 0x02)

**`firmware/rzc/test/test_Swc_Heartbeat_asilc.c`**
- Same pattern as FZC test updates
- Config DataId = 0x03

### Phase 6: Build + Verify

1. `make test` — all 3 ECUs pass (CVC, FZC, RZC)
2. Deploy to SIL — verify heartbeats decode correctly in CAN monitor:
   - ECU_ID shows constant value (CVC=1, FZC=2, RZC=3)
   - OperatingMode shows vehicle state
   - E2E_AliveCounter cycles 0-15
   - E2E_CRC8 changes each frame
3. Flash to physical STM32s — verify on CAN bus with CAN monitor

## Key Design Decisions

1. **Use shared BSW E2E module** (E2E.h/E2E.c) for all heartbeats — consistent with CVC and DBC layout. FZC/RZC have their own E2E variant in Swc_FzcCom/Swc_RzcCom for other messages — those stay unchanged.

2. **FaultStatus = lower 4 bits of fault_mask** for FZC/RZC. CVC heartbeat has no FaultStatus (stays 0). Detailed fault info travels via dedicated messages (0x210, 0x211, etc.).

3. **Keep PduR_Transmit for FZC/RZC** — don't migrate to Com_SendSignal. Just add E2E_Protect before the PduR call.

4. **E2E DataIds use ECU-adjacent values**: CVC=0x02, FZC=0x02, RZC=0x03. Per-CAN-ID scoping means CVC TX DataId 0x02 and FZC TX DataId 0x02 don't conflict (different CAN IDs).

## Risk

- CVC currently monitors FZC/RZC heartbeats by polling alive counter changes from Com shadow buffers. After fix: alive counter extraction changes from bitPos=0/8-bit to bitPos=4/4-bit. The comparison logic (`fzc_alive != fzc_last_alive`) still works with 4-bit values (0-15). No logic change needed in Swc_Heartbeat RX path.

- FZC/RZC currently send heartbeats WITHOUT E2E. CVC's `Swc_CvcCom_E2eCheck` on FZC/RZC heartbeats currently fails silently (DataId mismatch, CRC invalid). After fix, E2E check will start passing — this is the desired behavior, not a regression.
