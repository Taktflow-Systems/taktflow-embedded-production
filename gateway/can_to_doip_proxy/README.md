<!--
SPDX-License-Identifier: Apache-2.0
SPDX-FileCopyrightText: 2026 Taktflow Systems
-->

# CAN-to-DoIP Proxy

Pi-hosted Rust service that bridges DoIP (ISO 13400) over TCP/UDP to UDS
over CAN ISO-TP. Lets an upstream CDA reach physical STM32 / TMS570 ECUs
that do not have an Ethernet stack.

See [ADR-0004](../../../eclipse-opensovd/docs/adr/0004-can-to-doip-proxy-on-raspberry-pi.md)
and [ADR-0010](../../../eclipse-opensovd/docs/adr/0010-doip-discovery-both-broadcast-and-static.md)
for the decision record.

## Crates

- `proxy-core` — routing table, DoIP-to-CAN translation, discovery logic,
  all pure logic with no I/O. Host-independent; tests on Windows and Linux.
- `proxy-doip` — DoIP TCP/UDP server. Wire format bit-for-bit compatible
  with `firmware/platform/posix/src/DoIp_Posix.c`.
- `proxy-can` — SocketCAN + ISO-TP client. Linux-only; stubbed on
  non-Linux so the workspace still compiles on Windows dev boxes.
- `proxy-main` — binary entry point. Clap CLI + figment config + tokio
  runtime.

## Non-goals

- No DoIP stack on the ECU firmware. Per ADR-0011 that is deferred
  post-MVP.
- No CDA fork. The proxy is transparent from the CDA's point of view.
