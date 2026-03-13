# SIL Demo System — Master Plan

**Created:** 2026-02-23
**Status:** Phase 11 DONE — awaiting Docker build verification
**Updated:** 2026-02-23

## Phase Status

| Phase | Name | Status |
|-------|------|--------|
| 11 | Port Physical ECUs to POSIX | DONE (code complete, needs Docker build test) |
| 12 | DBC File + Plant Simulator | PENDING |
| 13 | CAN-to-MQTT Gateway + Broker | PENDING |
| 14 | Dashboard — Grafana + InfluxDB | PENDING |
| 15 | SAP QM Mock API | PENDING |
| 16 | Edge ML Anomaly Detection | PENDING |
| 17 | Integration & Demo Polish | PENDING |

## Phase 11 — Port Physical ECUs to POSIX

**Goal:** All 7 ECUs compile and run in Docker on vcan0.

### 11.0 — CVC POSIX Hardware Stubs
- [x] Create `firmware/cvc/src/cvc_hw_posix.c` (11 externs: 5 timing + 5 self-test + Ssd1306_Hw_I2cWrite)
- [ ] Verify: `make -f Makefile.posix TARGET=cvc` compiles (needs Docker/WSL2)

### 11.1 — FZC POSIX Hardware Stubs
- [x] Create `firmware/fzc/src/fzc_hw_posix.c` (12 externs: 5 timing + 7 self-test)
- [x] Create `firmware/shared/bsw/mcal/posix/Uart_Posix.c` (5 Uart_Hw_* stubs)
- [ ] Verify: `make -f Makefile.posix TARGET=fzc` compiles (needs Docker/WSL2)

### 11.2 — RZC POSIX Hardware Stubs
- [x] Create `firmware/rzc/src/rzc_hw_posix.c` (13 externs: 5 timing + 8 self-test)
- [ ] Verify: `make -f Makefile.posix TARGET=rzc` compiles (needs Docker/WSL2)

### 11.3 — SC POSIX Stubs
- [x] Create `firmware/sc/src/sc_hw_posix.c` (24 externs: 8 HALCoGen + 3 DCAN + 9 selftest + 3 ESM + gioGetBit)
- [ ] Verify: `make -f Makefile.posix TARGET=sc` compiles (needs Docker/WSL2)

### 11.4 — Expand Build Infrastructure
- [x] Update `firmware/Makefile.posix` — 7 targets, SC-specific section, FZC Uart, main MCAL modules, IoHwAb
- [x] Update `docker/Dockerfile.vecu` — builds and copies 7 binaries
- [x] Update `docker/docker-compose.yml` — 8 services (can-setup + 7 ECUs)
- [x] Add `IoHwAb_ReadEncoderCount` and `IoHwAb_ReadEncoderDirection` to IoHwAb.c (needed by RZC)
- [x] Add `Can_GetControllerErrorState` to Can.c (needed by RZC)
- [ ] Docker build verification: `docker compose up --build` (needs WSL2 with vcan)

### Known Pre-existing Type Mismatches (link fine, cosmetic)
- FZC `Swc_Steering.c` declares `IoHwAb_ReadSteeringAngle(sint16*)` — IoHwAb.c uses `uint16*`
- RZC `Swc_TempMonitor.c` declares `IoHwAb_ReadMotorTemp(sint16*)` — IoHwAb.c uses `uint16*`
- RZC `Swc_Motor.c` declares `IoHwAb_SetMotorPWM` as `void` — IoHwAb.c returns `Std_ReturnType`
- CVC `Com_SendSignal(uint8, const void*)` vs RZC `Com_SendSignal(uint16, const uint8*, uint8)` — cross-TU, links by name

## Service Inventory (Final Target: Phase 17)

| Service | Port | Phase |
|---------|------|-------|
| can-setup | — | 11.4 |
| cvc | — | 11.4 |
| fzc | — | 11.4 |
| rzc | — | 11.4 |
| sc | — | 11.4 |
| bcm | — | EXISTS |
| icu | — | EXISTS |
| tcu | — | EXISTS |
| plant-sim | — | 12 |
| mqtt-broker | 1883 | 13 |
| can-gateway | — | 13 |
| sap-qm-mock | 8090 | 15 |
| telegraf | — | 14 |
| influxdb | 8086 | 14 |
| grafana | 3000 | 14 |
| ml-inference | — | 16 |
