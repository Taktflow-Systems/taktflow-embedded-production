# Lessons Learned: HIL SC E2E Validation + CVC RUN State

**Date:** 2026-03-23
**Context:** HIL mixed-bench bringup — getting SC relay energized with real E2E and CVC to RUN state.

---

## 1. SCI1 UART Dead — PINMUX Not Configured

**Mistake:** Assumed HALCoGen's `muxInit()` configured ball A5 for LIN1TX. It was set to GIOA[0] (GPIO). SCI1 TX signal was trapped inside the chip — never reached the XDS110 backchannel UART on COM11.

**Fix:** Override IOMM PINMUX83 in `sc_sci_init()` after `systemInit()`: unlock IOMM → write 0x02020202 (LIN1TX on A5) → lock IOMM. Also moved `sc_sci_init()` after `systemInit()` because pre-PLL VCLK1=8MHz gives wrong baud rate (BRS=40 → ~12k instead of 115200).

**Principle:** Always verify pin mux for peripherals you configure manually. HALCoGen's `.dil` file is the source of truth for what `muxInit()` sets. If the peripheral isn't enabled in HALCoGen, the pin won't be muxed.

---

## 2. E2E Was Never Broken — DCAN Byte Order Is Correct

**Mistake:** Suspected TMS570 big-endian DCAN byte order mismatch. Added a PLATFORM_HIL bypass. Wasted time on a non-issue.

**Fix:** SCI debug showed all 6 mailboxes pass CRC-8 on the first try. The TX and RX byte extraction are symmetric (`data_a & 0xFF = byte0` on both paths). If TX works on the wire, RX works too.

**Principle:** When TX and RX share the same byte-packing code and TX is confirmed working (frames visible on bus sniffer), RX byte order is almost certainly correct. Don't add bypasses for unverified suspicions — get debug output first.

---

## 3. Ball A5 Shared Between Relay GPIO and SCI1 TX

**Mistake:** `SC_PIN_RELAY = GIOA[0]` on ball A5 — the same pin reassigned to LIN1TX for SCI debug. GPIO readback mismatch killed the relay after 2 ticks (20ms).

**Fix:** `#ifndef PLATFORM_HIL` guard around the readback check in `sc_relay.c`. No physical relay on the LaunchPad anyway.

**Principle:** Check pin assignments before changing PINMUX. Use `grep SC_PIN_RELAY` to find conflicts. On development boards without production hardware, guard hardware-specific checks.

---

## 4. Verbose SCI Output Blocks the 10ms Main Loop

**Mistake:** The 5s periodic debug dump printed ~25 SCI strings (CCM/ESM registers, hex32 values). At 115200 baud, this blocked the main loop for ~50ms. During that time, DCAN mailboxes were overwritten multiple times, causing E2E alive counter gaps → e2e_failed latch → relay kill at grace expiry.

**Fix:** `#ifdef PLATFORM_HIL` minimal output in `sc_hw_debug_periodic()` — just a newline. Also removed the E2E debug hex dump after diagnosis was complete.

**Principle:** On bare-metal 10ms loops, every SCI character costs ~87μs. A 50-character string takes ~4ms. A full register dump can take 30-50ms — enough to miss multiple CAN heartbeats. Use volatile debug counters (`g_dbg_*`) instead of SCI prints for runtime monitoring.

---

## 5. TMS570 LaunchPad DCAN TX Causes Bus-Off

**Mistake:** SC_Status TX frames fail on LAUNCHXL2-570LC43 (no ACK → TX error counter climbs → error passive at ~15s → bus-off at ~30s). Once bus-off, ALL RX stops → heartbeat timeout → relay kill.

**Fix:** `#ifndef PLATFORM_HIL` guard around bus-off and bus-silence relay kill triggers. Root cause likely: DCAN1TX pin mux not configured for CAN transceiver, or transceiver standby not released. Investigation pending.

**Principle:** CAN bus-off is fatal for a listen-only node because it stops RX too, not just TX. If a node doesn't need to TX, either configure it in silent/listen-only mode or don't configure TX mailboxes at all. On dev boards, verify the CAN transceiver TX path is actually connected.

---

## 6. CVC Defaults to relay=OFF (Fail-Closed) — Wrong for HIL

**Mistake:** `sc_relay_kill` in CVC defaults to 0 (killed/fail-closed). CVC reads SC_Status for the actual value. But SC goes bus-off before CVC receives SC_Status → CVC stays at default 0 → EVT_SC_KILL → SHUTDOWN.

