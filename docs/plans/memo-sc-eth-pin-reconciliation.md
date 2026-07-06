# Memo: SC Ethernet Pin Reconciliation

Date: 2026-07-06
Step: S-ETH-02

## Decision

Keep the default SC safety build pin map unchanged:

- CVC fault LED stays on GIOA[1].
- FZC fault LED stays on GIOA[2].
- RZC fault LED stays on GIOA[3].
- System fault LED stays on GIOA[4].
- WDI stays on GIOA[5].

When `SC_ETH_ENABLE` is defined, reserve GIOA[3] and GIOA[4] for the
DP83630 PHY and move only the conflicting LED assignments:

- RZC fault LED moves to GIOB[6].
- System fault LED moves to GIOB[7].
- The GIOB[6]/[7] debug heartbeat blink in `sc_main.c` is disabled in
  Ethernet builds so those two pins have only one owner.

`Sc_Eth_Init()` still drives GIOA[3] and GIOA[4] high before software PHY
wake. GIOA[3] high is harmless after `MICR.INT_OE` disables the PHY pin's
power-down function; GIOA[4] high releases PHY reset.

## GIOA Ownership

| GIO bit | Default SC owner | Ethernet build owner | Source reference |
| --- | --- | --- | --- |
| GIOA[0] | Kill relay output | Kill relay output | `SC_PORT_RELAY` / `SC_PIN_RELAY` in `firmware/ecu/sc/include/Sc_Hw_Cfg.h`; configured in `firmware/ecu/sc/src/sc_main.c` |
| GIOA[1] | CVC fault LED | CVC fault LED | `SC_PORT_LED_CVC` / `SC_PIN_LED_CVC` in `firmware/ecu/sc/include/Sc_Hw_Cfg.h`; used by `firmware/ecu/sc/src/sc_led.c` and `firmware/ecu/sc/src/sc_heartbeat.c` |
| GIOA[2] | FZC fault LED | FZC fault LED | `SC_PORT_LED_FZC` / `SC_PIN_LED_FZC` in `firmware/ecu/sc/include/Sc_Hw_Cfg.h`; used by `firmware/ecu/sc/src/sc_led.c` and `firmware/ecu/sc/src/sc_heartbeat.c` |
| GIOA[3] | RZC fault LED | DP83630 PWRDOWN/INTN | `SC_PIN_ETH_PHY_PWRDOWN` in `firmware/ecu/sc/include/Sc_Hw_Cfg.h`; PHY fact in `firmware/ecu/sc/test/test_eth_ping.c`; release mask in `firmware/ecu/sc/src/sc_eth.c` |
| GIOA[4] | System fault LED | DP83630 RESET_N | `SC_PIN_ETH_PHY_RESET_N` in `firmware/ecu/sc/include/Sc_Hw_Cfg.h`; PHY fact in `firmware/ecu/sc/test/test_eth_ping.c`; release mask in `firmware/ecu/sc/src/sc_eth.c` |
| GIOA[5] | TPS3823 WDI | TPS3823 WDI | `SC_PORT_WDI` / `SC_PIN_WDI` in `firmware/ecu/sc/include/Sc_Hw_Cfg.h`; configured in `firmware/ecu/sc/src/sc_main.c` |
| GIOA[6] | Unassigned by SC firmware | Unassigned by SC firmware | `SC_GIO_PORT_A` pin range in `firmware/ecu/sc/include/Sc_Hw_Cfg.h`; no `SC_PIN_*` assignment |
| GIOA[7] | Unassigned by SC firmware | Unassigned by SC firmware | `SC_GIO_PORT_A` pin range in `firmware/ecu/sc/include/Sc_Hw_Cfg.h`; no `SC_PIN_*` assignment |

## Applied Diff Summary

- Added explicit port macros in `Sc_Hw_Cfg.h` for relay, LED, WDI, and PHY
  ownership.
- Made `SC_PIN_LED_RZC` and `SC_PIN_LED_SYS` conditional on
  `SC_ETH_ENABLE`.
- Updated SC LED call sites to use `SC_PORT_LED_*` macros instead of
  hard-coded `SC_GIO_PORT_A`.
- Disabled the GIOB[6]/[7] debug heartbeat blink when `SC_ETH_ENABLE` is set.

## Verification Results

- Default build: `make -f firmware/platform/tms570/Makefile.tms570 all`
  passes, and the default map reports no `Sc_Eth` or `sc_eth` symbols.
- Ethernet compile check: the changed SC sources compile with
  `SC_ETH_ENABLE` defined under the TMS570 compiler flags, proving the
  alternate pin map compiles before S-ETH-03 wires the full application
  variant.
- Bench boot check: the default SC image flashes successfully and emits the
  expected boot, BIST pass, and relay-monitoring serial text. Default LED
  pin macros are unchanged, so default LED behavior is preserved by the
  applied diff.
