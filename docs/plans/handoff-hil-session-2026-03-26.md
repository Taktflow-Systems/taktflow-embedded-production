# HIL Session Handoff — 2026-03-26

## What Was Done

### Platform HIL Test Suite (new)
4 new test scripts + 11 YAML scenarios added to `test/hil/`:

| File | Purpose | Hops |
|------|---------|------|
| `test_hil_uds.py` | UDS diagnostics: TesterPresent, ReadDID, ECUReset on CVC/FZC/RZC | 16 |
| `test_hil_scheduler.py` | Scheduler timing: mean/std/max_gap, cross-ECU phase diversity | 6 |
| `test_hil_selftest.py` | ECU startup self-test: reset recovery, INIT→RUN, no DTC | 6 |
| `test_hil_wdgm.py` | Watchdog: alive counter, 60s heartbeat soak, no WdgM DTC | 5 |
| YAML `hil_050..064` | 11 scenarios for UDS, scheduler, self-test, watchdog | — |

Bug fix: `test_hil_battery.py` missing `CAN_MOTOR_STATUS` import.

### Pi as vECU Station
Moved vECU station from laptop to Pi (192.168.0.197):

| File | Purpose |
|------|---------|
| `docker/docker-compose.hil-pi.yml` | Minimal compose: BCM/ICU/TCU + plant-sim + MQTT on `can0` |
| `scripts/deploy/deploy-pi.sh` | Rsync repo to Pi, build Docker, bring up `can0`, start containers |
| `scripts/hil/run_hil_pi.sh` | Run all HIL tests on Pi |
| `test/hil/hil_test_lib.py` | Auto-detects Linux → SSH to PC for CVC hardware reset |

Pi setup: Docker installed, `hil-venv` at `~/hil-venv`, system mosquitto on port 1883 (Docker mqtt container not needed — stop it or it crashes with "Address in use").

### SC DCAN TX Fix
**Root cause found and fixed.** Custom `dcan1_transmit()` in `sc_hw_tms570.c` had BE32 byte ordering issues with IF1CMD/IF1DATA register writes. Replaced with HALCoGen's `canTransmit()` which handles byte ordering correctly.

