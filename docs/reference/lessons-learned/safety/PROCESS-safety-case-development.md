# Lessons Learned — Safety Case Development

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-20 to 2026-02-27
**Scope:** Building a complete ISO 26262 safety case from scratch (HARA → Safety Goals → FSR → TSR → SSR/HSR)
**Result:** 8 safety goals, 20 hazardous events, 25 FSRs, 50 FMEA failure modes, 6 CCFs, complete flow-down

---

## 1. Build the Safety Case Top-Down, Not Bottom-Up

The correct order is:
```
Item Definition → HARA → Safety Goals → FSC → FSR → TSR → SSR/HSR
```

**Mistake temptation:** Start with the code (SSR) and work up. This produces requirements that justify existing code, not requirements that drive safe design.

**Lesson:** Even for a portfolio project, top-down safety development produces a defensible argument. Bottom-up produces rationalization.

---

## 2. HARA: S3/E4/C3 = ASIL D — No Debate

For drive-by-wire:
- **S3** (fatal/life-threatening): unintended acceleration, loss of steering, loss of braking
- **E4** (high probability): vehicle is operated every time it's powered on
- **C3** (uncontrollable): at speed, operator cannot prevent collision

This combination always yields ASIL D. Every pedal/steering/brake safety goal is ASIL D.

**Lesson:** Don't overthink HARA for drive-by-wire. The numbers are clear. Spend time on the safety mechanisms, not the rating.

---

## 3. Safety Goals Need FTTI Justification

Each safety goal must include a Fault Tolerant Time Interval (FTTI) — the maximum time from fault occurrence to reaching a safe state.

**Example:** SG-001 (unintended acceleration): FTTI = 100 ms. Derived from: 50 ms fault detection + 50 ms motor de-energization.

**Lesson:** FTTI is not a guess. It's derived from the detection mechanism's response time + the actuator's de-energization time. Document the derivation.

---

## 4. FMEA: 50 Failure Modes Is Realistic for This Scale

The FMEA covered:
- Sensor failures (pedal, steering, lidar, current, temperature)
- Actuator failures (motor, steering servo, brake servo)
- Communication failures (CAN bus, E2E, heartbeat)
- Processing failures (CPU, memory, watchdog)
- Power failures (battery, relay, regulator)

**Lesson:** A complete FMEA for a 7-ECU system produces ~50 failure modes. Don't aim for 200 — that's a sign of over-decomposition.

---

## 5. DFA: Document Coupling Factors Explicitly

Dependent Failure Analysis identified 6 Common Cause Failures (CCFs):
- Shared CAN bus (all ECUs)
- Shared power supply (12V bus)
- Shared SPI bus (dual pedal sensors on SPI1)
- Shared software methodology (same developer for all ECUs)
- Physical proximity (bench setup)
- Environmental stress (bench temperature)

**Lesson:** The coupling factors ARE the analysis. List every shared resource, evaluate if it can take out redundant channels, document the mitigation.

---

## 6. SG-003 Correction: DEGRADED → SAFE_STOP

Original safety goal SG-003 (steering fault) specified transition to DEGRADED state. HITL review corrected: steering has no mechanical fallback, so the only safe state is SAFE_STOP (full stop).

**Before:** Steering fault → DEGRADED (reduced steering capability)
**After:** Steering fault → SAFE_STOP (return to center, apply brakes, stop)

**Lesson:** DEGRADED is only valid when the subsystem has a degraded-mode capability. No fallback = no degraded mode = SAFE_STOP.

---

## 7. FSR to TSR: The "How" Translation

Functional Safety Requirements (FSR) say WHAT: "Detect pedal sensor disagreement."
Technical Safety Requirements (TSR) say HOW: "Compare sensor 1 and sensor 2 readings every 10 ms; fault if |diff| > 5% for 2 cycles."

**Lesson:** FSR should be implementable by multiple designs. TSR commits to a specific design. If your FSR mentions specific hardware (AS5048A, SPI), it's a TSR.

---

## 8. ASIL Decomposition Is Rarely Worth It

ASIL decomposition (e.g., D → B+B) was evaluated but not used. The independence proof (DFA, FFI) costs more effort than developing both elements at ASIL D.

**Lesson:** For small teams, decomposition adds process overhead without reducing development effort. Just develop at the original ASIL unless elements are physically separate.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Build order | Top-down: HARA → SG → FSR → TSR → SSR. Never bottom-up. |
| HARA for DbW | S3/E4/C3 = ASIL D, no debate needed for drive-by-wire |
| FTTI | Derive from detection + reaction time, don't guess |
| FMEA scope | ~50 failure modes for 7-ECU system — don't over-decompose |
| DFA | List coupling factors explicitly — that IS the analysis |
| DEGRADED vs SAFE_STOP | No mechanical fallback = no degraded mode |
| FSR vs TSR | FSR = what (implementation-free), TSR = how (specific design) |
| ASIL decomposition | Independence proof costs more than just developing at ASIL D |
