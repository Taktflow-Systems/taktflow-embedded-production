# Plan: SC (TMS570) Ethernet Exploitation Roadmap

## How to read this

Audience: an AI worker (or engineer) landing cold, with no prior conversation
context. Each phase below is broken into steps; every step carries a stable
Step ID, Goal, Inputs, Deliverables (concrete paths), Acceptance criteria,
Gate/review reference, and Definition of done — execute a step without asking
follow-up questions. Project gates and coding rules live in
`.claude/rules/` (development-discipline.md, c-code.md, firmware-general.md,
asil-d.md) and `docs/reference/`. Phases are strictly ordered by dependency;
steps inside a phase are ordered but may be reviewed independently.
Status markers: PENDING / IN PROGRESS / DONE.

## Context and constraints (read before any step)

- Ethernet bring-up on the LAUNCHXL2-570LC43 (TMS570LC4357, DP83630 PHY) is
  DONE and verified end-to-end (ARP + ICMP, 0% loss). Reference
  implementation: `firmware/ecu/sc/test/test_eth_ping.c` (commit `6a98475`).
  Hardware facts (PHY power-down pin, EDCR, clock source) are documented in
  that file's comments and in the bring-up handoff records.
- Board IP/MAC are defined in `test_eth_ping.c` (`g_my_ip`, `g_my_mac`).
  Plans and docs must not duplicate literal bench IPs (privacy rule).
- SAFETY CONSTRAINT: the SC is the ASIL-D lockstep safety controller
  (`.claude/rules/firmware-general.md`: minimal, auditable code, no large
  stacks). ALL Ethernet code in the SC application build must sit behind a
  dedicated build flag (`SC_ETH_ENABLE`, set by `ETH=1` make argument) and be
  OFF by default. Ethernet features are QM bench/HIL instrumentation, not
  safety functions; they must never run in the safety build.
- Known hardware quirk: GIOA[3] drive does not reach ball E1 (PHY PWRDOWN
  pin); the PHY is woken in software via MICR.INT_OE. GIOA[3]/GIOA[4] also
  conflict with SC LED assignments (see Phase 0).
- MISRA C:2012 applies to all new hand-written firmware
  (`.claude/rules/c-code.md`); TDD with Unity applies to all logic testable
  off-target (`test_<module>.c` BEFORE implementation).

---

## Phase 0 — Foundations (shared by all later phases) — DONE

#### S-ETH-00 — Ethernet bring-up (reference datapath) — DONE
- **Goal:** Prove EMAC + DP83630 datapath end-to-end on the SC LaunchPad.
- **Inputs:** none (complete).
- **Deliverables:** `firmware/ecu/sc/test/test_eth_ping.c` (commit `6a98475`).
- **Acceptance criteria:** ICMP echo from bench PC answered, 0% loss — MET.
- **Gate / review:** ungated (standalone test firmware).
- **Definition of done:** ping succeeds from bench PC — observed 2026-07-06.

#### S-ETH-01 — Extract reusable Ethernet driver module — DONE
- **Goal:** Lift the proven wake/link/RX-poll/cache logic out of the test
  into a reusable, MISRA-clean SC module usable by the SC application build.
- **Inputs:** `firmware/ecu/sc/test/test_eth_ping.c` (working reference);
  `firmware/ecu/sc/halcogen/` EMAC/MDIO drivers.
- **Deliverables:**
  - `firmware/ecu/sc/include/sc_eth.h` — API: `Sc_Eth_Init` (wake PHY via
    MICR.INT_OE, link wait with timeout, EMACHWInit), `Sc_Eth_PollRx`
    (descriptor poll + cache-safe copy), `Sc_Eth_Tx` (cache-clean + transmit),
    `Sc_Eth_LinkUp`.
  - `firmware/ecu/sc/src/sc_eth.c` — implementation, guarded by
    `#ifdef SC_ETH_ENABLE`.
  - `firmware/ecu/sc/test/test_sc_eth.c` — Unity tests for the pure logic
    (descriptor flag parsing, bounds checks) with mocked BD structures,
    written BEFORE sc_eth.c per TDD rule.