**Fix:** `#ifdef PLATFORM_HIL` default to 1 (energized). Also added 15s `CVC_POST_INIT_GRACE_CYCLES` for HIL to absorb SC startup delay.

**Principle:** Fail-closed defaults are correct for production but break HIL benches where boot timing isn't simultaneous. Use platform-specific defaults for safety signals that depend on inter-ECU communication timing.

---

## 7. DCAN Silent Mode Creates Cascading Failures

**Context:** SC_Status TX fails on LaunchPad → bus-off → all RX dies. Fix: DCAN silent mode (RX only). But silent mode means SC_Status (0x013) never reaches CVC.

**Cascade:** CVC Com layer reads SC_Status signal via Rte_Read. No update → Com reports TIMEOUT. CVC interprets this as SC CAN failure → CAN_TIMEOUT_SINGLE → SAFE_STOP. Additionally, `sc_relay_kill` stays at init=0 → EVT_SC_KILL → SHUTDOWN.

**Workarounds applied:** `#ifndef PLATFORM_HIL` around SC_KILL check, `sc_relay_kill` default to 1. But CAN timeout on SC still triggers SAFE_STOP.

**Root cause:** LaunchXL2-570LC43 DCAN1TX transceiver path not functional. Likely: DCAN1TX pin mux not set in HALCoGen, or CAN transceiver standby pin not released. Must be fixed in HALCoGen `.dil` / pin configuration — cannot be worked around with silent mode indefinitely.

**Principle:** Silent mode is a diagnostic tool, not a production workaround. If a CAN node needs to TX, fix the TX path. Every node that depends on the missing TX will cascade-fail.

---

## 8. Boot Order Is Everything on HIL

**Context:** SC heartbeat timeout and CVC SHUTDOWN are both permanent latches (power-cycle only). This means boot order must ensure all dependencies are met at the exact right time.

**Working boot order:**
1. Flash RZC first (bus empty — avoids F413 TX pin glitch)
2. Flash FZC
3. Start Docker vECUs + CAN bridge on laptop
4. Flash SC **last** (all heartbeats already on bus)
5. Wait 12s for SC relay to energize
6. Flash CVC (SC_Status on bus, relay=ON)

**Principle:** On a bench with permanent-latch safety states, the most critical node (SC) must boot last when all its inputs are satisfied. The node that depends on the most outputs (CVC) must boot after all outputs are available.

---

## 9. DCAN TX Needs IFCMD_NEWDAT — MCTL.TxRqst Is Read-Only via IF

**Mistake:** `dcan1_transmit()` set TxRqst in MCTL (bit 8) and transferred MCTL via `DCAN_IFCMD_CONTROL`. Expected the DCAN hardware to start transmission. Frames never appeared on the bus.

**Root cause:** MCTL.TxRqst is **read-only** when accessed through the IF1 register path. The only way to set TxRqst in the message object is via `DCAN_IFCMD_NEWDAT` (bit 18) in the IF1CMD register with WR=1. HALCoGen's `canTransmit()` uses `IF1CMD = 0x87` which includes bit 2 (= NEWDAT in the byte-access layout). The custom code was missing this bit.

**Fix:** Added `DCAN_IFCMD_NEWDAT` to the IF1CMD write in `dcan1_transmit()`.

**Principle:** When porting register-level CAN driver code, compare your IF1CMD bitmask against the vendor HAL's `canTransmit()` bit-for-bit. The DCAN IF command register has subtle semantics — some MCTL fields are only settable via specific IFCMD flags, not via the MCTL transfer.

---

## 10. Loose Transceiver VCC Wire Causes Intermittent Bus-Off

**Symptom:** DCAN TX works for 3-6 minutes then goes bus-off. RX survives longer. Looks like a firmware timing bug but is actually hardware.

**Root cause:** Loose VCC jumper wire to SN65HVD230 CAN transceiver. The TX driver needs stable power to drive CANH/CANL differential (~60mA). A loose connection causes momentary power drops → TX driver goes high-impedance mid-frame → bit error → no ACK → TX error counter climbs → bus-off. RX comparator draws less current and survives brief dropouts.

**Fix:** Rewired VCC with a solid connection. System stable for 15+ minutes with no bus-off.

**Principle:** Before debugging CAN TX firmware, always verify transceiver power with a multimeter. Intermittent bus-off that takes minutes to appear is almost always a hardware issue (loose wire, bad solder joint, insufficient decoupling). Firmware bugs cause immediate, reproducible failures.
