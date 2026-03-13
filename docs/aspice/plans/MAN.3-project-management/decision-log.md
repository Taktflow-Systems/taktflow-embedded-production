---
document_id: MAN3-DECISION-LOG
title: "Architecture Decision Log"
version: "1.0"
status: active
updated: "2026-03-10"
---

## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.


# Architecture Decision Log

All project decisions must be recorded here with the mandatory fields below.

## Decision Format (Mandatory)

Every decision MUST include:

1. **Tier**: T1-T4 classification (see [decision-records rule](../../../../.claude/rules/decision-records.md))
2. **Scores**: Cost, Time, Safety, Resume (1-3 each, total /12)
3. **Rationale**: WHY it was chosen
4. **Effort**: cost ($) and time (hours/days) for the chosen option
5. **Alternative A**: description, effort ($ + time), pros, cons
6. **Alternative B**: description, effort ($ + time), pros, cons
7. **Why chosen wins**: explicit comparison against alternatives

## Compliance Markers

Decisions in ANY document can use this inline marker for scan detection:

```markdown
<!-- DECISION: ADR-NNN — {short title} -->
```

The daily scan script (`scripts/decision-audit.sh`) searches for:
- All `<!-- DECISION:` markers in `docs/`
- All entries in this file
- Any untagged decisions (heuristic grep for "chose", "decided", "use X over Y")

## Quick Reference

| ADR | Decision | Tier | Score | Date | Status |
|-----|----------|------|-------|------|--------|
| ADR-001 | Structure docs under ASPICE process areas | T4 | 5/12 | 2026-02-21 | Approved |
| ADR-002 | Keep master-plan.md as source baseline | T4 | 4/12 | 2026-02-21 | Approved |
| ADR-003 | Create central docs/research/ repository | T4 | 4/12 | 2026-02-21 | Approved |
| ADR-004 | Add MAN.3 live tracking set | T4 | 5/12 | 2026-02-21 | Approved |
| ADR-005 | STM32G474RE Nucleo for 3 zone ECUs | T1 | 10/12 | 2026-02-21 | Approved |
| ADR-006 | AUTOSAR Classic layered BSW | T1 | 9/12 | 2026-02-21 | Approved |
| ADR-007 | POSIX SocketCAN for simulated ECU MCAL | T2 | 7/12 | 2026-02-21 | Approved |
| ADR-008 | BMW vsomeip for SOME/IP demo | T2 | 6/12 | 2026-02-21 | Approved |
| ADR-009 | Docker containers for simulated ECU runtime | T2 | 5/12 | 2026-02-21 | Approved |
| ADR-010 | Unity + CCS Test + pytest for testing | T2 | 7/12 | 2026-02-21 | Approved |
| ADR-011 | CAN 2.0B at 500 kbps (no CAN FD) | T1 | 7/12 | 2026-02-21 | Approved |
| ADR-012 | AWS IoT Core + Timestream + Grafana | T2 | 6/12 | 2026-02-21 | Approved |
| ADR-013 | Custom trace-gen.py for traceability | T2 | 8/12 | 2026-02-25 | Approved |
| ADR-014 | Platform abstraction via link-time swap + config split | T2 | 9/12 | 2026-03-10 | Approved |

---

## ADR-001: Structure docs under ASPICE process areas

- **Date**: 2026-02-21
- **Tier**: T4 — Process
- **Scores**: Cost 1 | Time 1 | Safety 1 | Resume 2 = **5/12**
- **Decision**: Organize documentation under `docs/aspice/` by process area (SYS, SWE, SUP, MAN, HWE)
- **Rationale**: Assessors navigate by process area. Flat folder requires mental mapping.
- **Effort**: 2 hours to restructure, $0

### Alternative A: Keep flat docs/plans/ structure
- **Effort**: 0 hours (no change), $0
- **Pros**: Simple, already exists
- **Cons**: Assessor must search for artifacts. No clear process-to-folder mapping. Doesn't scale past 20 docs.

### Alternative B: Tool-based ALM (Jira + Confluence or Polarion)
- **Effort**: 8-16 hours setup, $0-50/month (cloud licenses)
- **Pros**: Professional ALM, built-in traceability, dashboards
- **Cons**: Overkill for portfolio project. Vendor lock-in. Not version-controlled with code. Costs money.

### Why chosen wins
File-based ALM in Git gives assessor-friendly structure at zero cost with full version control. Tool-based ALM is overkill for a 1-person portfolio project. Flat structure doesn't scale.

---

## ADR-002: Keep master-plan.md as source baseline

- **Date**: 2026-02-21
- **Tier**: T4 — Process
- **Scores**: Cost 1 | Time 1 | Safety 1 | Resume 1 = **4/12**
- **Decision**: Keep `docs/plans/master-plan.md` as the single strategic plan, with ASPICE execution plans as operational breakdowns
- **Rationale**: One source of truth for scope, phases, and architecture. ASPICE plans reference it.
- **Effort**: 0 hours (exists), $0

### Alternative A: Split master plan into per-phase documents
- **Effort**: 4 hours to split, $0
- **Pros**: Smaller files, easier to review per phase
- **Cons**: Loses holistic view. Cross-phase references break. Hard to maintain consistency.