- **Acceptance criteria:**
  - `test_sc_eth.c` passes under `make test`.
  - Rebuilt `test_eth_ping.c` refactored to call sc_eth API still answers
    ping 6/6 on bench.
  - cppcheck MISRA addon: zero mandatory violations in sc_eth.c.
- **Gate / review:** development-discipline.md Layer 1 (unit) + Layer 4
  (single ECU on bench); MISRA check per c-code.md.
- **Definition of done:** ping test passes on bench using only the sc_eth
  API, with unit tests green in CI.

#### S-ETH-02 — GIOA[3]/[4] pin reconciliation decision memo — DONE
- **Goal:** Resolve the conflict between PHY control pins and SC LED
  assignments so Ethernet can coexist with the SC application.
- **Inputs:** `firmware/ecu/sc/include/Sc_Hw_Cfg.h` (LED pin map);
  `test_eth_ping.c` header comments (pin facts); bring-up handoff records.
- **Deliverables:** `docs/plans/memo-sc-eth-pin-reconciliation.md` — decision:
  which pins the SC build uses for LEDs when `SC_ETH_ENABLE` is set, whether
  GIOA[3] keeps driving high (harmless; MICR.INT_OE makes it optional), and
  the resulting `Sc_Hw_Cfg.h` diff (applied in the same step).
- **Acceptance criteria:** memo lists every GIOA bit used by SC firmware and
  by the PHY with its source file reference; `Sc_Hw_Cfg.h` compiles in both
  `ETH=1` and default builds; SC LED behavior in default build unchanged
  (verified on bench by observing boot LED pattern).
- **Gate / review:** ungated (QM config change); reviewed via PR.
- **Definition of done:** both build variants compile and boot on bench with
  documented pin ownership.

#### S-ETH-03 — Build-flag integration in Makefile.tms570 — DONE
- **Goal:** One-flag opt-in Ethernet for the SC application binary.
- **Inputs:** S-ETH-01, S-ETH-02 deliverables;
  `firmware/platform/tms570/Makefile.tms570`.
- **Deliverables:** Makefile.tms570 change: `ETH=1` defines `SC_ETH_ENABLE`,
  compiles sc_eth*.c, links EMAC/MDIO/PHY HALCoGen objects into `sc.elf`;
  default build excludes them (checked via map file).
- **Acceptance criteria:**
  - `make ... ETH=1` produces sc.elf containing sc_eth symbols; default
    `make` produces sc.elf with zero EMAC/sc_eth symbols (grep the .map).
  - Flash-size report (`make size`) for both variants recorded in the PR
    description; default-build size unchanged.
- **Gate / review:** build-and-ci.md size budget rule.
- **Definition of done:** both variants build reproducibly from a clean tree.

---

## Phase 1 — UDP telemetry channel (no TCP/IP stack) — IN PROGRESS

#### S-UDP-01 — Telemetry wire-format specification — DONE
- **Goal:** Define a versioned, decodable-forever packet layout for SC
  telemetry over UDP.
- **Inputs:** `firmware/ecu/sc/src/sc_monitoring.c` (available counters);
  `firmware/ecu/sc/src/sc_main.c` (VSM state variables).
- **Deliverables:** `docs/api/sc-eth-telemetry-format.md` — header (magic,
  version, sequence number, timestamp source), field table (name, offset,
  type, unit, source symbol), byte order (big-endian, matching TMS570).
- **Acceptance criteria:** every field maps to a named symbol in SC source
  with file:line reference; sequence-number wraparound and loss-detection
  semantics specified; reviewed against sc_monitoring.c for completeness.
- **Gate / review:** ungated (QM doc); PR review.
- **Definition of done:** doc merged; Python receiver (S-UDP-04) can be
  written from the doc alone without reading firmware source.

