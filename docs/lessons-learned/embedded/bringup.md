# Embedded / HIL Bring-up — Lessons Learned

## 2026-03-25 — HIL test runner static analysis pass

### Com TX cycle rate bug
**Context**: CVC heartbeat (0x010) and Vehicle_State (0x100) were both firing at 10ms
instead of their configured periods (50ms/100ms). Bus load was 29% (target <8%).
**Mistake**: `Com_MainFunction_Tx` had no per-PDU cycle counter. All PDUs with
`com_tx_pending == TRUE` fired every 10ms regardless of `CycleTimeMs`.
**Fix**: Added `static uint16 com_tx_cycle_cnt[COM_MAX_PDUS]`. Periodic PDUs gate
on `(cnt * 10ms) >= CycleTimeMs`. Event-triggered PDUs (CycleTimeMs==0) still fire
on `com_tx_pending`.
**Principle**: Test COM cycle rates with a CAN sniffer before declaring a build
working. A PDU broadcasting 10x too fast is not immediately obvious from logs.

### SC heartbeat confirmation latch (HIL vs production)
**Context**: SC reported CVC=TIMEOUT even though CVC was alive and beating.
**Mistake**: `SC_HB_TIMEOUT_TICKS = 10` (100ms) was calibrated for production
(coordinated power-on). On the HIL bench, individual flash-reset cycles mean the
SCM is already monitoring when an ECU restarts. A single missed heartbeat during
reflash caused `hb_confirmed` to latch permanently until power cycle.
**Fix**: Created `Sc_Cfg_Platform.h` with `PLATFORM_HIL` overrides: 500ms timeout,
100ms confirm window, 10s startup grace.
**Principle**: Safety controller timing constants must be tuned per deployment
environment. Always have a platform config override mechanism — don't hardcode.

### E2E CRC polynomial mismatch in HIL runner
**Context**: `can_helpers.py` used `_CRC8_POLY = 0x07` (CRC-8/ATM) and `init=0x00`,
`XOR-out=0x00`. Firmware uses SAE-J1850: poly=0x1D, init=0xFF, XOR-out=0xFF.
All E2E verdicts (HIL-040/041/042) would have permanently failed.
**Fix**: `_CRC8_POLY = 0x1D`, `crc8()` uses init=0xFF, XOR-out=0xFF.
**Principle**: When porting CRC from C to Python, verify ALL parameters: poly, init,
XOR-out, reflection. The polynomial alone is not sufficient to define the algorithm.

### DTC broadcast frame parser: wrong byte layout
**Context**: Runner's `_verdict_dtc_broadcast` read `code = msg.data[0] | (msg.data[1] << 8)`
(16-bit little-endian) and `source = msg.data[2]`. Firmware packs a 24-bit big-endian
DTC at bytes 0-2 and ECU source ID at byte 4.
**Fix**: `code = (msg.data[0] << 16) | (msg.data[1] << 8) | msg.data[2]`,
`source = msg.data[4]`.
**Principle**: Always read the C source frame packing before writing a Python parser.
`code = data[0] | data[1] << 8` is little-endian uint16 — not a 24-bit DTC.

### YAML key mismatches silently ignored (signal min, expected_state, timeout_ms)
**Context**: Several YAML scenario files used `min:` instead of `min_value:`,
`expected_state:` instead of `expected:`, `timeout_ms:` instead of `within_ms:`.
Python `dict.get("min_value", 0)` returns 0 when key is absent — test passes vacuously.
**Fix**: Made runner accept both forms as aliases. Also fixed YAML files to use
canonical keys. `hil_032` had wrong DTC code (0xE202 vs 0xD101 from firmware).
**Principle**: YAML-driven test runners must validate required keys are present and
warn loudly when unknown keys are found. Silent fallback to defaults masks authoring
errors indefinitely.

