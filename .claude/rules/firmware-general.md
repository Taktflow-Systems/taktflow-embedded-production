# Firmware Rules

## Platform Abstraction
- Same SWC source compiles on STM32, TMS570, and POSIX (Docker SIL)
- Platform differences handled in `firmware/platform/*/` MCAL implementations
- Use `#ifdef PLATFORM_POSIX` only in MCAL/platform layer, never in SWC code
- HAL for all hardware access. Pin mappings in config, not source.

## Vendor Independence
Wrap vendor SDKs in abstraction — swap = 1-file change. Vendor code in `firmware/lib/vendor/`.

## OTA Updates
Signed (Ed25519/ECDSA) + verified BEFORE flash. A/B partitions. Anti-rollback.
Never install partial/unsigned images. Auto-rollback after N failed boots.

## Safety Controller (SC)
TMS570 runs lockstep — NO AUTOSAR BSW stack. Minimal, auditable code only.
