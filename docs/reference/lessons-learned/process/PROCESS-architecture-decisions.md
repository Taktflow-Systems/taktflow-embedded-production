# Lessons Learned — Architecture Decisions

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-19 to 2026-02-28
**Scope:** Major architectural decisions for the embedded platform
**Result:** 13 ADRs documented, decision framework established

---

## 1. ADR Format with Tier + Scoring

Every decision gets:
- **Tier** (T1-T4): Architecture → Design → Implementation → Process
- **4-dimension score**: Cost (1-3) + Time (1-3) + Safety (1-3) + Resume (1-3) = Impact score
- **Two alternatives** with effort estimates and pros/cons
- **Why chosen wins**: Explicit comparison

**Why the "Resume" dimension:** This is a portfolio project. Every decision should maximize learning value and keyword coverage for job applications, not just technical optimality.

**Lesson:** Decision records that include effort estimates and explicit alternatives are 10x more useful than records that only state the choice. The "why NOT the alternative" is often more valuable than "why this."

---

## 2. 7-ECU Hybrid Architecture (4 Physical + 3 Simulated)

| ECU | Platform | Why |
|-----|----------|-----|
| CVC, FZC, RZC | STM32 (physical) | Safety-critical: pedal, steering, brake, motor |
| SC | TMS570 (physical) | Lock-step safety controller — must be real HW |
| BCM, ICU, TCU | Docker (simulated) | Non-safety: body control, dashboard, telematics |

**Why hybrid:** Full 7-ECU physical build would cost ~€500+ and weeks of PCB work. Simulated ECUs run the same firmware code on POSIX, connected via virtual CAN (vcan).

**Lesson:** Simulate what you can, build what you must. The CAN bus doesn't know if the sender is physical or simulated.

---

## 3. File-Based ALM Over Heavyweight Tools

Chose Git + Markdown over Polarion, DOORS, or Jama for requirements management.

**Why:**
- €0 cost vs €10k+/year for DOORS
- Git provides versioning, branching, diffing, CI integration
- Markdown is human-readable without a tool
- `trace-gen.py` generates traceability matrix from file references

**Tradeoff:** No GUI, no drag-and-drop linking, no visual traceability. Mitigated by `trace-gen.py` automation.

**Lesson:** For a portfolio/demo project, file-based ALM demonstrates the process without the tool cost. The ASPICE assessor cares about the process, not the tool brand.

---

## 4. Custom trace-gen.py Over Commercial Tools (ADR-013)

Built a Python script that:
- Parses all requirement docs (SYS, SWR, TSR, SSR)
- Follows `@traces_to`, `@verifies` tags in code and test files
- Generates a complete V-model traceability matrix
- Detects broken links and untested requirements
- Runs in CI — blocks merge on traceability gaps

**Why not Polarion/DOORS:** €10k+/year for a personal project. The script covers 90% of the functionality in 500 lines of Python.

**Lesson:** Traceability is a process, not a tool. A script that runs in CI and catches gaps is more effective than an expensive tool that nobody configures correctly.

---

## 5. Zonal Architecture for Learning Value

The zonal architecture (zone controllers instead of domain controllers) was chosen partly for resume value — it's the architecture BMW, VW, and Tesla are moving to.

**Technical justification:** Zonal reduces wiring harness complexity and enables domain-independent zone processing. But honestly, for a bench-scale demo, a simpler domain architecture would work fine.

**Lesson:** In a portfolio project, "what will I learn" and "what keywords will appear on my CV" are valid decision criteria alongside technical merit. Document them transparently.

---

## 6. AUTOSAR-Like BSW (Not Full AUTOSAR)

Built a BSW layer that follows AUTOSAR patterns (MCAL, ECUAL, Services, RTE) but is not actual AUTOSAR. No AUTOSAR license, no code generation, no XML configuration.

**Why "like":** AUTOSAR Classic licensing costs €50k+. The pattern demonstrates the architecture understanding without the vendor lock-in.

**What's AUTOSAR-like:**
- Layer structure (MCAL → ECUAL → Services → RTE)
- Module naming (Can, CanIf, PduR, Com, Dcm, Dem, WdgM, BswM)
- API patterns (Init, MainFunction, callback registration)
- Configuration via `*_Cfg.h` / `*_Cfg.c`

**What's NOT AUTOSAR:**
- No ARXML configuration
- No code generator
- No standardized API signatures (simplified for this project)

**Lesson:** "AUTOSAR-like" is honest and impressive. Claiming "AUTOSAR" without a license is dishonest and easily challenged.

---

## 7. Decision Audit Script

`scripts/decision-audit.sh` greps for undocumented decisions:
- Checks all ADRs have required fields
- Checks all `<!-- DECISION: -->` tags reference valid ADRs
- Heuristic grep for untagged decisions in docs

**Lesson:** Decisions are easy to make and easy to forget documenting. An automated audit catches the gaps.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| ADR format | Include effort, alternatives, and "why chosen wins" — not just the choice |
| Resume dimension | In portfolio projects, learning value is a valid decision criterion |
| Hybrid architecture | Simulate what you can, build what you must |
| File-based ALM | Git + Markdown + scripts covers 90% of commercial ALM at 0% of the cost |
| Traceability | A CI script that blocks merge > an expensive tool nobody configures |
| AUTOSAR-like | Be honest about "like" vs actual; the pattern matters more than the license |
| Decision audit | Automate the check for undocumented decisions |
| Platform abstraction | Link-time swap > #ifdef. Same public API, different .c files per platform |

---

## 8. Platform Abstraction via Link-Time Swap (Not #ifdef)

**Date:** 2026-03-10

**Context**: IoHwAb.c and Det.c had 35 `#ifdef PLATFORM_*` blocks mixing POSIX, HIL, and target code in single files. MCAL-layer ifdefs were already clean (separate directories), but BSW ECUAL/Services were polluted.

**Mistake**: Original approach embedded platform-variant logic inline with `#ifdef PLATFORM_POSIX` / `#elif defined(PLATFORM_HIL)` / `#else` chains. This made every function a maze of conditionals, made testing require `-DPLATFORM_*` flags, and violated the AUTOSAR principle that BSW modules should be platform-agnostic above MCAL.

**Fix**: Applied Vector vVIRTUALtarget production pattern:
1. **Det.c** — extracted `fprintf`-based debug output to `Det_Callout_Sil.c` (POSIX-only file). Det.c now uses callback mechanism (`Det_SetCallback`). Zero ifdefs.
2. **IoHwAb.c** — split into 3 complete implementations: `IoHwAb.c` (target/MCAL), `IoHwAb_Posix.c` (injection buffers), `IoHwAb_Hil.c` (override + MCAL fallback). Each is a standalone compilation unit. Makefile selects which `.c` to link.
3. **Security**: Injection APIs (`IoHwAb_Posix_SetSensorValue`) exist only in `IoHwAb_Posix.c`. On target, calling them is a **link error** — not `E_NOT_OK`, an actual linker failure. Strongest possible protection against accidental sensor injection in production.

**Result**: 25/25 BSW tests pass. Det.c/IoHwAb.c/IoHwAb.h have zero `#ifdef PLATFORM_*`. 42 new TDD tests (6 Det callout + 18 POSIX + 18 HIL).

**Principle**: Link-time file swap is strictly better than `#ifdef` for platform abstraction. Benefits: (1) each variant is independently readable and testable, (2) no ifdef nesting makes code review trivial, (3) injection-as-link-error provides compile-time safety guarantees that runtime checks cannot match, (4) follows production AUTOSAR patterns (Vector, EB, ETAS).