- `firmware/platform/tms570/src/sc_hw_tms570.c`: Added `canTransmit()` extern + `canREG1` define (can't include `HL_can.h` due to `-Werror` on legacy TI macros), replaced `dcan1_transmit()` body.
- SC 0x013 confirmed on bus at 100ms after fix.
- **NEEDS MONITORING**: SC previously went bus-off (ES=0x231) within minutes in normal mode. The `canTransmit()` fix may resolve this since the old stuff errors were likely caused by malformed TX frames. Need 15+ min soak to confirm stability.

### Firmware Rebuilt and Flashed (all 4 ECUs)

| ECU | Board | Probe SN | Binary | CAN ID | Status |
|-----|-------|----------|--------|--------|--------|
| RZC | F413ZH | `0670FF38...` | `build/stm32f4/rzc_f4.bin` | 0x012 | ON BUS |
| FZC | G474RE | `001A0036...` | `build/stm32/fzc.bin` | 0x011 | ON BUS |
| CVC | G474RE | `0027003C...` | `build/stm32/cvc.bin` | 0x010 | ON BUS |
| SC | TMS570 | XDS110 | `build/tms570/sc.elf` | 0x013 | ON BUS (monitoring needed) |

### Bench Docs Updated
- `H:/tmp/taktflow-systems-hil-bench/README.md`: probe-to-board mapping with SN, COM ports, CVC=STM32 (not TMS570), Pi as vECU station, flash commands.

## What Needs To Be Done Next

### Immediate (this bench session)
1. **SC 15-min soak** — monitor 0x013 for bus-off. Run from Pi:
   ```bash
   source ~/hil-venv/bin/activate
   cd ~/taktflow-embedded-production
   python3 -c "
   import can, time
   bus = can.interface.Bus(channel='can0', interface='socketcan')
   start = time.time()
   count = gaps = 0; last_t = max_gap = 0
   while (time.time() - start) < 900:
       msg = bus.recv(timeout=2.0)
       if msg and msg.arbitration_id == 0x013:
           now = time.time(); count += 1
           if last_t > 0:
               g = (now - last_t) * 1000
               if g > max_gap: max_gap = g
               if g > 250: gaps += 1; print(f'GAP {g:.0f}ms at {int(now-start)}s')
           last_t = now
           if count % 300 == 0: print(f'{int(now-start)}s: {count} frames, {gaps} gaps, max={max_gap:.0f}ms')
       elif msg is None:
           gaps += 1; last_t = 0; print(f'TIMEOUT at {int(time.time()-start)}s')
   bus.shutdown()
   print(f'DONE: {count} frames, {gaps} gaps, max_gap={max_gap:.0f}ms')
   print('PASS' if gaps == 0 else 'FAIL')
   "
   ```

2. **Run HIL tests one by one** (user prefers one-by-one, not full suite):
   ```bash
   source ~/hil-venv/bin/activate
   export CAN_INTERFACE=can0 MQTT_HOST=localhost PYTHONPATH=test/hil
   python3 test/hil/test_hil_e2e.py          # PASSED earlier
   python3 test/hil/test_hil_heartbeat.py
   python3 test/hil/test_hil_uds.py
   python3 test/hil/test_hil_scheduler.py
   python3 test/hil/test_hil_wdgm.py
   python3 test/hil/test_hil_selftest.py
   python3 test/hil/test_hil_body.py
   python3 test/hil/test_hil_battery.py
   python3 test/hil/test_hil_overtemp.py
   python3 test/hil/test_hil_vsm.py
   ```
   After running each test, rsync updated results back:
   ```bash
   rsync -avz test/hil/results/ taktflow-pi@192.168.0.197:~/taktflow-embedded-production/test/hil/results/
   ```

3. **Rsync test code to Pi** (new platform tests not yet synced after SC fix):
   ```bash
   cd h:/VS-Taktflow-Systems/taktflow-embedded-production
   rsync -avz ./ taktflow-pi@192.168.0.197:/home/taktflow-pi/taktflow-embedded-production/ \
     --exclude '.git' --exclude 'build/' --exclude '__pycache__' --exclude '.claude/'
   ```

### After Soak Passes
4. **Commit SC fix** — `sc_hw_tms570.c` changes (canTransmit + extern decls)
5. **Commit platform HIL tests** — 4 new test scripts + 11 YAML scenarios + battery import fix
6. **Commit Pi deployment** — docker-compose.hil-pi.yml, deploy-pi.sh, run_hil_pi.sh, hil_test_lib.py
7. **Update HIL test plan** — plan already updated in `docs/plans/plan-hil-test-suite.md` (Phase 6)

### Known Issues
- **CVC stuck in INIT without SC**: CVC requires SC heartbeat (0x013) to transition INIT→RUN. With SC fix, this should now work. If SC goes bus-off again, CVC will be stuck.
- **`taktflow.dbc` vs `taktflow_vehicle.dbc`**: Old DBC still referenced in 22 files. `taktflow_vehicle.dbc` is the correct one. Cleanup deferred.
- **DBC_PATH in Dockerfile.plant-sim**: The Dockerfile `CMD` uses `gateway.plant_sim_py.simulator` which defaults to `taktflow_vehicle.dbc` — correct. The compose env var override also set correctly now.

## Key Learnings (save to lessons-learned)

1. **ST-LINK probe SN must be verified before every flash** — two G474RE boards (CVC + FZC) have different peripherals. Wrong firmware = wrong GPIO = potential hardware damage. Mapping documented in bench README.
2. **TMS570 BE32 byte ordering breaks custom register writes** — HALCoGen's `canTransmit()` uses `IF1DATx[]` byte array with `s_canByteOrder[]` lookup. Custom 32-bit word packing doesn't work on BE32 for IF1CMD. Always use HALCoGen API for DCAN on TMS570.
3. **SC `HIL=1` build flag enables silent mode** — without it, SC runs normal CAN (with TX). The old silent mode was a workaround for the byte-ordering bug, not a real hardware issue.
4. **Pi system mosquitto conflicts with Docker mosquitto** — stop Docker mqtt container or don't include it in compose. System mosquitto on port 1883 is sufficient.

## File Change Summary (uncommitted)

```
Modified:
  firmware/platform/tms570/src/sc_hw_tms570.c     — SC canTransmit fix
  test/hil/hil_test_lib.py                         — Pi auto-detect CVC reset
  test/hil/test_hil_battery.py                     — CAN_MOTOR_STATUS import fix
  docs/plans/plan-hil-test-suite.md                — Phase 6 added

New:
  test/hil/test_hil_uds.py
  test/hil/test_hil_scheduler.py
  test/hil/test_hil_selftest.py
  test/hil/test_hil_wdgm.py
  test/hil/scenarios/hil_050_cvc_uds_tester_present.yaml
  test/hil/scenarios/hil_051_cvc_uds_read_did.yaml
  test/hil/scenarios/hil_052_fzc_uds_vin.yaml
  test/hil/scenarios/hil_053_rzc_uds_motor_did.yaml
  test/hil/scenarios/hil_054_rzc_uds_ecu_reset.yaml
  test/hil/scenarios/hil_055_uds_invalid_did.yaml
  test/hil/scenarios/hil_060_scheduler_cvc_timing.yaml
  test/hil/scenarios/hil_061_scheduler_cross_ecu.yaml
  test/hil/scenarios/hil_062_selftest_startup.yaml
  test/hil/scenarios/hil_063_wdgm_alive_counter.yaml
  test/hil/scenarios/hil_064_wdgm_soak.yaml
  docker/docker-compose.hil-pi.yml
  scripts/deploy/deploy-pi.sh
  scripts/hil/run_hil_pi.sh
```