### Alternative B: Delete master plan, use only ASPICE execution plans
- **Effort**: 2 hours to migrate content, $0
- **Pros**: Single source (no duplication)
- **Cons**: ASPICE plans are operational, not strategic. Loses architecture overview, BOM, and demo scenarios context.

### Why chosen wins
Master plan provides strategic context (architecture, BOM, demos) that execution plans don't cover. Duplication risk is managed by execution plans referencing master plan, not copying content.

---

## ADR-003: Create central docs/research/ repository

- **Date**: 2026-02-21
- **Tier**: T4 — Process
- **Scores**: Cost 1 | Time 1 | Safety 1 | Resume 1 = **4/12**
- **Decision**: Centralize all external references and research notes in `docs/research/`
- **Rationale**: Scattered references across plans and notes lose provenance. Central log enables traceability.
- **Effort**: 1 hour setup, $0

### Alternative A: Inline references in each document
- **Effort**: 0 hours (default behavior), $0
- **Pros**: References live with context
- **Cons**: No central index. Duplicate research. Hard to audit which sources influenced which decisions.

### Alternative B: External tool (Zotero, Notion, bookmark manager)
- **Effort**: 2 hours setup, $0
- **Pros**: Better search, tagging, PDF storage
- **Cons**: Not in Git. Not version-controlled. Another tool to maintain.

### Why chosen wins
Git-versioned research log keeps provenance with the codebase. Link-log.md is greppable and auditable. External tools add friction for a solo project.

---

## ADR-004: Add MAN.3 live tracking set (dashboard/logs/gates)

- **Date**: 2026-02-21
- **Tier**: T4 — Process
- **Scores**: Cost 1 | Time 1 | Safety 1 | Resume 2 = **5/12**
- **Decision**: Add progress dashboard, issue log, decision log, and gate readiness checklist to MAN.3
- **Rationale**: ASPICE MAN.3 requires progress monitoring, issue tracking, and gate management.
- **Effort**: 3 hours to create templates, $0

### Alternative A: Use only checklist checkboxes in master plan
- **Effort**: 0 hours (exists), $0
- **Pros**: Simple, no new files
- **Cons**: No structured tracking. Can't query issues or decisions. Checkboxes don't show velocity or trends.

### Alternative B: Use GitHub Issues + Projects board
- **Effort**: 4 hours setup, $0
- **Pros**: Built-in kanban, issue tracking, milestones
- **Cons**: Not in repo (requires GitHub access). Harder to baseline. Assessor may not have GitHub access.

### Why chosen wins
Markdown tracking files in-repo satisfy ASPICE evidence requirements, are self-contained, and baseline with the code. GitHub Issues is a good complement but not sufficient as primary evidence.

---

## ADR-005: STM32G474RE Nucleo-64 for 3 zone ECUs

- **Date**: 2026-02-21
- **Tier**: T1 — Architecture
- **Scores**: Cost 2 | Time 2 | Safety 3 | Resume 3 = **10/12**
- **Decision**: Use 3x STM32G474RE Nucleo-64 boards for CVC, FZC, and RZC zone controllers
- **Rationale**: The G474RE offers 3x FDCAN (CAN-FD capable, backward-compatible with CAN 2.0B), 5x 12-bit ADCs (42 channels for dual-sensor plausibility), CORDIC + FMAC math accelerators (motor control), 170 MHz Cortex-M4F, and 128 KB RAM — all critical for ASIL D automotive BSW. STM32 is the dominant MCU family in automotive Tier 1 development, maximizing resume impact. Free toolchain (STM32CubeIDE + CubeMX) with onboard ST-LINK/V3 debugger.
- **Effort**: ~$60 for 3 boards ($20 each), 10-14 days BSW development, $0 toolchain

### Alternative A: STM32F446RE Nucleo-64
- **Effort**: ~$33-45 for 3 boards ($11-15 each), 8-12 days BSW development, $0 toolchain
- **Pros**: Lower cost (saves ~$25 total). Slightly higher DMIPS (225 vs 213). Massive community. Simpler bxCAN driver (less config than FDCAN). Same STM32CubeIDE toolchain.
- **Cons**: NO CAN-FD — only CAN 2.0 via bxCAN. Only 2 CAN controllers (CAN2 depends on CAN1). Only 3 ADCs vs 5. No CORDIC/FMAC math accelerators. Older F4 family (2012) vs modern G4 (2019). ST-LINK/V2-1 vs V3. Missing CAN-FD signals use of legacy technology to automotive interviewers.

### Alternative B: Arduino Mega 2560
- **Effort**: ~$150 for 3 boards ($50 each official, $18 clone) + $15-45 for 3x MCP2515 CAN modules + $15-50 debug adapter, 18-25+ days BSW development, $0 toolchain (Arduino IDE)
- **Pros**: Widely recognized brand. Very easy to get started. 4 hardware UARTs. 5V logic (simpler sensor interfacing). Clones are cheap ($18).
- **Cons**: 8-bit AVR at 16 MHz — fundamentally wrong for ASIL D (Microchip rates 8-bit at ASIL B max). No built-in CAN (requires external MCP2515 over SPI). Only 8 KB SRAM (catastrophically low for BSW buffers). No FPU (software float 10-100x slower). No DMA. 10-bit ADC vs 12-bit. No onboard debugger. Arduino IDE not MISRA-compatible. No AUTOSAR tooling supports AVR. Resume damage — signals hobbyist work, not automotive engineering.