### MQTT payload double-encoding
**Context**: YAML authors wrote `payload: '{"fault":"overcurrent","current_ma":28000}'`
(a JSON string). Runner passed this string to `inject_mqtt_fault` which called
`json.dumps(payload)` — encoding the string again: `'"{\\"fault\\":\\"overcurrent\\"}"'`.
Plant-sim received a JSON string, not a JSON object, and silently ignored the fault.
**Fix**: Runner parses string payloads via `json.loads()` before passing to
`inject_mqtt_fault`.
**Principle**: When a YAML value is intended to be structured data, use YAML dict
syntax, not a JSON string. When accepting either, validate and normalize early.

### MQTT API key mismatch: "fault" vs "type"
**Context**: All 5 fault injection YAMLs sent `{"fault":"overcurrent"}` etc. Plant-sim
`simulator.py` reads `cmd.get("type","")` — key "fault" is silently ignored. Every
fault injection test would have sent the command and received no response. Also:
`reset_mqtt_faults()` in `can_helpers.py` sent `{"fault":"reset"}` — resets never executed.
Additionally, hil_033 used `voltage_mv`/`soc_pct` field names; plant-sim reads `mV`/`soc`.
And hil_034 used type `"overtemp"` instead of the correct handler name `"inject_temp"`.
**Fix**: All payloads aligned to `"type"` key. Field names verified against
`gateway/fault_inject/plant_inject.py` (authoritative API surface).
**Principle**: Before writing test YAML files, read the fault injection handler source —
not just the topic name. The canonical field names are in the handler, not in any doc.
`plant_inject.py` is the single source of truth for plant-sim MQTT API shape.

### DTC code mismatch: firmware DEM path vs. plant-sim direct generation
**Context**: hil_032 was "fixed" to use `dtc_code: 0xD101` (FZC DEM `FZC_DTC_BRAKE_FAULT`)
based on reading `firmware/fzc/src/main.c`. But plant-sim generates `DTC_BRAKE_FAULT =
0x00E202` directly in `_send_dtc()`, bypassing the FZC DEM path (which is not yet wired
for brake fault in firmware). So the correct expected DTC on the bus is 0xE202.
**Fix**: Reverted hil_032 to `dtc_code: 0xE202`.
**Principle**: For fault injection DTCs, trace both paths: firmware DEM and plant-sim
direct generation. Where the FZC/RZC DEM path is not yet wired, plant-sim generates
the DTC itself. Check `simulator.py` `_check_and_send_dtcs()` comments.

## 2026-03-27 — MISRA C:2012 Required violations in CanSM and Xcp

### Rule 10.7: compound assignment with wider essential type (CanSM.c)
**Context**: `cansm_recovery_timer` (uint16) was updated via compound assignment
`cansm_recovery_timer += CANSM_MAIN_PERIOD_MS` where `CANSM_MAIN_PERIOD_MS = 10u`
(unsigned int = uint32 on ARM Cortex-M). MISRA C:2012 Rule 10.7 (Required) states
that in a compound assignment `a += b`, if `b` has a wider essential type than `a`,
the LHS is effectively treated as a composite expression — violating the rule.
**Fix**: Changed to explicit cast form:
`cansm_recovery_timer = (uint16)(cansm_recovery_timer + CANSM_MAIN_PERIOD_MS);`
This separates the arithmetic (uint16 + uint32 → uint32) from the narrowing assignment
(explicit cast to uint16), satisfying both Rule 10.7 and Rule 10.3.
**Principle**: Never use `uint16 += uint32` or `uint8 += uint32` compound assignments.
Use `x = (uint16)(x + y)` with explicit cast. The compound form is a MISRA Rule 10.7
violation even though the arithmetic and truncation are intentional and safe.

