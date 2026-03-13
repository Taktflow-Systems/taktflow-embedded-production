# Naming Conventions

## Code
- **BSW modules**: PascalCase (`Com_SendSignal`, `CanIf_Transmit`)
- **SWC functions**: `Swc_<Module>_<Action>` (`Swc_Steer_Main`, `Swc_Brake_Init`)
- **Config defines**: `<ECU>_<CATEGORY>_<NAME>` (`FZC_SIG_STEER_CMD`, `CVC_COM_TX_HEARTBEAT`)
- **Types**: `<Module>_<Type>Type` (`Com_SignalIdType`, `Dem_EventIdType`)
- **Constants**: UPPER_SNAKE_CASE
- **Local variables**: camelCase
- **File names**: PascalCase for modules (`Com_Cfg.c`), snake_case for tools (`swc_extractor.py`)

## Git
- Branches: `feat/`, `fix/`, `hotfix/`, `release/`
- Commits: Conventional Commits, imperative mood
- Tags: `vX.Y.Z` (semver)

## Docs
- Plans: `docs/plans/plan-<feature-name>.md`
- Safety: `docs/safety/<iso-part>/<document>.md`
