# Lessons Learned — HIL Gap Analysis & Fault Injection Safety

**Date**: 2026-03-01
**Scope**: Comparison of DIY HIL bench vs professional dSPACE/Vector systems, gap closure strategy, fault injection hardware safety

---

## 1. DIY HIL Is Legitimately Beyond Any GitHub/YouTube Project

- Searched GitHub and YouTube exhaustively for comparable DIY embedded projects
- No DIY project found includes: MISRA compliance, hardware FIU, rest-bus simulation, automated test traceability to safety requirements, or XCP calibration
- Our 7-ECU AUTOSAR platform with 1,067 tests and 548 requirements is categorically different from typical Arduino/ESP32 projects
- **Lesson**: Don't undersell the project's complexity. It belongs in a different tier than hobbyist embedded work.

## 2. Professional HIL Costs Are Absurd — But Their Architecture Is Instructive

- dSPACE SCALEXIO: $150k-$500k per bench
- Vector VT System + CANoe: $80k-$250k per bench
- OEMs have 50+ spare ECUs at $500-$5000 each — they can afford to blow them up during fault injection
- **Lesson**: We can replicate ~80% of professional HIL capability for ~€105 in hardware + $0 software. The remaining 20% (sub-µs timing, 16-bit analog, VBAT fault injection) doesn't matter for a portfolio project.

## 3. VBAT Fault Injection Is Too Dangerous for Budget Hardware

- Professional HIL does short-to-VBAT because ECUs have built-in 12V protection on I/O pins, and OEMs have unlimited spare boards
- We have 3 Nucleos (€16 each) and 1 LaunchPad (€40) — no spares, no protection
- 12V on a 3.3V STM32 GPIO kills the chip instantly — no recovery
- **Decision**: Skip VBAT fault bus entirely. Only build short-to-GND + open-circuit. This covers ~70% of wiring fault scenarios with zero hardware risk.
- **Lesson**: Never build a test setup that can destroy the test target. Budget constraints change the risk calculus vs. production HIL labs.

## 4. Preemptive Protection Is Cheaper Than Replacement

- BZX84C3V3 Zener + 100R resistor = €0.10 per ADC channel
- Nucleo-64 replacement = €16 + shipping + lost weekend
- **Action taken**: Added Phase 4.6 to bring-up plan — install Zener clamps on all ADC pins before connecting any sensor (Phase 5)
- **Lesson**: Always add overvoltage protection before connecting anything to MCU analog pins. It costs nothing and prevents the most common beginner mistake.

## 5. Oscilloscope Is a Verification Tool, Not a Protection Device

- Initial confusion: thought the oscilloscope could "stop" faults
- Reality: the oscilloscope shows you what's happening on a wire — it doesn't interrupt anything
- **Correct use**: Probe a wire before triggering a fault, confirm the voltage matches expectation (3.3V signal, not 12V power), then trigger the fault
- **Lesson**: Verification before action. The oscilloscope is your last chance to catch a miswiring before it destroys hardware.

## 6. Reuse Inventory Before Buying New

- Initially listed Arduino Mega as new purchase (€12) for FIU controller
- User pointed out: Arduino UNO R3 already in Table D of bring-up plan (unused)
- Also found reusable: passives (resistors, caps from BOM #43-54), perfboard (BOM #61)
- **Savings**: ~€15 by checking inventory first
- **Lesson**: Always grep the BOM and bring-up plan for existing parts before adding new items. "Not needed" items in Table D might be perfect for secondary uses.

## 7. Gap Closure Priority Should Follow ROI, Not Difficulty

- Best order (from gap analysis):
  1. Rest-bus simulation ($0, 2-3 days) — highest ROI
  2. Automated test traceability ($0, 1 week) — ASPICE compliance
  3. Hardware FIU (~€33, 3-5 days) — tangible artifact
  4. Sensor simulation (~€25, 3-5 days) — signal chain demo
- **Lesson**: Free software-only gaps first, then cheap hardware gaps. Don't start with the expensive items.

## 8. Safety Rules Must Be Written Down Before Hardware Arrives

- Discussed 7 mandatory safety rules for fault injection testing
- Added them to both the gap analysis (Section 10) and the bring-up plan (Safety Rules section)
- **Lesson**: Document safety rules while the reasoning is fresh. By the time hardware arrives and you're excited to plug things in, you won't remember why VBAT fault buses are dangerous.

---

## Key Files Modified (2026-03-01)

| File | Change |
|------|--------|
| `docs/plans/gap-analysis-hil-bench-vs-professional.md` | Created (v1.1). 8 gaps, closure strategies, Section 10 safety cautions |
| `hardware/bom.md` | Added Section 3.13 — HIL gap closure items #77-87 with Amazon.de links |
| `docs/INDEX.md` | Added HIL Gap Analysis entry |
| `docs/plans/plan-hardware-bringup.md` | Added Phase 4.6 (Zener protection) and rules 8-14 (FIU safety) |

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| Skip VBAT fault bus | MCU damage risk too high for budget. GND-only + open-circuit covers ~70% of faults |
| Use Arduino UNO R3 for FIU | Already owned, adequate I/O for 8-channel relay control |
| Install Zener clamps before sensors | €0.10/channel vs €16 board replacement |
| GND-only faults with 100R limiting | 33mA at 3.3V is safe for all components |
| Total HIL closure: ~€105 | Down from ~€115 after inventory reuse |
