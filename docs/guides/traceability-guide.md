# Traceability Guide

How to use `scripts/trace-gen.py` to generate and maintain the full V-model traceability matrix.

## What It Does

`trace-gen.py` scans all requirement documents, source code, and test files to build a complete bidirectional traceability graph from stakeholder requirements (STK) down to test results. It:

1. **Parses 8 levels of requirements** from markdown docs (STK, SYS, SG, FSR, TSR, SSR, HSR, SWR)
2. **Scans firmware source code** for `@safety_req` and requirement ID references
3. **Scans test files** for `@verifies` tags (unit, integration, SIL)
4. **Builds a bidirectional graph** resolving all parent/child links
5. **Validates links** detecting broken references, orphans, and untested leaf requirements
6. **Generates** `docs/aspice/traceability/traceability-matrix.md`

## How to Run

```bash
# Generate the traceability matrix (overwrites the existing file)
python scripts/trace-gen.py

# CI mode — exit 1 if broken links or untested leaf requirements exist
python scripts/trace-gen.py --check

# Print summary statistics only (no file output)
python scripts/trace-gen.py --stats

# Output as machine-readable JSON
python scripts/trace-gen.py --json
python scripts/trace-gen.py --json --output path/to/output.json

# Custom output path for the markdown matrix
python scripts/trace-gen.py --output path/to/matrix.md
```

No pip dependencies — pure Python stdlib.

## Requirement Levels

The script parses these documents in order:

| Level | Document | ID Pattern | Example |
|-------|----------|------------|---------|
| STK | `docs/aspice/system/stakeholder-requirements.md` | `STK-NNN` | `STK-001` |
| SYS | `docs/aspice/system/system-requirements.md` | `SYS-NNN` | `SYS-042` |
| SG | `docs/safety/concept/safety-goals.md` | `SG-NNN` | `SG-001` |
| FSR | `docs/safety/requirements/functional-safety-reqs.md` | `FSR-NNN` | `FSR-015` |
| TSR | `docs/safety/requirements/technical-safety-reqs.md` | `TSR-NNN` | `TSR-033` |
| SSR | `docs/safety/requirements/sw-safety-reqs.md` | `SSR-{ECU}-NNN` | `SSR-CVC-007` |
| HSR | `docs/safety/requirements/hw-safety-reqs.md` | `HSR-{ECU}-NNN` | `HSR-RZC-003` |
| SWR | `docs/aspice/software/sw-requirements/SWR-*.md` | `SWR-{ECU}-NNN` | `SWR-BCM-012` |

## How Trace Tags Work

### In requirement documents

Each requirement heading is followed by a bulleted property block:

```markdown
### SYS-001: Dual Pedal Position Sensing

- **Traces up**: STK-005, STK-016
- **Traces down**: TSR-001, TSR-002, SWR-CVC-001
- **Safety relevance**: ASIL D
- **Status**: draft
```

The script extracts `**Traces up**` and `**Traces down**` fields to build parent/child links. For Safety Goals, `**Source**: HE-NNN` is used instead of `**Traces up**`.

### In source code (`@safety_req`)

```c
/**
 * @file    Swc_Motor.c
 * @safety_req SWR-RZC-001: Motor PWM control
 * @safety_req SWR-RZC-002: Motor thermal protection
 */
```

The script scans all `.c` and `.h` files under `firmware/` (excluding test directories) for `@safety_req` tags and bare `SWR-*/SSR-*` references.

### In test files (`@verifies`)

```c
/** @verifies SWR-CVC-001 */
void test_Swc_Pedal_init_spi(void) {
    // test body
}
```

File-level tags can list multiple IDs:

```c
/**
 * @verifies SWR-TCU-002, SWR-TCU-003, SWR-TCU-004
 */
```

### In SIL scenario YAML files

```yaml
verifies:
  - "SWR-FZC-005"
  - "SWR-SC-001"
aspice: "SWE.5"
```

### Scanned locations

| Type | Glob Pattern | Tag |
|------|-------------|-----|
| Unit tests | `firmware/**/test_*.c` | `@verifies` |
| Integration tests | `test/integration/test_int_*.c` | `@verifies` |
| SIL scenarios | `test/sil/scenarios/*.yaml` | `verifies:` list |
| Source code | `firmware/**/*.c`, `firmware/**/*.h` (excl. test/) | `@safety_req`, bare IDs |

## Adding a New Requirement

1. **Add the requirement** to the appropriate markdown document with the correct heading format and trace fields
2. **Add `@verifies` tag** to the test file that covers it
3. **Add `@safety_req` tag** to the source file that implements it (for SSR/SWR)
4. **Run `python scripts/trace-gen.py --stats`** to verify the new requirement appears and is traced correctly
5. **Run `python scripts/trace-gen.py`** to regenerate the matrix

Example — adding a new SWR requirement:

```markdown
### SWR-CVC-999: New Feature Description

- **ASIL**: D
- **Traces up**: SYS-042, TSR-033, SSR-CVC-050
- **Traces down**: firmware/cvc/src/new_module.c:New_Function()
- **Verified by**: TC-CVC-999
- **Status**: draft
```

Then in the test file:
```c
/** @verifies SWR-CVC-999 */
void test_new_module_nominal(void) { ... }
```

## Reading the Gap Analysis

The matrix output includes a **Gap Analysis** section with:

### Broken Links
Requirement IDs referenced in `**Traces up**` or `**Traces down**` that don't exist in any document. These must be fixed — either the reference is wrong or the target requirement is missing.

### Orphan Requirements
Requirements with no parent AND no child (disconnected from the V-model chain). STK and SG are exempt since they are top-level.

### Untested Requirements
SSR and SWR requirements without any `@verifies` tag in test files. These are compliance gaps — every leaf requirement needs test coverage.

### Asymmetric Links
Cases where A traces down to B, but B doesn't trace up to A (or vice versa). These are warnings — the link exists in one direction but not both.

### ASIL Consistency Warnings
Cases where a child requirement has a lower ASIL than its parent. This may be intentional (ASIL decomposition) but should be reviewed.

## CI Integration

Add to your CI pipeline:

```yaml
- name: Traceability check
  run: python scripts/trace-gen.py --check
```

This will fail the build if:
- Any broken links exist (requirement references a non-existent ID)
- Any SSR/SWR requirements lack test coverage

## Coverage Metrics

| Level | Coverage means |
|-------|---------------|
| STK | % with at least one traced-down child |
| SYS | % with both traced-up parent AND traced-down child |
| SG | % with at least one traced-down child (FSR) |
| FSR | % with both traced-up (SG) AND traced-down (TSR) |
| TSR | % with both traced-up (FSR) AND traced-down (SSR/HSR) |
| SSR | % with test coverage (`@verifies` in test files) |
| HSR | % with test coverage (hardware — typically 0%) |
| SWR | % with test coverage (`@verifies` in test files) |

## Relationship to gen-traceability.sh

`gen-traceability.sh` covers the **bottom half** of the V-model: SWR -> source code -> tests. It remains useful for quick SWR-only checks.

`trace-gen.py` covers the **full V-model**: STK -> SYS -> SG -> FSR -> TSR -> SSR/HSR -> SWR -> source -> tests. It supersedes the manual traceability matrix.

Both can coexist. For full compliance audits, use `trace-gen.py`.
