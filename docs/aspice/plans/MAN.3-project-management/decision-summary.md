---
document_id: MAN3-DECISION-SUMMARY
title: "Decision Summary — Quick Reference"
version: "2.0"
status: active
updated: "2026-02-22"
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


# Decision Summary

Quick reference for all approved decisions with tier ranking and impact scores. For full rationale, alternatives, and effort analysis, see [decision-log.md](decision-log.md).

## Ranking System

**Tiers** (by reversibility and impact):

| Tier | Name | Reversibility | Meaning |
|------|------|---------------|---------|
| **T1** | Architecture | Locked (months+) | Defines system shape — MCU, bus, safety architecture |
| **T2** | Design | Hard (weeks) | Affects multiple components — BSW, test stack, cloud |
| **T3** | Implementation | Moderate (days) | Affects one component — library, driver, API |
| **T4** | Process | Easy (hours) | Affects workflow only — folders, naming, docs |

**Scores** (1-3 per dimension, total /12):

| Dimension | 1 (Low) | 2 (Medium) | 3 (High) |
|-----------|---------|------------|----------|
| Cost ($) | < $50 | $50-$500 | > $500 |
| Time | < 1 week | 1-4 weeks | > 4 weeks |
| Safety | QM | ASIL A-C | ASIL D |
| Resume | Generic | Industry-relevant | Top automotive keyword |

---

## All Decisions Ranked by Tier

### T1 — Architecture (system-defining, locked)

| ADR | Decision | Cost | Time | Scores (C/T/S/R) | Total | Key Reason |
|-----|----------|------|------|-------------------|-------|------------|
| ADR-005 | **STM32G474RE Nucleo-64** ×3 | $60 | 10-14d | 2/2/3/3 | **10** | 3x FDCAN, 5x ADC, CORDIC/FMAC, dominant automotive MCU |
| ADR-011 | **CAN 2.0B at 500 kbps** | $0 | 0d | 1/1/3/2 | **7** | TMS570 DCAN = classic only; 35% utilization, no pressure |
| ADR-006 | **AUTOSAR Classic layered BSW** | $0 | 10-14d | 1/2/3/3 | **9** | 16+ resume keywords, ASPICE SWE.2/SWE.3, 10/10 impact |

### T2 — Design (multi-component, hard to reverse)

| ADR | Decision | Cost | Time | Scores (C/T/S/R) | Total | Key Reason |
|-----|----------|------|------|-------------------|-------|------------|
| ADR-007 | **POSIX SocketCAN** simulated MCAL | $0 | 5-8d | 1/2/1/3 | **7** | 100% code reuse physical ↔ simulated ECUs |
| ADR-008 | **BMW vsomeip** for SOME/IP | $0 | 4-7d | 1/1/1/3 | **6** | Industry-standard, millions deployed, protocol visible |
| ADR-009 | **Docker containers** for vECUs | $0 | 1-2d | 1/1/1/2 | **5** | Isolation + reproducibility + CI/CD |
| ADR-010 | **Unity + CCS Test + pytest** | $0 | 2-4h+1-2d | 1/1/3/2 | **7** | Pure C, MIT, embedded-designed, actively maintained |
| ADR-012 | **AWS IoT Core + Timestream + Grafana** | $4-7/mo | 3-5d | 1/1/1/3 | **6** | Highest automotive OEM adoption, free tier |
| ADR-013 | **Custom trace-gen.py + CI** for traceability | $0 | 3d | 1/2/3/2 | **8** | Free, git-native, CI-enforced, demonstrates systems engineering |

### T4 — Process (workflow, easy to reverse)

| ADR | Decision | Cost | Time | Scores (C/T/S/R) | Total | Key Reason |
|-----|----------|------|------|-------------------|-------|------------|
| ADR-001 | **ASPICE folder structure** | $0 | 2h | 1/1/1/2 | **5** | Assessor-friendly, scales with project |
| ADR-002 | **master-plan.md as baseline** | $0 | 0h | 1/1/1/1 | **4** | Single strategic source of truth |
| ADR-003 | **Central docs/research/** | $0 | 1h | 1/1/1/1 | **4** | Git-versioned research provenance |
| ADR-004 | **MAN.3 live tracking set** | $0 | 3h | 1/1/1/2 | **5** | ASPICE MAN.3 evidence in-repo |

---

## Summary by Tier

| Tier | Count | Total Cost | Total Time | Avg Score |
|------|-------|-----------|------------|-----------|
| T1 — Architecture | 3 | $60 | 20-28 days | 8.7 |
| T2 — Design | 6 | $4-7/mo | 17-27 days | 6.5 |
| T4 — Process | 4 | $0 | 6 hours | 4.5 |
| **All** | **13** | **$60 + $4-7/mo** | **~39-53 days** | **6.3** |

## Rejected Alternatives (Top Reasons)

| Rejected | ADR | Tier | Why |
|----------|-----|------|-----|
| STM32F446RE | 005 | T1 | No CAN-FD — legacy technology signal |
| Arduino Mega 2560 | 005 | T1 | 8-bit, 8KB RAM, no CAN — resume damage |
| Bare-metal (no AUTOSAR) | 006 | T1 | ASPICE SWE.2 non-compliant, 3/10 resume |
| FreeRTOS without AUTOSAR | 006 | T1 | No AUTOSAR keywords, 5/10 resume |
| CAN FD (via TCAN4550) | 011 | T1 | 8-13 extra days for unneeded bandwidth |
| Ethernet + CAN hybrid | 011 | T1 | 6-10 weeks, must replace all MCU boards |
| Python-can simulation | 007 | T2 | Zero code reuse with physical firmware |
| Vector CANoe | 007 | T2 | $10K-$15K+ — prohibitive for portfolio |
| Custom SOME/IP | 008 | T2 | 4-8 weeks reimplementing existing solution |
| CommonAPI + vsomeip | 008 | T2 | 6-repo toolchain, hides protocol details |
| QEMU/Renode emulation | 009 | T2 | 3-15x setup, G474 not supported upstream |
| Google Test (gtest) | 010 | T2 | C++17 required, unnecessary for pure C |
| CUnit | 010 | T2 | Abandoned since 2018, no mock generation |
| Azure IoT Hub | 012 | T2 | $25/month min (5x AWS cost) |
| Self-hosted Mosquitto | 012 | T2 | Loses automotive resume keyword |
| Siemens Polarion ALM | 013 | T2 | $15K+/year, weeks of setup, proprietary DB |
| IBM DOORS / DNG | 013 | T2 | $8K+/year, aging UI, no git integration |
| Jama Connect | 013 | T2 | $10K+/year, SaaS-only, disconnected from git |

