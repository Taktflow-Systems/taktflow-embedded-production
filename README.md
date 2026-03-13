# Taktflow Embedded Production

ISO 26262 ASIL D zonal vehicle platform — 7 ECUs, AUTOSAR-like BSW, CAN 500k, E2E protection.

## Architecture

| ECU | Role | Target | ASIL |
|-----|------|--------|------|
| CVC | Central Vehicle Controller | STM32F407 | D |
| FZC | Front Zone Controller (steering, brake, lidar) | STM32F407 | D |
| RZC | Rear Zone Controller (rear motor, sensors) | STM32F407 | C |
| SC  | Safety Controller (lockstep watchdog) | TMS570LS0432 | D |
| BCM | Body Control Module | Docker (SIL) | QM |
| ICU | Instrument Cluster Unit | Docker (SIL) | QM |
| TCU | Transmission Control Unit | Docker (SIL) | B |

## Quick Start

```bash
# SIL simulation (all 7 ECUs in Docker)
docker compose -f docker/docker-compose.sil.yml up

# Build for POSIX (host testing)
make -f firmware/platform/posix/Makefile.posix all

# Regenerate configs from DBC
pip install -r tools/arxml/requirements.txt
python tools/arxml/dbc2arxml.py gateway/taktflow.dbc arxml/
python tools/arxml/codegen.py model/ecu_model.json generated/
```

## Project Structure

```
firmware/
  bsw/              Shared AUTOSAR-like BSW stack
    mcal/            MCAL: Can, Spi, Adc, Pwm, Dio, Gpt, Uart
    ecual/           ECUAL: CanIf, PduR, IoHwAb
    services/        Com, Dcm, Dem, E2E, WdgM, BswM, NvM, SchM, Det, CanTp
    rte/             Runtime Environment
    os/              OS abstraction
  ecu/               Per-ECU application SWCs and generated configs
  platform/          Platform-specific MCAL + build systems
  lib/vendor/        Wrapped third-party libraries
gateway/             Edge gateway services
docker/              SIL/HIL container orchestration
test/                xIL testing (SIL, HIL, MIL, PIL)
tools/               ARXML tooling, codegen, CI, MISRA
docs/                Safety (ISO 26262), ASPICE, plans, reference
hardware/            Schematics, pin maps, BOM
```

## Key Principles

1. **DBC is truth** — all CAN config generated from `gateway/taktflow.dbc`
2. **Generate, don't copy** — `ecu/*/cfg/` files are machine-generated
3. **Platform abstraction** — same SWC code on STM32, TMS570, and POSIX
4. **Fail-closed safety** — all faults transition to safe state
5. **Vendor independence** — wrap SDKs, prefer generic protocols

## License

Copyright Taktflow Systems 2026. All rights reserved.
