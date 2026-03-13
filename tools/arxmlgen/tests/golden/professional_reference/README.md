# Professional Reference Files

These files represent what **Vector DaVinci Developer** (or EB tresos) would generate
for the same BCM ECU data. They are **simulated** — not from an actual DaVinci license —
but follow documented AUTOSAR configurator output patterns.

## Purpose

Quality comparison target for arxmlgen output. Tests verify that arxmlgen-generated
files match the **structural properties** of professional output:

1. Same signal count, bit positions, types, PDU assignments
2. Same PDU count, DLC, cycle times, timeouts
3. Same runnable count, periods, execution order
4. Same CAN ID ↔ PDU ID routing
5. Same aggregate config struct pattern

## What Differs (By Design)

| Aspect | Professional Tool | arxmlgen |
|--------|------------------|----------|
| Naming | `ComConf_ComSignal_*` (ECUC) | `BCM_SIG_*` (project convention) |
| MemMap | `COM_START_SEC_CONST_*` macros | Not generated (platform handles) |
| Memory qualifiers | `VAR()`, `CONST()` macros | Plain `static` / `const` |
| Comment style | `/* PRQA S 5087 */` + Doxygen | `@brief` + inline comments |
| Signal IDs | ECUC-assigned (arbitrary order) | Sorted by name, BSW reserved 0–15 |
| Type system | `Std_ReturnType`, `PduIdType` | `uint8_t`, `uint16_t` |
| File header | Copyright + tool version + timestamp | `GENERATED -- DO NOT EDIT` + tool version |

## What Must Match (Quality Gate)

Tests in `test_quality.py` verify structural equivalence:

- [ ] Signal count matches
- [ ] Every signal has valid bit position + size (no overlap within PDU)
- [ ] PDU ↔ CAN ID mapping is consistent across Com, CanIf, PduR
- [ ] Runnable periods and count match between reference and generated
- [ ] Aggregate config struct has all required fields populated
- [ ] No magic numbers without comments
