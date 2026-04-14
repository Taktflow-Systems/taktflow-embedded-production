# DCM Walkthrough

## Purpose

This is the Phase 0 cheat sheet for the generic BSW DCM in `firmware/bsw/services/Dcm/`.
It is the Phase 1 insertion point for new generic handlers for `0x19`, `0x14`, and `0x31`.

Important repo reality: this checkout does not have a generated SID table, `Dcm_DispatchRequest()`,
or `Dcm_ServiceTable.c`. Service dispatch is a hand-written `switch` inside
`firmware/bsw/services/Dcm/src/Dcm.c`.

## Request path

1. `CanIf` receives the CAN frame and calls `PduR_CanIfRxIndication()`.
2. `PduR` routes by destination:
   - direct to `Dcm_RxIndication()` for single-frame traffic
   - or to `CanTp_RxIndication()` for ISO-TP payloads
3. `CanTp` reassembles SF/FF/CF traffic and calls `Dcm_TpRxIndication(..., NTFRSLT_OK)`.
4. `Dcm_TpRxIndication()` delegates to `Dcm_RxIndication()`.
5. `Dcm_RxIndication()` copies bytes into `dcm_rx_buf`, stores `dcm_rx_len`, and sets
   `dcm_request_pending = TRUE`.
6. `Dcm_MainFunction()` notices the pending flag and calls `dcm_process_request(dcm_rx_buf, dcm_rx_len)`.

## Dispatch pattern in this checkout

- `dcm_process_request(const uint8* data, PduLengthType length)` reads `sid = data[0]`.
- It resets the S3 timer before dispatch.
- It then uses a `switch (sid)` to call one static handler per service.
- Supported generic BSW SIDs today are:
  - `0x10` DiagnosticSessionControl
  - `0x11` ECUReset
  - `0x22` ReadDataByIdentifier
  - `0x27` SecurityAccess
  - `0x3E` TesterPresent
- Any unknown SID falls through to `dcm_send_nrc(sid, DCM_NRC_SERVICE_NOT_SUPPORTED)`.

## Exact handler shape to copy

New generic service handlers in this checkout follow this exact pattern:

```c
static void dcm_handle_<service>(const uint8* data, PduLengthType length)
{
    if (length < <minimum>) {
        dcm_send_nrc(<sid>, DCM_NRC_INCORRECT_MSG_LENGTH);
        return;
    }

    if (<session or security precondition fails>) {
        dcm_send_nrc(<sid>, <nrc>);
        return;
    }

    dcm_tx_buf[0] = <sid> + DCM_POSITIVE_RESPONSE_OFFSET;
    dcm_tx_buf[1] = ...;
    dcm_send_response(dcm_tx_buf, <response_length>);
}
```

What that means for Phase 1:

- Add the new SID macro to `Dcm.h`.
- Add a `static void dcm_handle_<service>(const uint8* data, PduLengthType length);`
  prototype near the other handler prototypes in `Dcm.c`.
- Implement the handler in `Dcm.c`.
- Add a `case` in `dcm_process_request()`.
- Use `dcm_send_nrc()` for all negative exits.
- Build the positive response in `dcm_tx_buf` and finish with `dcm_send_response()`.

## DID table pattern

The only table-driven part of the generic BSW DCM today is the DID inventory.

```c
typedef Std_ReturnType (*Dcm_DidReadFuncType)(uint8* Data, uint8 Length);

typedef struct {
    uint16              Did;
    Dcm_DidReadFuncType ReadFunc;
    uint8               DataLength;
} Dcm_DidTableType;
```

`dcm_handle_read_did()` does a linear scan over `dcm_config->DidTable[0..DidCount-1]`.
On a DID match it:

1. checks `3 + DataLength <= DCM_TX_BUF_SIZE`
2. writes `0x62`, DID high, DID low into `dcm_tx_buf[0..2]`
3. calls `ReadFunc(&dcm_tx_buf[3], DataLength)`
4. sends the response if the callback returns `E_OK`
5. otherwise returns NRC `0x31`

The ECU-owned cfg files `firmware/ecu/*/cfg/Dcm_Cfg_<Ecu>.c` provide:

- the DID table
- `DidCount`
- `TxPduId`
- `S3TimeoutMs`

## Response and NRC behavior

- Positive response SID is always `request_sid + 0x40`.
- Negative response is always `[0x7F, request_sid, nrc]`.
- `dcm_send_response()` uses:
  - `PduR_DcmTransmit()` when response length is `<= 7`
  - `CanTp_Transmit()` when response length is `> 7`
- NRCs defined in generic `Dcm.h` are:
  - `0x11` serviceNotSupported
  - `0x12` subFunctionNotSupported
  - `0x13` incorrectMessageLengthOrInvalidFormat
  - `0x31` requestOutOfRange
  - `0x33` securityAccessDenied
  - `0x35` invalidKey
  - `0x36` exceededNumberOfAttempts

Gap to note: generic `Dcm.h` does not define NRC `0x22` ConditionsNotCorrect, even though
some ECU-local diagnostic stacks in this repo do use it.

## Session and security state already present

Generic BSW DCM already keeps the state Phase 1 handlers will need:

- `dcm_current_session`
- `dcm_s3_timer_ms`
- `dcm_security_unlocked`
- `dcm_seed_active`
- `dcm_security_fail_count`

Current behavior:

- `0x10 default session` clears security state.
- `0x27` is the only service that explicitly requires extended session today.
- `Dcm_MainFunction()` drops back to default session on S3 timeout and also re-locks security.

## Practical implications for `0x19`, `0x14`, and `0x31`

For new generic handlers, the current dispatcher is simple enough that the real work is not
the switch statement; it is the service-specific backing model.

- `0x19 ReadDTCInformation` will need a DEM-side read/filter API that does not exist yet.
- `0x14 ClearDiagnosticInformation` will need selective clear behavior or a conscious decision
  to map the service to today's all-or-nothing `Dem_ClearAllDTCs()`.
- `0x31 RoutineControl` will need a routine registry or a local static switch; there is no
  routine table type in generic BSW DCM today.

## Repo divergences that matter

- The repo also contains ECU-local diagnostic stacks with broader service sets:
  - `firmware/ecu/cvc/src/Swc_CvcDcm.c`
  - `firmware/ecu/fzc/src/Swc_FzcDcm.c`
  - `firmware/ecu/rzc/src/Swc_RzcDcm.c`
  - `firmware/ecu/tcu/src/Swc_UdsServer.c`
- TCU `Dcm_Cfg_Tcu.c` already documents a future service/session/security table in comments,
  but the active `Dcm_ConfigType` still only contains DID table, response PDU, and S3 timeout.
- For Phase 1 generic BSW work, the source of truth is still `firmware/bsw/services/Dcm/`.
  Do not assume the ECU-local service lists represent the generic dispatcher.