#### S-UDP-02 — Minimal IP/UDP frame encoder — DONE
- **Goal:** Build Ethernet+IPv4+UDP headers around a payload with correct
  checksums — TX only, no stack.
- **Inputs:** S-ETH-01 (`Sc_Eth_Tx`); S-UDP-01 format doc.
- **Deliverables:**
  - `firmware/ecu/sc/test/test_sc_eth_udp.c` — Unity tests FIRST: header
    field placement, IPv4 header checksum against known-good reference
    vectors, length fields, broadcast vs unicast destination MAC handling.
  - `firmware/ecu/sc/include/sc_eth_udp.h`, `firmware/ecu/sc/src/sc_eth_udp.c`
    — `Sc_EthUdp_Send(dst_ip, dst_port, payload, len)`; pure functions for
    header building (unit-testable off-target).
- **Acceptance criteria:** unit tests green in `make test`; on bench, a
  Wireshark/tshark capture on the PC NIC shows well-formed UDP packets
  (no checksum errors flagged) from the board.
- **Gate / review:** development-discipline.md Layer 1 + Layer 4; MISRA
  check; TDD hook (tests exist before implementation commit).
- **Definition of done:** bench validation confirms 1000 consecutive UDP
  datagrams from the board with zero malformed payloads and zero sequence
  gaps; pcap/tshark raw-frame validation remains an optional repeat when
  Npcap or another capture backend is available.
- **Current status:** done. The standalone TMS570 UDP probe was flashed over
  XDS110 and a PC-side UDP receiver accepted 1000 consecutive S-UDP-02 probe
  datagrams with zero gaps and zero invalid payloads.

#### S-UDP-03 — SC telemetry producer integration
- **Goal:** Emit the S-UDP-01 packet from the SC main loop at a fixed
  cadence without violating loop timing.
- **Inputs:** S-UDP-02 encoder; `sc_main.c` scheduler structure;
  S-ETH-02/03 (pins + build flag).
- **Deliverables:** `firmware/ecu/sc/src/sc_eth_telemetry.c` +
  `firmware/ecu/sc/include/sc_eth_telemetry.h` — cadence-driven producer
  (default 100 Hz, compile-time constant) called from the main loop under
  `SC_ETH_ENABLE`; sequence counter; populated from sc_monitoring/VSM state.
- **Acceptance criteria:**
  - `ETH=1` build boots on bench, streams packets at configured rate
    (measured by receiver, S-UDP-04): rate error < 1% over 60 s.
  - Main-loop period jitter with ETH=1 vs default build measured (existing
    debug counters or GIO toggle + scope): degradation < 5%; watchdog never
    trips over a 10-minute soak.
  - Default build binary is byte-identical in behavior (no telemetry code).
- **Gate / review:** asil-d.md WCET concern (bench measurement recorded in
  PR); development-discipline.md Layer 4.
- **Definition of done:** 10-minute soak on bench: zero watchdog resets,
  zero sequence gaps at the receiver.

#### S-UDP-04 — PC-side telemetry receiver
- **Goal:** Decode and record the telemetry stream on the bench PC.
- **Inputs:** S-UDP-01 format doc only (deliberate independence check).
- **Deliverables:** `tools/bench/eth_telemetry_rx.py` — binds the bench NIC,
  decodes per format doc, live table print + CSV logging + gap/loss report;
  `tools/bench/README.md` section describing usage.
- **Acceptance criteria:** decodes a live stream from the board; reports
  sequence gaps; CSV columns match format-doc field names; runs on Python
  3.10+ with stdlib only (no pip installs).
- **Gate / review:** ungated (QM tooling); PR review.
- **Definition of done:** 60 s capture from the bench produces a CSV with
  zero decode errors and a printed loss report of 0.

#### S-UDP-05 — Bench validation against a HIL scenario
- **Goal:** Prove the telemetry channel is trustworthy during real HIL load
  (CAN traffic + fault injection running).
