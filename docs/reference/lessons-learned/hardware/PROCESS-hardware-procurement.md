# Lessons Learned — Hardware Procurement and BOM Management

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-20 to 2026-03-01
**Scope:** Sourcing, verifying, and documenting hardware for a 4-ECU physical build + HIL bench
**Result:** Complete BOM, procurement validated, multiple datasheet errors caught

---

## 1. Amazon Listings Lie — Always Check the Datasheet

The MG996R servo was listed as "360-degree" on Amazon. It's actually a 180-degree servo (±90° from center). This affects steering angle calculations and mechanical design.

**Impact:** Steering angle range in firmware was initially wrong.

**Lesson:** Never trust seller descriptions. Download the manufacturer datasheet and verify every parameter.

---

## 2. Component Variants Have Different Specs

The ACS723 current sensor comes in multiple variants:
- ACS723-05A: 5A range, **400 mV/A** sensitivity
- ACS723-20A: 20A range, **100 mV/A** sensitivity

The project uses the 20A variant but 5 documents had 400 mV/A (the 5A variant's sensitivity).

**Impact:** Current calculations were 4x wrong everywhere the incorrect sensitivity was used.

**Lesson:** When specifying a component, always include the full part number including variant suffix. "ACS723" is ambiguous; "ACS723LLCTR-20AU-T" is not.

---

## 3. SOT-23 Components Need Breakout Boards

BZX84C3V3 Zener diodes come in SOT-23 package (2.9mm × 1.6mm). Too small to hand-solder directly to perfboard or breadboard.

**Fix:** Ordered SOT-23 to DIP adapter breakout boards (Colcolo). Arriving separately.

**Lesson:** Check the package size of every SMD component against your assembly method. If hand-soldering, order breakout boards with the components.

---

## 4. Pin Assignment Conflicts Surface Late

The Safety Controller's LED_SYS was assigned to a port already used by another function. This wasn't caught during schematic review because pin assignments were spread across multiple documents.

**Fix:** Moved LED_SYS to port A, added heartbeat LED on port B. Updated all documents.

**Lesson:** Maintain a single-source pin mapping document. Review it against every module's requirements before ordering hardware.

---

## 5. BOM Should Include Procurement Status

The BOM was initially just a parts list. Added columns for:
- **Source** (Amazon, Mouser, DigiKey, AliExpress)
- **Status** (ORDERED, SHIPPED, ON HAND, BACKORDERED)
- **Received date**
- **Quantity on hand vs needed**

**Lesson:** A BOM without procurement status is a shopping list. A BOM with status is a project management tool.

---

## 6. Merge BOM Files Early

Multiple BOM files existed: one in `hardware/bom.md`, one in the hardware design doc, and one in the procurement validation doc. They drifted.

**Fix:** Single `hardware/bom.md` as source of truth. All other docs reference it.

**Lesson:** Same as cross-document consistency: one source, references everywhere else. BOMs are especially prone to drift because they change with every order.

---

## 7. Budget Tracking Per Subsystem

| Subsystem | Budget | Actual | Status |
|-----------|--------|--------|--------|
| MCUs (STM32 × 3) | €45 | €42 | On hand |
| Safety Controller (TMS570) | €25 | €23 | On hand |
| Sensors (AS5048A, ACS723, TFMini-S, NTC) | €45 | €52 | On hand |
| Actuators (motor, servos) | €35 | €38 | On hand |
| Protection (Zeners, resistors) | €5 | €4 | On hand |
| Breakout boards | €10 | €8 | Arriving Mar 5-10 |
| **Total** | **€165** | **€167** | **On track** |

**Lesson:** Track actual spend against budget per subsystem. €2 over on sensors, €3 over on actuators — still within total budget.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Seller descriptions | Never trust — download the manufacturer datasheet |
| Component variants | Include full part number with variant suffix |
| SMD packages | Check package vs assembly method — order breakouts if needed |
| Pin conflicts | Single-source pin map, review before hardware order |
| BOM status | Add procurement status columns — shopping list → project tool |
| BOM drift | One source of truth — same as any engineering document |
| Budget tracking | Track per subsystem, compare plan vs actual |
