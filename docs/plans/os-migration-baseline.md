# OS Migration Baseline (S-OS-00)

Date: 2026-07-07
Branch: `feat/os-osek-migration`
Branch point: `eb56b0b` ("feat(bench): xcp_smoke broadcast request support"),
tip of `hil-tx-debug-2026-04-21`. Note: the migration plan was drafted against
`814b9f7`; three commits (`3a2fc4f`, `1d4a0de`, `eb56b0b` — SC XCP/bench work,
no OS files) landed between plan drafting and branch creation. Plan findings
remain valid; no `firmware/bsw/os/**` or `firmware/platform/**` OS sources
changed in that window.

Host: Windows 11, Git Bash environment, host `gcc` 15.2.0
(x86_64-posix-seh, MinGW-Builds), GNU Make 4.4.1, Python 3.14.

## Suites that ran on this host

### OSEK kernel + port smoke suites (host gcc)

Command (from repo root):

```
make -f firmware/bsw/os/bootstrap/test/Makefile
```

Result: ALL 11 suites PASS — 242 tests, 0 failures, 0 ignored.

Verbatim runner output tail:

```
PASS  test_Os_asild  (58 Tests 0 Failures 0 Ignored)
PASS  test_Os_Interrupt  (12 Tests 0 Failures 0 Ignored)
PASS  test_Os_MemProt  (17 Tests 0 Failures 0 Ignored)
PASS  test_Os_ProtectionHook  (13 Tests 0 Failures 0 Ignored)
PASS  test_Os_ScheduleTable  (13 Tests 0 Failures 0 Ignored)
PASS  test_Os_ServiceProt  (19 Tests 0 Failures 0 Ignored)
PASS  test_Os_TimingProt  (12 Tests 0 Failures 0 Ignored)
PASS  test_Os_Conformance_Sched  (14 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Tms570_bootstrap  (41 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap  (23 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32L5_bootstrap  (20 Tests 0 Failures 0 Ignored)
```

Per-suite raw outputs are written by the runner to
`build/os-tests/test_*.out` (not committed).

## Suites that could NOT run on this host

- `make test` from repo root: there is no root `Makefile`/`GNUmakefile`
  (only `Makefile.pipeline`, which has codegen step targets, no `test`
  target). Running `make test` prints "Nothing to be done for 'test'"
  because GNU make, with no makefile, treats the existing `test/`
  directory as an up-to-date file target. The documented `make test`
  entry point exists only inside the Docker/Linux SIL environment.
  NOT RUN on this Windows host per plan instruction (do not spin up
  Docker for baseline).
- POSIX SIL build (`make -f firmware/platform/posix/Makefile.posix build`)
  and `test/sil/` scenario suite: the POSIX MCAL uses SocketCAN
  (`Can_Posix.c`, vcan) which is Linux-only. Must be captured in
  CI/Docker; recorded as a baseline gap to be closed before S-OS-20
  frame-parity comparisons (which need a Linux candump capture anyway).
- STM32 (`Makefile.stm32`) and TMS570 (`Makefile.tms570`) target builds:
  cross-toolchains not exercised for baseline; these makefiles compile
  no OS sources at HEAD, so the kernel suites above are the relevant OS
  baseline.

## Baseline interpretation

- The 8 kernel suites + 3 port smoke suites (Tms570/Stm32/Stm32L5
  bootstrap) at the branch point are 100% green. Any post-harvest
  (S-OS-01) failure in these suites is a regression introduced by the
  harvest, not pre-existing.
- SIL scenario baseline (frame sets/periods per ECU) is deferred to the
  Linux CI environment; S-OS-20 acceptance requires capturing it there
  before the BCM migration lands.