- **Inputs:** S-UDP-03 firmware, S-UDP-04 receiver, existing HIL scenarios in
  `test/hil/`.
- **Deliverables:** `test/hil/reports/sc-eth-telemetry-validation.md` —
  procedure, scenario used, UDP-reported counters cross-checked against
  UART-reported counters at test end, loss/jitter numbers.
- **Acceptance criteria:** counter values via UDP match UART within one
  sampling period; packet loss 0 over the scenario run; CAN scenario results
  unchanged vs a run without ETH=1 (no interference).
- **Gate / review:** development-discipline.md Layer 5 (multi-node bench).
- **Definition of done:** report merged with all three criteria met.

---

## Phase 2 — XCP over Ethernet (UDP transport) — PENDING

#### S-XCP-01 — XCP transport abstraction assessment
- **Goal:** Determine how the existing CAN-based Xcp module can accept a
  UDP transport, or whether a minimal SC-side XCP slave is a better fit
  (SC has no BSW stack by design).
- **Inputs:** existing Xcp module sources (locate under `firmware/bsw/` /
  ECU configs — enumerate in the memo); S-UDP-02 encoder; XCP part 2
  transport-layer spec for Ethernet (ASAM, UDP variant).
- **Deliverables:** `docs/plans/memo-sc-xcp-eth-transport.md` — findings with
  file:line citations, chosen architecture, RX-path requirement analysis
  (XCP needs board-side RX: extend `Sc_Eth_PollRx` dispatch to UDP port
  filtering — specify the change), effort estimate per remaining step.
- **Acceptance criteria:** memo cites the actual Xcp source files found;
  decision is one of (reuse-with-transport / minimal-slave / defer) with
  rationale; RX dispatch design specified to function-signature level.
- **Gate / review:** plan review before implementation (workflow rule:
  plan -> approve -> code).
- **Definition of done:** memo merged and decision approved.

#### S-XCP-02 — UDP RX dispatch + XCP slave connect/read
- **Goal:** Implement the memo's chosen architecture far enough that an XCP
  master can CONNECT and read one measurement over Ethernet.
- **Inputs:** S-XCP-01 memo (blocking: do not start without it).
- **Deliverables (paths fixed, content per memo):**
  - `firmware/ecu/sc/src/sc_eth_rx_dispatch.c` — UDP port -> handler table.
  - `firmware/ecu/sc/src/sc_xcp_eth.c` — XCP-on-UDP binding (CONNECT,
    DISCONNECT, SHORT_UPLOAD minimum command set).
  - `firmware/ecu/sc/test/test_sc_xcp_eth.c` — Unity tests for frame
    parse/build, written first.
  - `tools/bench/xcp_smoke.py` — pyxcp-or-stdlib client performing
    CONNECT + SHORT_UPLOAD of a known symbol address.
- **Acceptance criteria:** xcp_smoke.py connects and reads a known volatile
  counter from the running board; unit tests green; MISRA clean.
- **Gate / review:** Layers 1+4; MISRA; TDD hook.
- **Definition of done:** smoke script output shows a changing counter value
  read over XCP twice, one second apart.

#### S-XCP-03 — Measurement description (A2L) generation
- **Goal:** Make XCP measurements tool-consumable without hand-maintained
  addresses.
- **Inputs:** S-XCP-02; `build/tms570/sc.map` (symbol addresses);
  `tools/codegen/` Jinja2 infrastructure.
- **Deliverables:** `tools/xcp/gen_a2l.py` — parses the .map + a YAML symbol
  allowlist (`tools/xcp/xcp_symbols.yaml`) and emits
  `build/tms570/sc_xcp.a2l`; Makefile hook `make a2l`.
- **Acceptance criteria:** generated A2L loads in the chosen master tooling
  (xcp_smoke.py extended to consume it) and resolves >= 5 real symbols;
  regenerating after a rebuild picks up moved addresses (verified by
  intentionally reordering one variable).