### Why chosen wins
The STM32G474RE costs $15-25 more total than the F446RE but gains CAN-FD (the current automotive standard), 2 extra ADCs for dual-sensor plausibility, and hardware math accelerators — all directly relevant to ASIL D portfolio credibility. The Arduino Mega is disqualified on technical grounds: 8-bit/8KB RAM/no CAN/no FPU makes AUTOSAR-like BSW impractical, and listing it on an ASIL D project would damage resume credibility. The G474RE hits the sweet spot of modern automotive peripherals at hobby-budget pricing.

---

## ADR-006: AUTOSAR Classic layered BSW architecture

- **Date**: 2026-02-21
- **Tier**: T1 — Architecture
- **Scores**: Cost 1 | Time 2 | Safety 3 | Resume 3 = **9/12**
- **Decision**: Implement a custom AUTOSAR Classic-inspired layered BSW with MCAL, EAL (CanIf, PduR, IoHwAb), Services (Com, Dcm, Dem, WdgM, BswM, E2E), and RTE — approximately 3,000 LOC shared across 3 STM32 ECUs
- **Rationale**: AUTOSAR BSW module names (CanIf, PduR, Com, Dcm, Dem, WdgM, BswM, RTE) are the most searched keywords in automotive embedded job postings. The layered architecture directly satisfies ASPICE SWE.2 (software architecture) and SWE.3 (detailed design). The MCAL abstraction enables SIL testing by swapping STM32 MCAL for POSIX SocketCAN MCAL — same BSW code on hardware and in Docker. No licensing cost ($0) versus commercial AUTOSAR stacks ($10K-$100K+).
- **Effort**: 10-14 days for single developer (~3,000 LOC across 16 modules), $0

### Alternative A: Bare-metal with custom HAL (no AUTOSAR structure)
- **Effort**: 6-8 days for 3 ECUs (~2,500-3,500 LOC duplicated per ECU), $0
- **Pros**: Fastest to implement (saves 4-6 days). Simplest code. Smallest binary size. Shows low-level embedded skills.
- **Cons**: ASPICE SWE.2 non-compliant — flat code with no defined software architecture rates Capability Level 0-1. No AUTOSAR keywords on resume (indistinguishable from hobby project). No hardware abstraction = no SIL testing portability. Code duplication across 3 ECUs. Ad-hoc traceability. No standardized diagnostic stack. Automotive resume impact: 3/10.

### Alternative B: FreeRTOS + STM32Cube HAL (without AUTOSAR layering)
- **Effort**: 8-10 days for 3 ECUs (~3,000-4,000 LOC), $0 (FreeRTOS MIT license; SafeRTOS certified version costs $5K-$15K)
- **Pros**: RTOS is a valuable resume keyword. Task-based architecture demonstrates concurrency skills. FreeRTOS integrated into STM32CubeMX (one checkbox). Priority-based preemptive scheduling. FreeRTOS-aware debugging in STM32CubeIDE. Migratability story to SafeRTOS for ASIL D.
- **Cons**: No AUTOSAR keywords — FreeRTOS is not AUTOSAR. No standardized communication stack (ad-hoc CAN handling). No Dcm/Dem for diagnostics. ASPICE SWE.2 partial compliance only (L1-L2). No RTE concept = no clean separation between application and infrastructure. FreeRTOS not safety-certified (needs qualification argument). Task priority tuning adds complexity. Automotive resume impact: 5/10 vs 10/10 for AUTOSAR.

### Why chosen wins
The AUTOSAR-like BSW costs 2-6 extra days over alternatives but delivers 16+ automotive resume keywords, ASPICE SWE.2/SWE.3 compliance, hardware abstraction for SIL testing, and a code structure immediately recognizable to any OEM or Tier 1 interviewer (10/10 resume impact). Bare-metal saves time but produces a project indistinguishable from a university capstone. FreeRTOS adds RTOS skills but misses the AUTOSAR architecture story that is the project's core differentiator. The 10-14 day investment is the highest-ROI work in the entire project.

---

## ADR-007: POSIX SocketCAN for simulated ECU MCAL

- **Date**: 2026-02-21
- **Tier**: T2 — Design
- **Scores**: Cost 1 | Time 2 | Safety 1 | Resume 3 = **7/12**
- **Decision**: Use POSIX SocketCAN API as the MCAL layer for 3 simulated ECUs (BCM, ICU, TCU) running in Docker containers, enabling the same C firmware codebase to compile for both STM32 (hardware MCAL) and Linux (SocketCAN MCAL)
- **Rationale**: 100% code reuse between physical and simulated ECUs. The same BSW stack runs on target hardware and in Docker, swapping only the MCAL layer. This demonstrates the AUTOSAR portability principle and enables CI/CD testing without hardware. Virtual CAN interfaces (vcan/vxcan) provide zero-cost inter-container CAN communication.
- **Effort**: 5-8 days for MCAL abstraction + SocketCAN implementation, $0

