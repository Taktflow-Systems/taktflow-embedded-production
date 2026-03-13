# Lessons Learned — arxmlgen (AUTOSAR Code Generator)

## 2026-03-10 — Cross-ECU E2E Data ID Propagation

**Context:** Broadcast CAN means every non-sender ECU receives all messages. E2E data IDs were only applied to the owning ECU's PDUs, leaving RX copies on other ECUs without protection IDs.

**Mistake:** `_apply_pdu_e2e_map` was called per-ECU inside the sidecar loop, so only the ECU that *defined* the E2E map got data IDs on its PDUs. Other ECUs receiving the same PDU as RX had `e2e_data_id = None`.

**Fix:** Collect all `pdu_e2e_map` entries from all sidecar ECUs into a single global dict first, then apply across ALL ECUs after the per-ECU loop completes.

**Principle:** On broadcast CAN, any per-ECU annotation that applies to a PDU must be propagated to every ECU that routes that PDU. Think globally, apply globally.

---

## 2026-03-10 — Missing Sidecar Entries for ICU/TCU E2E

**Context:** After fixing cross-ECU propagation, 14 warnings remained for ICU_Heartbeat and TCU_Heartbeat — these PDUs had no E2E data IDs at all.

**Mistake:** `ecu_sidecar.yaml` had E2E entries for CVC, FZC, RZC, SC, BCM but not ICU or TCU. The sidecar was incomplete.

**Fix:** Added `pdu_e2e_map` entries for ICU (0x31) and TCU (0x41) heartbeats to the sidecar YAML.

**Principle:** When a pattern applies to "all ECUs of type X", verify coverage exhaustively. Don't assume the config file is complete — count entries vs. expected ECUs.

---

## 2026-03-10 — Broadcast CAN Makes Signal/PDU Counts Equal Across ECUs

**Context:** Tests asserted `cvc_signals > bcm_signals` expecting CVC to have more signals than BCM.

**Mistake:** On broadcast CAN, every non-sender ECU receives ALL messages. This means signal counts, RX PDU counts, etc. are identical across all ECUs — only TX counts differ.

**Fix:** Changed assertions from `>` to `>=` where comparing cross-ECU counts.

**Principle:** Broadcast CAN equalizes RX-side counts. Only TX-side counts are ECU-specific. Design tests with this topology in mind.

---

## 2026-03-10 — RTE Signal Count = 16 BSW + App Signals (Not Just Map Length)

**Context:** `rte_signal_map` only contains app signals (ID >= 16). Tests and the Ecu_Cfg.h generator both need the total count including BSW reserved slots.

**Mistake:** Used `len(ecu.rte_signal_map)` as total signal count, which was 162 instead of 178 (missing the 16 BSW reserved slots).

**Fix:** `sig_count = 16 + len(ecu_app_signals)` in both the generator and tests. The generated output correctly produces `BCM_SIG_COUNT 178u`.

**Principle:** When a numbering scheme has reserved ranges, always account for them in counts. The map stores what's variable; the total includes what's fixed.

---

## 2026-03-10 — Init Runnables Have period_ms=0 (Event-Triggered, Not Invalid)

**Context:** Test `test_runnable_period_is_positive` failed because `Swc_DoorLock_Init` has `period_ms=0`.

**Mistake:** Assumed all runnables must have positive periods. Init runnables are event-triggered and correctly have `period_ms=0`.

**Fix:** Changed assertion to `period >= 0` and renamed test to `test_runnable_period_is_non_negative`.

**Principle:** Not all runnables are periodic. Event-triggered runnables (init, shutdown, mode-change) have zero period by design. Validate semantics, not arbitrary constraints.

---

## 2026-03-10 — SIG_COUNT Define Matched by Signal ID Regex

**Context:** Test checking sequential signal IDs found a "gap" — last signal was ID 177, but regex also matched `BCM_SIG_COUNT 178u`.

**Mistake:** Regex `#define\s+BCM_SIG_\w+\s+(\d+)u` matched both signal ID defines and the count define.

**Fix:** Used negative lookahead: `#define\s+BCM_SIG_(?!COUNT)\w+\s+(\d+)u` to exclude the count define.

**Principle:** When regex-scraping generated output, account for all defines that share a prefix. Use negative lookahead to exclude meta-defines (COUNT, MAX, TOTAL).

---

## 2026-03-10 — Shadow Buffer Type Regex Must Match `_t` Suffix and `boolean`

**Context:** Test for shadow buffers expected `static uint8 sig_*` but template generates `uint8_t` and `boolean` types.

**Mistake:** Regex `static\s+uint\d+\s+sig_` didn't match `uint8_t` or `boolean` types.

**Fix:** Updated to `static\s+(?:uint\d+_t|uint\d+|sint\d+_t|boolean)\s+sig_`.

**Principle:** C type names vary (`uint8` vs `uint8_t` vs `boolean`). When testing generated C code, match all type variants the template can produce.

