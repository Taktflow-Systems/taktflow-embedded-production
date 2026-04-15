# Phase 4 Line B — Multi-frame ISO-TP bench pass evidence

**Date:** 2026-04-15
**Bench:** Linux laptop `an-dao@192.168.0.158` (Ubuntu 6.17.0-20, vcan0)
**Branch:** `auto/line-b/proxy-can-isotp-fc-fix-2026-04-15`
**Gate:** D4 live verification of the proxy-can FlowControl fix

## Context

Phase 4 Line B D2 (PR #7, `33b0bfbf`) changed the CVC F190 handler to
return the 17-byte VIN from `cvc_identity.toml` instead of the 4-byte
stub. That forced the CAN-to-DoIP proxy into its first-ever live
multi-frame ISO-TP exchange (FirstFrame -> FlowControl ->
ConsecutiveFrames), and the post-merge smoke test failed with
`recv_isotp failed err=Timeout { id: 2024 }` — the proxy was not
sending a FlowControl back on 0x7E0 after receiving the FirstFrame
from the CVC POSIX container on 0x7E8.

## Root causes (two)

1. The `Reassembler` state machine had no way to signal to the socket
   layer that a FC frame must be transmitted after a FirstFrame. The
   legacy `push` method returned `Option<Vec<u8>>`, which could only
   express "not done yet" or "complete" — never "need to send a FC
   now". Fix: new `ReassemblerEvent` enum + `push_event` API + new
   `CanInterface::recv_isotp_with_flow_control` that writes the FC
   frame on the request CAN id while still holding the socket guard.

2. Even after the FC path was wired, the first D4 live run showed the
   FC being emitted 726 ms AFTER the FirstFrame. The recv poll loop
   was sleeping 2 ms between every frame read, matched or filtered.
   With the POSIX compose fleet emitting ~500 Hz of unrelated periodic
   signals on vcan0, the loop was throttled to 500 reads/s and 363
   filtered frames ate the full 726 ms. Fix: only sleep on idle
   (WouldBlock); yield cooperatively when there was real work.

## Pytest output (D4 gate)

```
$ cd ~/taktflow-embedded-production
$ docker compose -f deploy/docker/compose-posix-ecus.yml up -d
$ python3 -m pytest tests/interop/compose_ecus_via_proxy.py -v -s
============================= test session starts ==============================
platform linux -- Python 3.12.3, pytest-8.4.1, pluggy-1.6.0
rootdir: /home/an-dao/taktflow-embedded-production
plugins: timeout-2.4.0, cov-7.1.0, asyncio-1.3.0, mock-3.15.1
collected 1 item

tests/interop/compose_ecus_via_proxy.py::test_compose_cvc_returns_vin_via_proxy PASSED

============================== 1 passed in 22.63s ==============================
```

## Candump hex trace (full FF + FC + CF exchange)

Captured on the Linux laptop with
`candump -tz -x vcan0,7E0:7F0` during the successful pytest run:

```
 (000.000000)  vcan0  TX - -  7E0   [8]  03 22 F1 90 00 00 00 00
 (000.010120)  vcan0  TX - -  7E8   [8]  10 14 62 F1 90 54 41 4B
 (000.012931)  vcan0  TX - -  7E0   [8]  30 00 00 CC CC CC CC CC
 (000.020290)  vcan0  TX - -  7E8   [8]  21 54 46 4C 57 43 56 43
 (000.040632)  vcan0  TX - -  7E8   [8]  22 30 30 30 30 30 30 31
```

Decoded:

| t (s)     | Id     | PCI / Data                  | ISO-TP role   | UDS bytes             |
|-----------|--------|-----------------------------|---------------|-----------------------|
| 0.000000  | 0x7E0  | `03 22 F1 90`               | SingleFrame   | `22 F1 90` (RDBI VIN) |
| 0.010120  | 0x7E8  | `10 14 62 F1 90 54 41 4B`   | FirstFrame    | total=0x14, `62 F1 90 TAK` |
| 0.012931  | 0x7E0  | `30 00 00 CC CC CC CC CC`   | FlowControl   | CTS, BS=0, STmin=0    |
| 0.020290  | 0x7E8  | `21 54 46 4C 57 43 56 43`   | ConsecutiveFrame #1 | `TFLWCVC`       |
| 0.040632  | 0x7E8  | `22 30 30 30 30 30 30 31`   | ConsecutiveFrame #2 | `0000001`       |

Reassembled UDS payload (20 bytes):
`62 F1 90 54 41 4B 54 46 4C 57 43 56 43 30 30 30 30 30 30 31`
i.e. positive RDBI response `62 F1 90` + VIN `TAKTFLWCVC0000001`
(matches `firmware/ecu/cvc/cfg/cvc_identity.toml`).

FC latency (FF -> FC): **2.8 ms**. Previous broken run (before the
busy-bus fix): 726 ms. Previous broken run (before the FC-emit fix):
never (4 s timeout).

## Test flow

1. pytest brings up the POSIX compose fleet (cvc, fzc, rzc).
2. pytest spawns the release-built proxy pointed at vcan0 with
   `opensovd-proxy-vcan.toml`.
3. pytest opens a TCP connection to `127.0.0.1:13400`, sends a DoIP
   routing activation and a diagnostic message carrying the
   UDS ReadDataByIdentifier 0x22 0xF1 0x90 request for CVC.
4. Proxy sends `03 22 F1 90` as ISO-TP SingleFrame on 0x7E0.
5. CVC container responds with FirstFrame on 0x7E8.
6. Proxy (via the new `recv_isotp_with_flow_control`) emits a
   spec-compliant FlowControl on 0x7E0 within ~3 ms.
7. CVC releases CF1 and CF2 back-to-back.
8. Proxy reassembles the 20-byte PDU, wraps it in a DoIP
   diagnostic-message positive ack, and returns it to the tester.
9. pytest asserts the UDS payload is `62 F1 90` + the 17-byte VIN
   from `cvc_identity.toml`.

## Related commits

- `8237edf` test(proxy-can): D1-red — ISO-TP multi-frame reassembly with FlowControl
- `051f9b8` feat(proxy-can): D2-green — ISO-TP FC-send + CF-reassemble state machine
- `952fa94` feat(proxy-can): D3 wire FC-send into socketcan tx path
- `32fc08f` fix(proxy-can): D3 run ECU sim on std::thread, multi-thread runtime
- `8525fa1` fix(proxy-can): D4 stop starving recv loop on busy buses