### Alternative A: Python-can simulation (Python scripts, not C)
- **Effort**: 3-5 days (faster due to Python simplicity), $0 (python-can is LGPL-3.0, free)
- **Pros**: Fastest development time — CAN send/receive in 3-5 lines of Python. Excellent for rapid prototyping. Built-in log replay and format conversion (ASC, BLF). Rich ecosystem (cantools for DBC, python-can-isotp). Lower barrier to entry.
- **Cons**: Zero code reuse with physical ECU firmware (Python vs C). Cannot demonstrate MCAL abstraction architecture — destroys the core portfolio story. Not representative of real automotive ECU software (no OEM writes production ECU code in Python). Drops messages above ~1000 msg/sec on Pi. Python GIL adds latency.

### Alternative B: Vector CANoe/CANalyzer simulation
- **Effort**: 10-15 days + 1-2 weeks procurement, ~$10,000-$15,000 per seat license + ~$850-$1,900 for VN16xx hardware interface
- **Pros**: Industry-standard tool — every automotive recruiter recognizes "CANoe experience." Professional-grade bus simulation with full timing control. Built-in test automation and conformance testing. Real UDS diagnostic simulation. Signal-level simulation with DBC/ARXML.
- **Cons**: Prohibitively expensive for a portfolio project ($10K-$15K+). No code reuse with embedded C firmware (CAPL is Vector-proprietary). Windows-only — incompatible with Docker Linux simulation. Requires Vector hardware interface. Overkill for the project scope. CANoe simulates "rest of bus," not the ECU itself.

### Why chosen wins
The SocketCAN MCAL approach is the only option that achieves 100% code reuse between physical and simulated ECUs — the defining architectural feature. Python-can is cheaper and faster but destroys the AUTOSAR portability demonstration. Vector CANoe is the wrong class of tool (test environment, not ECU code) at a prohibitive cost. The 5-8 day investment produces a CI-testable, Docker-deployable vECU architecture that mirrors real Tier 1 vECU development practices at $0 cost.

---

## ADR-008: BMW vsomeip for SOME/IP demo

- **Date**: 2026-02-21
- **Tier**: T2 — Design
- **Scores**: Cost 1 | Time 1 | Safety 1 | Resume 3 = **6/12**
- **Decision**: Use vsomeip (COVESA/BMW open-source SOME/IP implementation) for service-oriented communication between simulated ECUs and Pi gateway
- **Rationale**: vsomeip is the industry-standard open-source SOME/IP stack, deployed in millions of BMW production vehicles. 1,400 GitHub stars, 55+ contributors, actively maintained with monthly public meetings. A working hello-world client-server pair takes ~300-400 lines. Direct API usage exposes SOME/IP protocol concepts (service discovery, request/response, publish/subscribe) — exactly what interviewers want to see.
- **Effort**: 4-7 days for 2-3 service interfaces + Docker integration, $0 (MPL-2.0 license)

### Alternative A: Custom SOME/IP implementation from scratch
- **Effort**: 4-8 weeks for a portfolio-quality subset (header parsing, service discovery, events), $0
- **Pros**: Complete understanding of protocol internals — impressive if completed. Zero external dependencies. Can tailor exactly to project needs.
- **Cons**: Massive time investment for marginal portfolio value — interviewers care that you used SOME/IP, not that you reimplemented it. 400-450+ pages of AUTOSAR specifications to implement against. Will be buggy and non-conformant. No interoperability guarantee. Reinventing the wheel when a production-grade OEM-backed solution exists. Signals unfamiliarity with industry practice (no OEM implements SOME/IP from scratch).

### Alternative B: CommonAPI C++ with vsomeip binding (higher-level abstraction)
- **Effort**: 8-14 days (6 repositories to build + Franca IDL learning + code generators), $0 (MPL-2.0) + Java 8 runtime for generators
- **Pros**: Auto-generated proxy/stub code reduces boilerplate. Transport-agnostic interfaces (D-Bus or SOME/IP binding). Franca IDL used in production at BMW and other OEMs. Type-safe generated interfaces with automatic serialization.
- **Cons**: 6 repositories to version-match and build (fragile toolchain). Small community (111 stars vs 1,400 for vsomeip). Known compilation failures in Release 3.2. Franca IDL is an extra DSL with limited documentation. Code generators are Java-based black boxes. Abstraction hides the protocol details you want to demonstrate. Doubles development effort for no additional portfolio value.

### Why chosen wins
vsomeip direct API gives the best ROI: 4-7 days produces a working SOME/IP demo with BMW/COVESA credibility, full protocol visibility, and a single well-maintained dependency. Custom implementation wastes 4-8 weeks reimplementing what already exists. CommonAPI doubles the effort and hides protocol details behind generated code, defeating the purpose of demonstrating SOME/IP understanding. The SOME/IP testing tools market is $512M and growing at 13.2% CAGR — this is a high-value resume skill.

---

## ADR-009: Docker containers for simulated ECU runtime

- **Date**: 2026-02-21
- **Tier**: T2 — Design
- **Scores**: Cost 1 | Time 1 | Safety 1 | Resume 2 = **5/12**
- **Decision**: Run 3 simulated ECUs (BCM, ICU, TCU) in Docker containers with virtual CAN (vcan/vxcan) interfaces on Linux
- **Rationale**: Docker provides strong isolation (namespaces, cgroups), reproducible environments (Dockerfile), and excellent CI/CD integration. The same C firmware source compiles for x86 host with SocketCAN MCAL. Docker-based vECU is an emerging industry pattern (SYS TEC, Collabora, dSPACE all document this approach). Containers can be started/stopped/reset with docker-compose.
- **Effort**: 1-2 days for Dockerfile + docker-compose + vcan setup, $0