---

## 2026-03-10 — Per-Module Jinja2 Templates Are Industry Standard

**Context:** Questioned whether building Jinja2 templates from project-specific code is scalable.

**Research:** Vector DaVinci (proprietary GENy engine), EB tresos (JET/Java), COMASSO (Eclipse Xpand), PySAR (Python+Jinja2) — all use per-module templates with a universal engine. Templates are the project-specific layer; the engine/model is reusable.

**Principle:** The template-per-module pattern is proven at scale across all major AUTOSAR tooling. Keep templates external and swappable. The engine reads a universal model; templates format it.

---

## 2026-03-10 — TDD Catches Model Assumptions Early

**Context:** Wrote 75 tests before implementing generators. Multiple tests failed on first run due to model assumptions (broadcast CAN counts, BSW offset, init runnable periods).

**Principle:** Writing tests first forces you to articulate what the generator should produce. When tests fail, they reveal model/assumption mismatches before they become bugs in generated C code that's hard to debug.

---

## 2026-03-10 — Port Signal Names vs CAN Signal Names Are Different Namespaces

**Context:** Port types stayed `uint32_t` (unresolved) even after adding `_resolve_port_types`. Port `signal_name = "VehicleState"` but CAN signal `name = "Vehicle_State_VehicleState"`.

**Mistake:** Assumed port signal names match CAN signal names exactly. They don't — CAN signals are PDU-prefixed (`{PduName}_{SignalName}`), while ports derive names from S/R interfaces (just `{SignalName}`).

**Fix:** Strip PDU name prefix from CAN signal names to build a suffix lookup table. Match port signal names against suffixes, falling back to exact match.

**Principle:** ARXML has two naming domains: PDU-scoped signals (CAN layer) and interface-scoped ports (SWC layer). When bridging them, always account for the PDU name prefix.

---

## 2026-03-10 — uint32_t Is a Valid Resolved Type, Not Always "Untyped"

**Context:** Test asserted `param_type != "uint32_t"` to catch unresolved port types. But some CAN signals genuinely are 32-bit (e.g., FaultMask bitmask).

**Mistake:** Conflated "uint32_t" (the default unresolved value) with "uint32_t" (a legitimate 32-bit signal type). The test rejected valid resolved types.

**Fix:** Removed the `!= "uint32_t"` assertion. Type resolution correctness is validated by `test_port_types_not_all_uint32` (checks for type diversity) and `test_port_type_matches_signal_type` (checks individual matches).

**Principle:** Don't use sentinel values that overlap with valid data. If you must, test resolution via diversity or cross-referencing, not by excluding a specific value.

---

## 2026-03-10 — CanIf/PduR PDU IDs Mirror Com PDU IDs (1:1 Pass-Through)

**Context:** Designing CanIf and PduR config generators. Initial instinct was to assign separate CanIf PDU ID numbering.

**Insight:** In a single-bus CAN system with no gateway, CanIf PDU IDs and Com PDU IDs are the same — there's no ID translation. The PduR routing table is technically a no-op identity mapping (src == dst). But both tables are required by the AUTOSAR BSW module contracts for abstraction.

**Principle:** Even when a config table is trivial (identity mapping), generate it anyway if the BSW API requires it. The abstraction layer enables future multi-bus routing without changing application code.

---

## 2026-03-10 — Clean Templates Beat Complex Generators

**Context:** Phase 4 (CanIf + PduR) produced 44 tests, all passing on first run — zero failures.

**Insight:** Phases 1-3 required significant debugging (port type resolution, broadcast CAN counts, BSW signal offsets). Phase 4 had zero issues because: (1) the data model was already correct and battle-tested, (2) the template pattern was proven, (3) the new generators were structurally identical to Com_Cfg.

**Principle:** Investment in data model correctness (earlier phases) pays compound returns in later generators. When the model is right, generators are just template plumbing — write tests, write template, done.

---

## 2026-03-10 — include_in: None Means "All Generators Enabled"

**Context:** E2E test assumed BCM/ICU/TCU would be excluded from E2E config because the plan listed only CVC/FZC/RZC for E2E.

