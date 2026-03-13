# Lessons Learned — SYS-001: Dual Pedal Position Sensing

**Requirement**: The system shall sense the operator pedal position using two independent AS5048A magnetic angle sensors connected to the CVC via SPI1 with separate chip-select lines (PA4 and PA15). Both sensor readings shall be acquired in every control cycle and made available to the pedal plausibility monitoring function.
**ASIL**: D
**Reviewers**: An Dao (FSE), Claude (AI assistant)
**Review date**: 2026-02-27
**Status**: Closed

---

## Review Discussion

### Topic 1: ASIL D Rating

- **Raised by**: An Dao
- **Discussion**: Is ASIL D correct for pedal sensing? Undetected pedal position error → unintended acceleration. HARA: S3 (fatal), E4 (high probability), C3 (uncontrollable). S3/E4/C3 = ASIL D per determination matrix.
- **Resolution**: ASIL D confirmed. No debate — drive-by-wire pedal sensing is the primary operator input to the torque path.
- **Action item**: None.

### Topic 2: 1oo2D Architecture on Shared SPI Bus

- **Raised by**: An Dao
- **Discussion**: Two AS5048A sensors with separate chip-select lines (PA4, PA15) on shared SPI1. Is this truly independent for ASIL D?
  - **1oo2D** = "1 out of 2 with Diagnostics" (IEC 61508). Two channels, either sufficient, each with diagnostics (AS5048A CRC).
  - **Chip Select (CS)** gives logical isolation: CVC pulls PA4 LOW → sensor 1 responds, sensor 2 ignores (and vice versa). Each sensor can be read and diagnosed individually.
  - **Shared-bus concern**: SCK/MISO/MOSI are shared wires. A physical fault on any shared line takes out both sensors — this is a common cause failure (CCF) coupling factor per ISO 26262 Part 9 DFA.
  - **Counter-argument**: Both sensors return bad CRC simultaneously → plausibility check detects → torque = 0 (safe state). The failure mode is detected, not silent — it becomes a detected multi-point fault.
- **Resolution**: Acceptable for dev platform. Detection path exists (CRC + plausibility → safe state). DFA must explicitly document this coupling factor with the detection argument.
- **Action item**: Document shared SPI bus as CCF coupling factor in DFA report. Owner: FSE. Deadline: DFA phase.

### Topic 3: Alternative — SPI1 + SPI2 (Separate Buses)

- **Raised by**: Claude
- **Discussion**: Full electrical independence — each sensor on its own SPI peripheral with completely separate wires. Why not?
  1. **Detection covers it** — shared bus fault → both CRCs fail → plausibility catches it → safe state. Never silent.
  2. **Pin/peripheral budget** — SPI2 may be needed elsewhere; burns 3 more GPIO pins.
  3. **Dev platform, not production** — risk profile doesn't demand full electrical independence.
  - **Corrected misconception** (raised by An Dao): DMA/driver complexity is NOT a valid reason. SPI2 config is copy-paste from SPI1 with different peripheral address and DMA channel. Trivial to set up.
- **Resolution**: Keep shared SPI1 for dev platform. Upgrade to SPI1 + SPI2 for production.
- **Action item**: None (documented as production upgrade path).

### Topic 4: Verification Criteria

- **Raised by**: Claude
- **Discussion**:
  - **1% linearity across 0–100% travel**: Measurable on bench with reference angle fixture. AS5048A datasheet: 0.05° typical accuracy on 360°. For ~90° pedal travel, 1% ≈ 0.9° — well within sensor capability.
  - **Both readable within 10 ms**: SPI at 1 MHz, AS5048A frame = 16 bits → ~16 µs per read. Two reads ≈ 32 µs. Budget is 10,000 µs (300x margin).
  - **Gap noted**: Verification criteria don't explicitly state what happens if a sensor read hangs (SPI bus locked up). The "readable within 10 ms" wording implies failure, but an explicit SPI timeout requirement could strengthen this.
- **Resolution**: Criteria are concrete and testable. Gap is covered implicitly by SYS-002's plausibility fault path. Consider adding explicit SPI timeout to verification criteria in a future revision.
- **Action item**: None (gap is non-critical, covered by SYS-002).

---

## Corrections Made During Review

| What changed | Before | After | Reason |
|-------------|--------|-------|--------|
| DMA complexity argument | Listed as reason against SPI1+SPI2 | Removed — not valid | SPI2 config is trivial copy-paste; don't overstate complexity |

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Shared SPI bus | Acceptable for dev platform IF detection path exists (CRC + plausibility → safe state). Document as CCF in DFA. |
| 1oo2D vs 2oo3 | 1oo2D sufficient when "off" is a safe state. 2oo3 only for fail-operational (autonomous). |
| DMA complexity | Not a valid argument against SPI1+SPI2. Don't overstate complexity to justify shortcuts. |
| Verification margins | Know your margins. 32 µs actual vs 10,000 µs budget = 300x margin. |
| Production upgrade | SPI1 + SPI2 for production. Document as known improvement. |

---

## Open Action Items

| # | Action | Owner | Deadline | Status |
|---|--------|-------|----------|--------|
| 1 | Document shared SPI bus as CCF coupling factor in DFA report | FSE | DFA phase | Open |