### Alternative A: QEMU/Renode system emulation (ARM Cortex-M4)
- **Effort**: 8-15 days (QEMU) or 3-5 days (Renode) for custom board + peripheral models, $0
- **Pros**: Runs the actual ARM binary — no recompilation needed. Catches ARM-specific bugs (endianness, alignment). Closer to hardware behavior (register-level emulation). Renode used by Google/NASA. Can test interrupt handlers and DMA. Industry-recognized approach for vECU.
- **Cons**: STM32G474RE not directly supported (no upstream QEMU board definition). FDCAN peripheral not emulated in upstream QEMU (requires 3-5 days of custom C modeling). Not cycle-accurate — cannot do real WCET analysis. 8-15 days upfront (QEMU) or 3-5 days (Renode) vs 1-2 days for Docker. Slower iteration than native compilation. Peripheral models may diverge from real silicon behavior.

### Alternative B: Native Linux processes (no containerization)
- **Effort**: 1-2 days setup, $0
- **Pros**: Fastest iteration — compile and run immediately with no container build step. Zero overhead. Simplest debugging (GDB, Valgrind, AddressSanitizer directly). No Docker Desktop dependency (saves 2-3 GB RAM on Windows/WSL2). Lowest maintenance burden.
- **Cons**: No isolation between ECU processes (shared filesystem, shared network namespace). Environment not reproducible — "works on my machine" risk. No resource limiting (runaway process can consume all host resources). Less impressive on resume. Harder to reset to clean state. Not a recognized vECU approach in industry. Cannot demonstrate containerized deployment skills.

### Why chosen wins
Docker costs the same 1-2 days as native processes but adds isolation, reproducibility, and CI/CD portability — the exact properties needed for a credible vECU demonstration. Native processes are fragile and unimpressive. QEMU/Renode is technically interesting but costs 3-15x more setup time to build custom peripheral models for an MCU not supported upstream, and still does not provide cycle-accurate timing. Docker is the industry trend for SIL/vECU testing and adds "Docker, docker-compose, container orchestration" to the resume at minimal cost.

---

## ADR-010: Unity + CCS Test + pytest for testing

- **Date**: 2026-02-21
- **Tier**: T2 — Design
- **Scores**: Cost 1 | Time 1 | Safety 3 | Resume 2 = **7/12**
- **Decision**: Use Unity (ThrowTheSwitch) for C unit testing on STM32 firmware, CCS (Code Composer Studio) built-in testing for TMS570 Safety Controller, and pytest for Python gateway/cloud testing
- **Rationale**: Unity is pure ANSI C (runs on 8-bit MCUs to 64-bit hosts), MIT-licensed, 5,040 GitHub stars, actively maintained (336/365 days active). CMock auto-generates C mocks from headers. The entire framework is 1 C file + 2 headers (~1,500 lines) — trivial to include in any build system. CCS Test is the only practical option for TMS570 on-target testing. pytest is the Python standard.
- **Effort**: 2-4 hours Unity setup + 1-2 days CCS test harness + 1-2 hours pytest setup, $0