**Mistake:** BCM/ICU/TCU have no `include_in` in project.yaml (it's `None`), which means ALL generators run — including E2E. Only SC has explicit `include_in: ["canif"]`.

**Fix:** Updated test to expect BCM/ICU/TCU in E2E output (they receive E2E-protected messages on broadcast CAN and need to check them). Only SC excluded.

**Principle:** Test your assumptions against the actual config, not the plan description. `None` include_in is a wildcard (all enabled), not "nothing enabled." This is actually correct behavior — RX-side E2E checking is needed even on "non-safety" ECUs.

---

## 2026-03-10 — Simplified AUTOSAR Needs Inline Struct Definitions

**Context:** E2E BSW module has no config struct types in E2E.h (just `E2E_Protect`/`E2E_Check` functions). The generated E2E_Cfg.c needed to define `E2E_PduProtectCfgType` and `E2E_ConfigType` itself.

**Insight:** Full AUTOSAR has hundreds of pre-defined config types. In simplified AUTOSAR, the BSW API is minimal. Generated config must define its own struct types when the BSW doesn't provide them.

**Principle:** When bridging generated config to a simplified BSW, define config structs in the generated file rather than modifying BSW headers. This keeps BSW stable and makes the generated config self-contained.

---

## 2026-03-10 — Safety Controller Needs cfg + e2e (Not Just canif)

**Context:** SC (TMS570 lockstep) had `include_in: ["canif"]`. But both CanIf_Cfg.c and E2E_Cfg.c templates `#include "<Prefix>_Cfg.h"`, which is generated by the `cfg` generator. Without `cfg` in include_in, CanIf config wouldn't compile. Additionally, SC is a safety monitor that must verify E2E (CRC/alive counters) on incoming messages per ISO 26262.

**Mistake:** Treated SC as "minimal = canif only" without checking template dependencies. Missed that `cfg` is a compile-time dependency for canif AND e2e, and that E2E verification is a safety requirement for the safety controller itself.

**Fix:** Changed SC to `include_in: ["cfg", "canif", "e2e"]`. SC now generates 3 files: Sc_Cfg.h (PDU defines), CanIf_Cfg_Sc.c (CAN routing), E2E_Cfg_Sc.c (E2E verification). No Com, RTE, PduR, or SWC — those are application-layer concerns SC doesn't need.

**Principle:** Generator dependencies form a DAG. When a template `#includes` another generated file, both generators must be in the ECU's include_in. Map template dependencies explicitly: cfg is a leaf dependency for canif, pdur, and e2e. Safety controllers that monitor CAN messages MUST have E2E — ISO 26262 requires end-to-end data integrity verification at the receiving end.

## 2026-03-10 — E2E Data ID Source: DBC vs Sidecar Mismatch

**Context:** E2E data IDs were stored in sidecar YAML using an ECU-prefixed 8-bit scheme (CVC=0x0X, FZC=0x1X, RZC=0x2X). The authoritative CAN message matrix used a flat 4-bit scheme (0x00-0x0F). These two sources contradicted each other, producing wrong E2E data IDs in generated code.

**Mistake:** Allowed E2E data IDs to live in a sidecar YAML file that was manually maintained, disconnected from the CAN message matrix. No automated validation checked that sidecar IDs matched the matrix. The OEM document chain (CAN Matrix -> DBC -> codegen) was broken.

**Fix:** Added `--e2e-source {dbc,sidecar}` CLI option. DBC mode reads `BA_ "E2E_DataID"` attributes added to the DBC file with values matching the CAN matrix exactly. Sidecar mode preserved for backward compatibility. Added 14 TDD tests covering both paths.

**Principle:** In an OEM-grade process, the CAN message matrix (SYS.3) is the single authoritative source. E2E data IDs must flow through the same toolchain as CAN routing: Matrix -> DBC -> codegen. Never maintain safety-critical IDs in a secondary file that requires manual synchronization. When multiple sources exist, make the conflict explicit with a CLI option and document the tradeoffs.

---

## 2026-03-13 — Hardcoded Com Signal IDs Break When Codegen Reorders Signals

**Context:** CVC heartbeat sent duplicate 0x010 [4] frames with all zeros alongside the correct 0x010 [8] heartbeat. Swc_Pedal.c called `Com_SendSignal(5u, ...)` and Swc_CvcCom.c called `Com_SendSignal(6u, ...)` / `Com_SendSignal(7u, ...)` — these IDs were correct in the dev repo's signal table but wrong in production.

**Mistake:** SWC code used hardcoded integer signal IDs (5, 6, 7, 13, 14, 17, 18, 19, 20, 21) migrated from the dev repo. Production codegen inserted heartbeat signals at positions 5-10, shifting all subsequent IDs. The hardcoded IDs now wrote torque/steer/brake data into heartbeat E2E fields (PDU 1), setting `com_tx_pending[1] = TRUE` and causing Com to transmit a corrupted heartbeat frame every 10ms.

**Fix:** Replaced all hardcoded IDs with generated `CVC_COM_SIG_*` defines from Cvc_Cfg.h. Also fixed Cvc_App.h aliases that cross-referenced `CVC_SIG_*` (RTE signal IDs) instead of `CVC_COM_SIG_*` (Com signal IDs) — two different namespaces.

**Principle:** Never hardcode Com signal IDs in SWC code. Always use the generated `<ECU>_COM_SIG_*` defines. Signal ordering is a codegen implementation detail that can change when messages/signals are added. RTE signal IDs (`CVC_SIG_*`) and Com signal IDs (`CVC_COM_SIG_*`) are separate namespaces — never mix them in aliases.