### Rule 10.1: boolean flag declared as uint8 compared to TRUE (Xcp.c)
**Context**: `xcp_seed_pending` was declared as `static uint8 xcp_seed_pending = FALSE`
but then assigned `TRUE`/`FALSE` and compared with `if (xcp_seed_pending != TRUE)`.
MISRA C:2012 Rule 10.1 (Required) forbids operands of an inappropriate essential type
for an operator. The `!=` operator requires both operands in the same essential type
category; `uint8` (essentially unsigned) mixed with `TRUE` (essentially Boolean) is
inappropriate.
**Fix**: Changed declaration to `static boolean xcp_seed_pending = FALSE`. All
assignments (= TRUE, = FALSE) and comparisons (!= TRUE) are then type-consistent.
**Principle**: Boolean flags must be declared as `boolean`, not `uint8` or `int`.
Using `uint8` for a flag that is only ever TRUE/FALSE is not a MISRA-safe substitute —
the essential type categories differ and comparisons against `TRUE`/`FALSE` will
violate Rule 10.1. Declare the type that matches the intent.
**Also note**: Previously CanSM.c had a local `extern` declaration of
`Can_SetControllerMode` with `CAN_CS_STARTED = 0u` — wrong value vs Can.h (`= 2u`).
Fixed by replacing inline extern with `#include "Can.h"`. Always include the
authoritative header; inline externs in .c files drift silently.

## 2026-03-27 — MISRA approach: inline suppressions vs file-level, and wrong rule citations

### Wrong MISRA rule cited in CanSM.c comment
**Context**: The comment on the `cansm_recovery_timer` compound assignment read
"explicit cast satisfies MISRA Rule 10.7 (uint16 + uint32 composite)". Rule 10.7
covers the widening of a composite expression's *other* operand — it was the previous
session's fix note (see lesson above). The current explicit `(uint16)` cast addresses
Rule 10.3 (narrowing assignment), not 10.7.
**Fix**: Updated comment to correctly cite Rule 10.3 and note that the global 10.3/10.7
suppression in `tools/misra/suppressions.txt` covers both concerns.
**Principle**: MISRA comment annotations that cite a rule number are evidence for
auditors — a wrong rule number misleads reviewers into thinking the wrong analysis was
done. When borrowing a code pattern from a prior fix, re-derive which rule(s) apply
rather than copying the comment verbatim.

### File-level suppression too broad for documented line-specific deviation
**Context**: DEV-003 in `misra-deviation-register.md` documents 3 specific lines in
`Xcp.c` where Rule 11.4 (pointer-to-integer) applies. The suppression file carried a
blanket `misra-c2012-11.4:*/Xcp.c` entry suppressing the rule for the entire file.
This means any new Rule 11.4 violation added to Xcp.c would be silently suppressed
without a deviation review.
**Fix**: Added `// cppcheck-suppress misra-c2012-11.4` (single-line `//` form) on the
line immediately before each of the 3 cast sites, with a `DEV-003` reference. The
file-level entry is retained temporarily as a fallback with a comment explaining why,
pending confirmation that `--inline-suppr` reliably overrides the rule on all CI
machines. Once confirmed, the file-level entry should be removed.
**Principle**: When a deviation is documented at specific source locations, the
suppression should be at the same granularity. File-level suppressions for line-specific
deviations mask future violations. Always use inline suppressions (with deviation reference)
when the deviating code is isolated to a few known locations. The `// cppcheck-suppress`
(not `/* */` block form) is preferred because cppcheck definitively applies it to the
immediately following line.

### Globally suppressed Required rules without deviation register entries
**Context**: Rules 10.3 and 10.4 were globally suppressed in `suppressions.txt` but
had no entries in `misra-deviation-register.md`. For an ASIL-D project, any Required
rule suppression must have a formal deviation. Auditors reviewing the register would see
no coverage for these rules.
**Fix**: Added DEV-004 (Rule 10.3) and DEV-005 (Rule 10.4) to the deviation register
with full justification (byte-packing patterns), risk assessment, and compensating
measures. Also restructured `suppressions.txt` with section headers distinguishing
Advisory vs Required rules and noting which DEV entry covers each Required suppression.
**Principle**: `suppressions.txt` and `misra-deviation-register.md` are a pair — every
Required-rule suppression must have a corresponding DEV entry. Review them together
after every MISRA triage pass. Advisory-rule global suppressions still need a comment
explaining the rationale even if no formal deviation is required.