### Alternative A: Google Test (gtest) + CMock + pytest
- **Effort**: 2-4 days setup (C/C++ boundary boilerplate, extern "C" wrappers, CMake FetchContent), $0 (BSD-3-Clause)
- **Pros**: Massive community (35,000 GitHub stars). Rich assertion library with parameterized tests, test fixtures, death tests. Google Mock (gmock) provides sophisticated mocking. Native JUnit XML output for CI. Excellent IDE support (CLion, VS Code). Used by Chromium, LLVM, OpenCV. Strong general software engineering resume value.
- **Cons**: Requires C++17 compiler — test harness is C++ calling into C code, creating extern "C" boilerplate. gmock designed for C++ classes, not C functions (mocking C requires workarounds). Cannot run on TMS570 target (needs C++17 runtime). Larger test binary (~2-5 MB vs Unity's ~50 KB). Overkill for pure C firmware testing. No ISO 26262 tool qualification (same as Unity).

### Alternative B: CUnit + custom TMS570 test harness + pytest
- **Effort**: 4-7 days (CUnit setup + custom TMS570 harness from scratch + manual mock writing), $0 (LGPL-2.0)
- **Pros**: Pure C — same toolchain as production code. Small footprint. Straightforward assertion API. Long history (since 2001).
- **Cons**: Effectively abandoned — last release February 2018, maintainers unreachable. No mock generation (all stubs manual). Limited assertion types. No test discovery (manual registration for every test). XML output is not JUnit format (needs custom parser for CI). LGPL-2.0 more restrictive than MIT. No active community. An abandoned tool weakens the ISO 26262 Part 8 tool qualification argument ("who validates a dead tool?").

### Why chosen wins
Unity takes 2-4 hours to set up (include unity.c in your build) vs 2-4 days for gtest or 4-7 days for CUnit. It is pure C (no C++17 compiler needed), actively maintained, MIT-licensed, and designed specifically for embedded testing. CMock auto-generates mocks — CUnit has no mock generation at all. gtest is excellent for C++ projects but the C/C++ boundary overhead is unnecessary when the entire codebase is C. CUnit is abandoned and choosing it would be questioned by any ASPICE assessor reviewing tool qualification evidence. Unity + CMock is the embedded industry standard for C unit testing.

---

## ADR-011: CAN 2.0B at 500 kbps (no CAN FD)

- **Date**: 2026-02-21
- **Tier**: T1 — Architecture
- **Scores**: Cost 1 | Time 1 | Safety 3 | Resume 2 = **7/12**
- **Decision**: Use CAN 2.0B at 500 kbps for all inter-ECU communication. STM32G474RE FDCAN runs in classic mode; TMS570LC43x DCAN operates natively at 500 kbps. All nodes are bus-compatible.
- **Rationale**: The TMS570LC43x Safety Controller only has DCAN (classic CAN). CAN-FD frames cause error frames on classic CAN controllers — they cannot coexist on the same bus segment. Since the Safety Controller must monitor all traffic in silent mode, the entire bus must use classic CAN. The project's 16 CAN messages at 10-50 Hz with 8-byte payloads = ~12.8 kB/s, well under CAN 2.0B's ~36 kB/s capacity (~35% utilization). No bandwidth pressure exists.
- **Effort**: $0 (no additional hardware), 0 additional days (native mode for both MCUs)

### Alternative A: CAN FD at 500k/2M via external TCAN4550 on TMS570
- **Effort**: +$3-5 per unit production ($149 EVM + $15 breakout for prototyping), 8-13 days (SPI driver 2-3d, FD framing 1-2d, STM32 FDCAN switch 1-2d, bus testing 2-3d, safety analysis update 2-3d)
- **Pros**: 8x payload per frame (64 vs 8 bytes). Lower latency for large transfers (UDS, OTA). Better CRC (17/21-bit vs 15-bit). Industry direction (CAN FD adoption accelerating in 2025+ platforms). Better portfolio keyword. More bandwidth for authentication overhead.
- **Cons**: Adds SPI-to-CAN path on the Safety Controller — increases complexity on the most safety-critical node (~400 LOC becomes ~700+). CAN FD nodes cannot coexist with classic CAN nodes on the same bus segment without a gateway. Bus length drops to ~40m at 2 Mbps data rate (fine for benchtop). 8-13 extra days of development. $165+ prototyping cost. Not needed — bus utilization is only 35%.

### Alternative B: Ethernet + CAN hybrid (100BASE-T1 backbone + CAN safety bus)
- **Effort**: +$63-$438 hardware (STM32G474RE has NO Ethernet MAC — must replace 3 boards with STM32H743ZI at $33 each + 100BASE-T1 PHYs + eval boards), 29-48 days (MCU re-porting 5-10d, Ethernet driver 3-5d, SOME/IP or DoIP 5-8d, dual-network architecture 2-3d, gateway 3-5d, safety analysis 3-5d, integration 5-7d)
- **Pros**: 200x bandwidth (100 Mbps vs 0.5 Mbps). Modern zonal architecture (mirrors real 2023+ vehicle platforms). Very impressive on resume. Supports DoIP (diagnostics over IP). Scalable to 1000BASE-T1. Industry trend — Ethernet backbone + CAN for safety is the dominant new-platform architecture.
- **Cons**: Enormous development effort (6-10 additional weeks). STM32G474RE has no Ethernet MAC — requires replacing all 3 zone ECU boards and re-porting all firmware. Complete HAL, clock tree, and peripheral migration. Safety qualification harder (Ethernet not inherently deterministic — needs TSN). Massive overkill for 16 CAN messages. Pushes hardware budget to $1,040-$1,415. Highest finishability risk of any decision.

### Why chosen wins
CAN 2.0B at 500 kbps is the zero-cost, zero-risk baseline that satisfies all project requirements: 35% bus utilization leaves ample headroom, both MCU families support it natively, and the Safety Controller can monitor all traffic in silent mode. CAN FD via TCAN4550 adds 8-13 days and complexity to the Safety Controller for bandwidth the project doesn't need. Ethernet requires replacing 3 MCU boards, re-porting all firmware, and 6-10 weeks of work — the single highest finishability risk in the project. The FDCAN capability of the STM32G474RE is documented as a future upgrade path (ADR-005) without requiring it now. Ship classic CAN, demonstrate CAN-FD awareness in documentation.

---

## ADR-012: AWS IoT Core + Timestream + Grafana for cloud telemetry

- **Date**: 2026-02-21
- **Tier**: T2 — Design
- **Scores**: Cost 1 | Time 1 | Safety 1 | Resume 3 = **6/12**
- **Decision**: Use AWS IoT Core (MQTT broker) + Amazon Timestream (time-series DB) + Grafana (self-hosted dashboards) for cloud telemetry from the Pi edge gateway, batching to 1 msg/5sec for free tier compliance
- **Rationale**: AWS dominates automotive IoT — Mercedes-Benz, VW, Toyota, Stellantis, Continental all run on AWS. AWS IoT Core free tier covers 500K messages/month for 12 months. AWS IoT FleetWise is a purpose-built automotive feature (vehicle signal catalog, data campaigns). Grafana is fully free and open-source with unlimited sharing. The combination puts "AWS IoT Core" on the resume, which is the most recognized cloud IoT platform in automotive hiring.
- **Effort**: 3-5 days for setup + integration, ~$4-6/month (Timestream minimum during year 1), ~$5-7/month after free tier

### Alternative A: Azure IoT Hub + Cosmos DB + Power BI
- **Effort**: 4-7 days setup, $0/month if <240K messages (free tier) or $25/month (S1 tier for 500K+ messages) + $14/user/month for Power BI Pro (required to share dashboards)
- **Pros**: Microsoft ecosystem (some OEMs like BMW, Bosch use Azure). Cosmos DB has lifetime free tier (1,000 RU/s + 25 GB forever, not 12-month limited). Azure DPS for zero-touch device provisioning. Portfolio shows cloud platform versatility.
- **Cons**: Azure IoT Hub free tier only covers 240K messages/month (vs 500K on AWS). S1 tier is $25/month — 5x more expensive than AWS post-free-tier. Power BI requires $14/user/month to share dashboards (Grafana is free). Cosmos DB is a document database, not a time-series DB — no built-in downsampling or retention policies. Requires Azure Functions or Stream Analytics as middleware. Less automotive IoT ecosystem (no equivalent to AWS IoT FleetWise).

### Alternative B: Self-hosted Mosquitto + InfluxDB + Grafana on VPS
- **Effort**: 2-4 days setup, ~$6/month (Hetzner CX22: 2 vCPU, 4 GB RAM + domain)
- **Pros**: Cheapest long-term (~$6/month flat, no surprise costs). No vendor lock-in (all open source). Full control over data and infrastructure. InfluxDB is purpose-built for time-series (better than Timestream for queries/retention). Grafana fully free. Fastest to set up. Telegraf can bridge MQTT to InfluxDB with zero glue code.
- **Cons**: No automotive cloud credibility — no OEM uses self-hosted VPS for cloud telemetry. You maintain everything (OS updates, security patches, backups, uptime monitoring). No device provisioning service. No device shadow/twin. Single point of failure (one VPS). Less resume buzzword value ("self-hosted Mosquitto" vs "AWS IoT Core"). Cannot claim cloud-scale architecture experience.

### Why chosen wins
AWS IoT Core costs ~$4-7/month (comparable to self-hosted at ~$6/month) but adds managed infrastructure, device provisioning, device shadows, and the highest automotive industry credibility. Azure costs 4-5x more ($25+/month) for the same message volume and locks dashboards behind Power BI Pro licensing. Self-hosted is cheapest and technically excellent but sacrifices the "AWS IoT Core" resume keyword that automotive hiring managers specifically look for. The AWS stack is the optimal balance of cost, credibility, and managed simplicity for a portfolio project targeting automotive employers.

---

## ADR-013: Custom trace-gen.py + CI for Traceability Management

- **Date**: 2026-02-25
- **Tier**: T2 — Design
- **Scores**: Cost 1 | Time 2 | Safety 3 | Resume 2 = **8/12**
- **Decision**: Use a custom Python script (`scripts/trace-gen.py`) + GitHub Actions CI workflow for bidirectional traceability enforcement across the full V-model (8 levels, 475 requirements), with `scripts/suspect-links.py` for change impact analysis on PRs
- **Rationale**: The traceability data already lives in Markdown docs and code comments (`@verifies`, `@safety_req`, `**Traces up**:`). A 954-line pure-stdlib Python script parses all of it, builds a bidirectional graph, validates links, and generates both Markdown matrix and JSON output. CI enforcement (`--check` flag) blocks merge on broken links or untested leaf requirements. This is version-controlled, auditable, free, and demonstrates systems engineering skill — not just tool usage. For a portfolio project, showing you can BUILD the tooling is more impressive than showing you can buy a license.
- **Effort**: ~3 days implementation, $0/month

### Alternative A: Siemens Polarion ALM
- **Effort**: $15,000-50,000/year (per-seat licensing), 2-4 weeks setup + training
- **Pros**: Industry-standard ALM tool used by BMW, Continental, Bosch, ZF. Full lifecycle management (requirements, test management, change requests, baselines). Built-in traceability matrix with GUI. Regulatory compliance reports (ISO 26262, ASPICE). Workflow automation with configurable approval chains. Live collaboration for distributed teams. TUV-recognized as qualified tool for safety-critical development. Assessors expect and trust Polarion data exports.
- **Cons**: Prohibitively expensive for a solo portfolio project ($15K+ minimum). Requires server infrastructure (on-prem or cloud instance). Licensing complexity (named vs concurrent seats). Heavy admin overhead — configuring schemas, workflows, roles takes weeks before any productive use. Requirements live in a proprietary database, not in version-controlled files alongside the code. Cannot diff requirement changes in git — breaks the "everything in git" philosophy. Learning curve for Polarion-specific concepts (Work Items, Live Documents, Linked Work Items). Vendor lock-in: extracting data for migration is notoriously painful (proprietary XML export). Overkill for 475 requirements managed by one person.

### Alternative B: IBM DOORS / DOORS Next Generation
- **Effort**: $8,000-30,000/year (per-seat licensing), 3-6 weeks setup
- **Pros**: The most widely used requirements management tool in automotive (legacy installed base: VW, Toyota, GM). OSLC integration for cross-tool traceability. Formal baseline and change management. Module-based structure mirrors V-model hierarchy well. Strong regulatory acceptance — assessors know DOORS inside and out.
- **Cons**: Even more expensive than Polarion for small teams. DOORS Classic is aging (Windows-only, dated UI). DOORS Next (DNG) is web-based but resource-heavy. Same vendor lock-in problem as Polarion — requirements stored in proprietary format. IBM's automotive tools division has been declining (Rational brand is legacy). Setup complexity is extreme for a solo developer — DOORS requires database server, license server, admin training. No git integration — requirements and code live in separate worlds. Cannot be demonstrated in a portfolio without a live license.

### Alternative C: Jama Connect
- **Effort**: $10,000-25,000/year, 1-2 weeks setup
- **Pros**: Modern web UI, strong automotive/defense customer base (Aptiv, Magna, Qualcomm). Built-in traceability views and impact analysis. REST API for integration. Good ASPICE/ISO 26262 compliance reporting.
- **Cons**: Same cost barrier ($10K+/year). SaaS-only — no self-hosted option, data lives on Jama's servers. Requirements disconnected from git. Smaller automotive footprint than Polarion or DOORS. Cannot run locally or in CI — depends on cloud service availability.

### Why chosen wins
All commercial ALM tools (Polarion, DOORS, Jama) cost $8,000-50,000/year and require weeks of setup — absurd for a solo portfolio project with 475 requirements. More importantly, they all store requirements in proprietary databases disconnected from the codebase. The custom approach keeps everything in git: requirements in Markdown, trace tags in code comments, validation in CI. This means `git diff` shows requirement changes, `git blame` shows who changed what, and every baseline is a git tag — all ASPICE SUP.8 requirements met for free. The CI enforcement (blocking merge on broken links) is functionally equivalent to what Polarion provides via workflow rules, but it's transparent, auditable, and version-controlled. For hiring, building a traceability pipeline from scratch demonstrates deeper systems engineering understanding than clicking through a Polarion UI — any engineer can learn Polarion in 2 weeks, but designing a V-model traceability graph with suspect-link detection shows architectural thinking.

---

## ADR-014: Platform abstraction via link-time hw-file swap + config header split

- **Date**: 2026-03-10
- **Tier**: T2 — Design
- **Scores**: Cost 1 | Time 2 | Safety 3 | Resume 3 = **9/12**
- **Decision**: Eliminate all `#ifdef PLATFORM_*` from application code (SWC + SC) using four AUTOSAR-aligned patterns: (1) link-time hw-file swap (Vector vVIRTUALtarget), (2) config header split via `-I` path (AUTOSAR EcuC), (3) Det callout for debug text, (4) MMIO register isolation into hw-file functions
- **Rationale**: `#ifdef` in application code violates the AUTOSAR principle "same source, same binary logic." Platform differences must be confined to MCAL/BSW layers selected at build time. This cleanup removed 35+ ifdefs across 10+ files in SWC, SC, and BSW layers, making application code compile identically on STM32, TMS570, and POSIX. The patterns chosen are identical to those used by Vector vVIRTUALtarget and EB tresos in production AUTOSAR stacks. Zero `#ifdef PLATFORM_*` in any SWC source or SC main code — only hw-files and MCAL retain platform guards (by design).
- **Effort**: ~2 days implementation + verification, $0

### Alternative A: Preprocessor ifdefs (status quo)

- **Effort**: 0 hours (no change), $0
- **Pros**: Already working. No risk of regressions. Familiar to all developers.
- **Cons**: Application code diverges per platform — bugs can hide behind ifdefs. SIL and target execute different code paths. MISRA Rule 20.11 violations. Not representative of AUTOSAR production practice. Grows worse with each new platform variant (HIL, PIL). Makes code review harder — reviewers must mentally track which blocks are active.

### Alternative B: Runtime platform detection (function pointers / vtable)

- **Effort**: 3-5 days for abstraction layer + registration, $0
- **Pros**: Single binary for all platforms (if hardware allows). Late binding — can switch platform at startup. Extensible to new platforms without recompilation.
- **Cons**: Runtime overhead (function pointer indirection on every HW call). Larger binary — all platform code linked even if unused. Safety concern — wrong platform code reachable at runtime (ASIL D violation: dead code in safety path). Not AUTOSAR practice — AUTOSAR uses compile-time/link-time selection, never runtime. Impossible for TMS570 MMIO — can't have POSIX code referencing TMS570 register addresses in the same binary.

### Why chosen wins

Link-time swap + config header split is the exact pattern used by Vector, EB, and ETAS in production AUTOSAR stacks. It gives zero runtime overhead, zero dead code in the binary, and compile-time guarantee that platform-wrong code is unreachable (link error, not runtime error). Runtime detection is fundamentally incompatible with ASIL D (reachable dead code) and impossible for MMIO (segfault on wrong platform). Keeping ifdefs is the easy path but accumulates technical debt and diverges SIL from target — the opposite of what platform abstraction should achieve.