- **Gate / review:** tool-qualification note per firmware-scripts.md (TCL1,
  bench-only tooling — record in the script header).
- **Definition of done:** `make a2l` output drives a successful scripted
  measurement session after a fresh rebuild.

---

## Phase 3 — lwIP integration (TCP/IP stack, QM builds only) — PENDING

#### S-LWIP-01 — Vendor import and wrap
- **Goal:** Bring lwIP into the repo per vendor-independence rules.
- **Inputs:** lwIP release (pin exact version in the import commit);
  `.claude/rules/firmware-general.md` (vendor wrap rule).
- **Deliverables:** `firmware/lib/vendor/lwip/` (pristine, pinned, license
  file retained); `docs/plans/memo-lwip-version-pin.md` recording version,
  checksum, and CVE review date.
- **Acceptance criteria:** vendor tree untouched by hand (checksum matches
  upstream release); memo lists version + license + known CVEs consulted.
- **Gate / review:** firmware-scripts.md supply-chain rule.
- **Definition of done:** vendor import commit merged, separate from any
  port-layer code.

#### S-LWIP-02 — TMS570 port layer (NO_SYS mode)
- **Goal:** Run lwIP main-loop-driven (NO_SYS=1, no OS) on top of sc_eth.
- **Inputs:** S-LWIP-01; S-ETH-01 driver; TI/HALCoGen lwIP demo and the
  Jan Cumps guide as reference ports.
- **Deliverables:** `firmware/ecu/sc/lwip_port/lwipopts.h`,
  `firmware/ecu/sc/lwip_port/sc_netif.c` (netif driver bridging
  sc_eth RX poll/TX, with the established cache maintenance),
  `firmware/ecu/sc/lwip_port/sc_lwip.c` (init + timers from main loop).
- **Acceptance criteria:** board answers ping via lwIP (test_eth_ping raw
  path disabled in this build variant to prove it is lwIP responding);
  TCP echo server on a fixed port echoes 1 MB from a PC client without
  stalling; RAM usage delta documented from .map.
- **Gate / review:** Layer 4 bench test; size budget per build-and-ci.md.
- **Definition of done:** TCP echo transfers 1 MB with zero retransmission
  storms (tshark inspection) on bench.

#### S-LWIP-03 — Build variant and soak
- **Goal:** Gate lwIP behind its own flag and prove stability.
- **Inputs:** S-LWIP-02; S-ETH-03 flag scheme.
- **Deliverables:** Makefile.tms570 `ETH_STACK=lwip` variant; 1-hour soak
  procedure + results in `test/hil/reports/sc-lwip-soak.md`.
- **Acceptance criteria:** 1-hour soak with periodic ping + TCP echo load:
  zero watchdog resets, zero lwIP assertion hits, heap/pool high-water marks
  recorded and below 80% of configured pools.
- **Gate / review:** development-discipline.md Layer 4/5.
- **Definition of done:** soak report merged with all criteria met.

---

## Phase 4 — DoIP / UDS over IP (depends on Phase 3) — PENDING

#### S-DOIP-01 — Scope and architecture memo
- **Goal:** Decide DoIP node placement (SC engineering build vs future
  non-safety gateway) and the UDS service subset to expose.
- **Inputs:** `firmware/bsw/services/Dcm/` (existing UDS services);
  `firmware/ecu/sc/src/sc_uds_shim.c` (current shim work — note: verify
  its commit status first; it was untracked on 2026-07-06);
  ISO 13400-2 (DoIP), existing `tools/odx-gen/`.
- **Deliverables:** `docs/plans/memo-sc-doip-scope.md` — placement decision,
  service subset (minimum: 0x10, 0x22, 0x19, 0x14), routing activation
  policy, security posture (bench-only network, no authentication — state
  it explicitly), step-level effort estimates.
- **Acceptance criteria:** memo addresses vehicle-announcement/discovery
  (UDP 13400), diagnostic session over TCP 13400, and how Dcm dispatch is
  reached from lwIP callbacks, each to function-signature level.
