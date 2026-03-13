# Process Playbook — Carried from Taktflow Systems

Web project patterns applied to embedded.

## Planning Discipline
Plan → approve → implement. Update plan before next phase. Ask before coding.

## Phase-Based Execution
Numbered phases with DONE criteria. Status table: PENDING → IN PROGRESS → DONE.

## Security-First Mindset
Fail-closed. Generic errors. Validate at boundaries. Graceful degradation. No hardcoded secrets.

## Claim Accuracy
Audit claims against code. Use qualifiers. Release gate.

## TODO Markers
Greppable: `TODO:SCALE`, `TODO:POST-BETA`, `TODO:HARDWARE`. Full list in `workflow.md`.

## Vendor Independence
Prefer generic protocols over vendor SDKs. Wrap unavoidable SDKs in abstraction — swap = 1-file change.
