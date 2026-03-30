# Lessons Learned Policy

Write lessons to the centralized repo: `H:/VS-Taktflow-Systems/taktflow-lessons-learned/`

Categories: `embedded/can/`, `embedded/isr/`, `platform/stm32/`, `platform/tms570/`, `platform/posix/`, `safety/`, `toolchain/gcc-arm/`

## When to CHECK existing lessons

1. **User asks** — "check lessons", "any known issues with X", "have we seen this before"
2. **Debugging is spinning** — 5+ minutes or 5+ tool calls on the same bug without resolution
3. **Planning phase** — when proposing an approach for embedded/platform/safety work

How to check: read the `rules:` arrays from .md files in the matching category folder. Mention any matching rule before continuing.

## When to WRITE a new lesson

After fixing a bug, BEFORE moving on, when any of these are true:
1. Bug took > 5 minutes or > 5 tool calls to find
2. You changed a register address/value to fix something
3. You discovered a platform difference (STM32 vs TMS570 vs POSIX)
4. You (Claude) suggested a wrong step that was corrected
5. Same class of bug appeared for the second time

Use YAML frontmatter from `templates/lesson-template.md`. The `rules:` array is critical — each entry is one atomic sentence: "Always X when Y" or "Never X because Y".