- **Gate / review:** plan review before code (workflow rule).
- **Definition of done:** memo merged and decision approved.

#### S-DOIP-02 — DoIP endpoint implementation
- **Goal:** UDS request/response over DoIP from a PC client.
- **Inputs:** S-DOIP-01 memo (blocking); S-LWIP-03 stable stack.
- **Deliverables:** `firmware/ecu/sc/src/sc_doip.c` +
  `firmware/ecu/sc/include/sc_doip.h`; Unity tests
  `firmware/ecu/sc/test/test_sc_doip.c` (header parse/build, payload-type
  handling — written first); `tools/bench/doip_client.py` (stdlib socket
  client: discovery, routing activation, ReadDataByIdentifier).
- **Acceptance criteria:** doip_client.py completes discovery -> routing
  activation -> 0x22 read of an existing DID against the bench board;
  negative test: malformed DoIP header is rejected with correct NACK code;
  unit tests green; MISRA clean.
- **Gate / review:** Layers 1+4; MISRA; TDD hook; security note per
  firmware-scripts.md (fail-closed on unknown payload types).
- **Definition of done:** scripted end-to-end UDS read succeeds on bench and
  the negative test returns the specified NACK.

---

## Phase 5 — IEEE 1588 PTP time sync (independent of Phases 2-4) — PENDING

#### S-PTP-01 — PHY 1588 capability bring-up memo
- **Goal:** Establish what the DP83630 hardware timestamping requires from
  the SC (register programming, GPIO trigger routing, clock topology) and
  define the bench synchronization experiment.
- **Inputs:** DP83630 datasheet SNLS335B sections on 1588; schematic PHY
  GPIO wiring (documented in bring-up research handoff); S-ETH-01 driver.
- **Deliverables:** `docs/plans/memo-sc-ptp-bringup.md` — register/sequence
  plan for RX/TX timestamp capture, chosen sync topology (PC as master with
  software PTP vs PHY-to-PHY), measurable success metric definition.
- **Acceptance criteria:** memo specifies every PHY register write needed
  for timestamp capture with register page/address, and defines the
  verification experiment with expected accuracy bound and how it is
  measured.
- **Gate / review:** plan review before code.
- **Definition of done:** memo merged and experiment design approved.

#### S-PTP-02 — Timestamp capture and offset measurement
- **Goal:** Capture hardware RX timestamps on the board and quantify
  PC-to-board time offset stability.
- **Inputs:** S-PTP-01 memo (blocking).
- **Deliverables:** `firmware/ecu/sc/src/sc_eth_ptp.c` (+header, +Unity
  tests for timestamp field decode written first);
  `tools/bench/ptp_offset_probe.py` — sends timestamped probe packets,
  collects board hardware timestamps via the S-UDP telemetry channel,
  reports offset and drift statistics.
- **Acceptance criteria:** probe run of 10 minutes produces an offset series
  with jitter statistics; hardware timestamps monotonic and consistent with
  the configured PHY clock rate; results appended to the memo.
- **Gate / review:** Layer 4 bench test.
- **Definition of done:** offset/drift report exists with plausible,
  reproducible numbers over two independent runs.

---

## Cross-cutting exit criteria (apply to every phase)

- Ethernet code compiled ONLY under its build flag; default safety build
  contains zero Ethernet symbols (checked via .map in CI or PR review).
- Every new hand-written C file: MISRA-clean (cppcheck addon), Unity tests
  first for off-target-testable logic, `@brief/@param/@return` docs.
- Every step ends with a conventional commit referencing this plan's Step ID
  in the body; generated files (if any) committed separately as
  `chore(codegen):`.
- Bench IPs/MACs stay in source constants (`g_my_ip`) or local config files
  — never in docs, commit messages, or reports (privacy rule).
- This plan is updated (status markers) before starting the next phase
  (workflow rule: update plan before next phase).
