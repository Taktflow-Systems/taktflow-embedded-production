# S-XCP-01 — XCP transport assessment memo (SC, XCP over Ethernet/UDP)

Status: DELIVERED — decision pending approval (roadmap gate: plan review
before implementation).
Date: 2026-07-07. Feeds: `docs/plans/plan-sc-ethernet-roadmap.md` Phase 2
(S-XCP-02, S-XCP-03).

## How to read this

Audience: the AI/human worker who will implement S-XCP-02 cold, and the
reviewer approving the architecture. Section 1 is the evidence (file:line
citations into this repo); section 2 weighs the three candidate
architectures; section 3 is the chosen design specified to
function-signature level; section 4 estimates the remaining Phase 2 steps
against their roadmap IDs. Rules that bind this memo:
`.claude/rules/firmware-general.md` (SC: no BSW, minimal auditable code)
and the roadmap's step definitions.

## 1. Findings (evidence)

### 1.1 The existing Xcp module is CAN/PduR-bound with no transport seam

- `firmware/bsw/services/Xcp/src/Xcp.c` (629 lines, "~3 KB Flash, ~256 B
  RAM", `Xcp.c:13`): minimal ASAM XCP V1.5 slave, polling, no DAQ/STIM
  (`Xcp.c:6-13`); commands CONNECT/DISCONNECT/GET_STATUS/
  GET_COMM_MODE_INFO/SHORT_UPLOAD/SHORT_DOWNLOAD/SET_MTA/UPLOAD/
  GET_SEED/UNLOCK (`Xcp.c:592-623`).
- TX is a direct `PduR_Transmit` call (`Xcp.c:25,59`); RX arrives via
  PduR routing `PDUR_DEST_XCP -> Xcp_RxIndication` (`PduR.c:80-81`);
  config is two CAN PDU IDs only (`Xcp.h:67-70`); CTO/DTO fixed at 8
  (`Xcp.h:56-57`). There is no transport abstraction to plug UDP into.
- It also includes `Det.h`/`ComStack_Types.h` — BSW headers.
- Enabled on CVC/FZC/RZC only (`cvc/src/main.c:186-189,393`,
  `fzc/src/main.c:414-419`, `rzc/src/main.c:337-342`). The SC's generated
  XCP CAN config (`Sc_Cfg.h:192-246`, `CanIf_Cfg_Sc.c:60-66`, 0x556/0x557)
  is dead placeholder — the SC links neither PduR nor Xcp.
- Reusable regardless of transport: the address-validation whitelist
  already contains the TMS570 flash/SRAM ranges (`Xcp.c:89-121,108`) and
  the Seed&Key logic (MISRA deviation DEV-003 documented,
  `docs/safety/analysis/misra-deviation-register.md:34,97-99`).

### 1.2 The SC's Ethernet RX path is present but unused; UDP is TX-only

- `Sc_Eth_PollRx(uint8 *frame, uint16 frame_size, uint16 *frame_len)`
  (`sc_eth.h:45`, `sc_eth.c:211-262`) returns one raw L2 frame from the
  EMAC ring; production `sc_main.c` never calls it (only
  `test_eth_ping.c:584` and unit tests do).
- `sc_eth_udp.c` builds Ethernet II + IPv4 + UDP TX frames
  (`Sc_EthUdp_BuildFrame` `:104-189`, `Sc_EthUdp_Send` `:191-214`) with
  reusable ones-complement checksum helpers (`:248-302`); "does not
  implement ARP, routing, fragmentation, or RX processing"
  (`sc_eth_udp.h:6-9`).
- EMAC accepts broadcast + own-unicast (no promiscuous): `HL_emac.c`
  RXBROADEN `:528-530`, RXUNICASTSET `:605`. No ARP responder in the
  production image (an ARP+ICMP implementation exists in the standalone
  `test_eth_ping.c` and can be lifted later if needed).
- ASAM XCP-on-Ethernet prepends a 4-byte transport header (LEN u16 +
  CTR u16, little-endian) to every packet — required new logic in any
  architecture.

### 1.3 SC charter and the proven shim precedent

- `.claude/rules/firmware-general.md:16-17`: SC runs **no AUTOSAR BSW**;
  minimal auditable code only. SC Ethernet code is QM instrumentation
  compiled only under `SC_ETH_ENABLE` and excluded from safety builds
  (`sc_eth.c:8-17`).
- `firmware/ecu/sc/src/sc_uds_shim.c` (185 lines) is the house pattern
  for a minimal protocol slave on the SC: single file, Init/Poll pair
  wired into the main loop (`sc_main.c:170,226`), `switch` dispatch,
  fixed local buffers, zero BSW.
- Resources are a non-issue: the whole ETH stack costs ~5.8 KB flash /
  ~16.7 KB RAM of the 2 MB / 518 KB available (map summaries,
  `build/tms570-sudp05-eth-hil/sc.map:15-18`).
- Tooling: `tools/xcp/xcp_test.py` already maps `'sc': (0x556, 0x557)`;
  `tools/xcp/gen_a2l.py:133` hard-codes little-endian `BYTE_ORDER
  MSB_LAST` — needs a byte-order option for the big-endian TMS570
  (S-XCP-03 scope).
- Prior discussion (`docs/plans/gap-analysis-hil-bench-vs-professional.md:441-459`)
  lists XCPlite (MIT, CAN+Ethernet) and pyxcp as the DIY route.

## 2. Options

| Option | Assessment |
|---|---|
| **Reuse-with-transport** (port `firmware/bsw/services/Xcp` onto UDP) | Rejected. The module has no transport seam (1.1) — porting means inventing an abstraction plus stubbing PduR/Det/ComStack on the SC, importing exactly the BSW surface the SC charter bans (1.3). The gain (already-written command handlers) is available more cheaply by copying the ~200 lines of transport-independent logic. |
| **Minimal SC-side slave** (new `sc_xcp_eth.c` per the `sc_uds_shim` pattern) | **Chosen.** Honors the SC charter, mirrors a proven in-repo pattern, reuses the Xcp command/whitelist/Seed&Key logic as copied portable helpers (with "must match Xcp.c" provenance comments, same convention as `hb_spoofer.py` vs `E2E.c`), and stays inside `SC_ETH_ENABLE`/QM gating. Roadmap S-XCP-02 already reserves the exact file paths for this shape. |
| **External stack (XCPlite)** | Rejected for the slave: designed around OS sockets/threads; porting to bare-metal big-endian TMS570 contradicts "minimal, auditable". pyxcp remains attractive for the *master*/smoke side and is already referenced in `tools/xcp/xcp_config.json`. |
| **Defer** | Rejected — Phase 2 is the approved roadmap and S-UDP-05 closed Phase 1 with the channel validated. |

**Decision (pending approval): minimal-slave.**

## 3. Chosen architecture — RX dispatch + XCP slave, to signature level

New/changed units (paths fixed by roadmap S-XCP-02):

```c
/* sc_eth_udp.h — additions (decoder mirroring the encoder) */
typedef struct {
    uint8  src_ip[4];
    uint16 src_port;
    uint16 dst_port;
} sc_eth_udp_rx_meta_t;

boolean Sc_EthUdp_ParseFrame(const uint8* frame, uint16 frame_len,
                             sc_eth_udp_rx_meta_t* meta,
                             const uint8** payload, uint16* payload_len);
/* validates EtherType 0x0800, IHL 0x45, proto 17; bounds-checks all
 * lengths (fail-closed on any mismatch); reuses sc_eth_udp_*checksum */

/* firmware/ecu/sc/src/sc_eth_rx_dispatch.c (new) + header */
typedef void (*sc_eth_rx_handler_t)(const uint8* payload, uint16 len,
                                    const sc_eth_udp_rx_meta_t* meta);
void    Sc_EthRx_Init(void);
boolean Sc_EthRx_RegisterPort(uint16 udp_port, sc_eth_rx_handler_t handler);
        /* static table, SC_ETH_RX_MAX_PORTS = 4, no allocation */
void    Sc_EthRx_Poll(void);
        /* one Sc_Eth_PollRx per call; internal EtherType switch so an
         * ARP responder can be added later without redesign; IPv4/UDP ->
         * port table lookup -> handler; silently drops non-matching */

/* firmware/ecu/sc/src/sc_xcp_eth.c (new) + header */
void Sc_XcpEth_Init(void);   /* registers SC_XCP_ETH_PORT (55002 — 55001
                                is telemetry) with the dispatcher */
/* internal handler: strips/validates the ASAM XCP-on-Ethernet 4-byte
 * header (LEN u16 LE + CTR u16 LE), dispatches the CTO command subset
 * CONNECT / DISCONNECT / GET_STATUS / SHORT_UPLOAD / SET_MTA / UPLOAD /
 * GET_SEED / UNLOCK (copied-portable from Xcp.c incl. the TMS570
 * address whitelist Xcp.c:89-121), replies via Sc_EthUdp_Send to
 * meta->src_ip / meta->src_port with echoed CTR */
```

Main-loop integration mirrors the shim precedent: `Sc_EthRx_Init()` +
`Sc_XcpEth_Init()` beside `Sc_Eth_Init` (`sc_main.c` init block) and
`Sc_EthRx_Poll()` in the loop next to `SC_UdsShim_Poll()`
(`sc_main.c:226`), all inside `#ifdef SC_ETH_ENABLE`.

Operational notes: the bench master must either use a static ARP entry
for the SC's MAC (no ARP responder in production firmware yet) or the
ARP handler from `test_eth_ping.c` gets lifted into the dispatcher as a
follow-up; XCP-on-UDP responses are unicast-to-sender so no broadcast
storm risk; everything is QM, `SC_ETH_ENABLE`-gated, absent from safety
builds.

## 4. Effort estimate per remaining Phase 2 step

- **S-XCP-02** (RX dispatch + slave; CONNECT + SHORT_UPLOAD working):
  ~1 bench day. `sc_eth_udp` parse additions ~120 LOC;
  `sc_eth_rx_dispatch.c` ~150 LOC; `sc_xcp_eth.c` ~300 LOC; Unity tests
  first (`test_sc_xcp_eth.c`, frame parse/build vectors) ~250 LOC;
  `tools/bench/xcp_smoke.py` (stdlib socket or pyxcp) ~150 LOC; bench
  validation of the DoD (changing counter read twice, 1 s apart).
  Risk buffer: F-DCAN-RX-style surprises in EMAC RX under load —
  mitigated by the S-UDP-05 tooling (driver + receiver) for regression
  checks.
- **S-XCP-03** (A2L generation): ~0.5 day. `gen_a2l.py` byte-order
  option (`MSB_FIRST` for TMS570), TI toolchain `nm` output handling,
  SC ELF/map integration, smoke against the S-XCP-02 counters.

## 5. Approval

- [ ] Decision "minimal-slave" approved (roadmap gate for S-XCP-02 —
  do not start implementation before this box is ticked by the
  reviewer).
