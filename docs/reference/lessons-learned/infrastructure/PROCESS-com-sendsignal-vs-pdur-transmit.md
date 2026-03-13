# Com_SendSignal vs PduR_Transmit for E2E-Protected PDUs

## 2026-03-05

**Context**: CVC heartbeat TX built an 8-byte PDU with E2E_Protect (bytes 0-1 = counter+dataid, CRC), then sent it via `Com_SendSignal(CVC_COM_TX_HEARTBEAT, pdu)`.

**Mistake**: `Com_SendSignal` takes a **signal ID** and copies a **single typed value** (uint8, uint16, etc.) into the Com TX PDU buffer at the configured `bitPos`. Passing the full 8-byte PDU buffer as `SignalDataPtr` only copies 1 byte (since the signal type is `COM_UINT8`) to `bitPos=16` (byte 2). Result: E2E counter+dataid ended up in byte 2 instead of byte 0, bytes 0-1 stayed zero, CRC was never written.

**Fix**: Use `PduR_Transmit(CVC_COM_TX_HEARTBEAT, &pdu_info)` which sends the raw 8-byte PDU buffer directly through CanIf, preserving the E2E byte positions.

**Principle**: When an SWC constructs a full PDU with E2E protection, always use `PduR_Transmit` (raw PDU path), never `Com_SendSignal` (signal-level path). `Com_SendSignal` is for individual signal values that Com packs into its own PDU buffer. These are fundamentally different TX paths.
