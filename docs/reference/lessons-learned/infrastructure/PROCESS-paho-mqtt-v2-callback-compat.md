# Lessons Learned — paho-mqtt v2.x Callback Signature Incompatibility

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Date:** 2026-03-03
**Scope:** SIL fault-inject test runner, MQTT callback compatibility, version pinning strategy

## Context

The fault-inject container's test runner subscribes to CAN gateway MQTT topics (via paho-mqtt) to monitor vehicle state during scenario execution. The runner waits for the CVC ECU to transition from INIT → RUN before injecting faults. The `_on_message` callback is registered using `message_callback_add()` with `CallbackAPIVersion.VERSION2`.

paho-mqtt v2.0.x and v2.1.x have different callback signatures for VERSION2. The requirements.txt specified `paho-mqtt>=2.0`, allowing any 2.x version to be installed at runtime.

## Mistake

1. **Callback signature mismatch (v2.1.0 vs v2.0.x)**
   - paho-mqtt 2.0.x passes 3 args to VERSION2 callbacks: `(client, userdata, msg)`
   - paho-mqtt 2.1.0 passes 4 args: `(client, userdata, msg, properties)`
   - The `_on_message` callback only defined 3 parameters, matching v2.0.x
   - When v2.1.0 was installed, the callback signature was incompatible

2. **Silent callback failure**
   - paho-mqtt wraps callback invocations in try-except, catching signature mismatch errors
   - No exception was logged to stdout/stderr
   - The callback simply never executed; MQTT messages were silently dropped
   - The test runner received zero vehicle state updates

3. **Result: cascade failure**
   - `_wait_for_run()` timed out waiting for CVC to reach RUN state
   - Test runner reported "CVC did not reach RUN" for all 8 scenarios
   - Root cause was invisible — no error messages, only silent callback failure
   - CVC was actually reaching RUN (verified via CAN trace), but monitor never saw the MQTT updates

4. **Secondary issue: state cleanup order**
   - Monitor state was reset BEFORE container restart
   - CVC's old boot cycle had published `VehicleState=1` (INIT) with `retain=True`
   - After container restart, monitor's `_wait_for_run()` immediately saw the retained INIT message from the previous cycle
   - Monitor incorrectly concluded CVC was still in INIT, timing out again

## Fix

1. **Update callback signature to accept optional properties parameter**
   ```python
   def _on_message(client, userdata, msg, properties=None):
       # Now compatible with both paho-mqtt 2.0.x and 2.1.0+
   ```

2. **Pin exact major.minor version in requirements.txt**
   ```
   paho-mqtt==2.0.0  # Changed from >=2.0
   ```
   This prevents automatic upgrade to 2.1.0 with breaking callback signature.

3. **Reset monitor AFTER container restart**
   - Changed cleanup order: `_reset()` (container restart) → `monitor.reset()` (clear stale state)
   - Ensures retained MQTT messages from the old boot cycle are cleared before polling for new state

4. **Added on_connect callback for subscription resilience**
   ```python
   def _on_connect(client, userdata, connect_flags, reason_code, properties):
       client.subscribe(...)  # Re-subscribe on reconnect
   ```
   Ensures subscriptions survive broker restarts and connection interruptions.

5. **Added temporary debug logging during callback execution**
   - Logs each MQTT message received to verify callback is actually firing
   - Helped confirm the root cause (callback never called at all)

## Principles

1. **Always add default parameter to paho-mqtt VERSION2 callbacks** — Future-proof for v2.1+ by accepting optional `properties` parameter: `def callback(client, userdata, msg, properties=None):`

2. **Pin exact major.minor versions in requirements.txt for MQTT libraries** — Use `paho-mqtt==2.0.0`, not `>=2.0`, to prevent breakage from minor version bumps that change callback signatures.

3. **Silent callback failures are catastrophic — add verification logging** — When callbacks fail silently, add temporary stdout/stderr logging to verify the callback is being invoked. This catches signature mismatches and other silent failures early.

4. **Container restart order matters for retained MQTT state** — Clear stale retained state AFTER container restart, not before. Resetting monitors before restart allows old retained messages to be seen by new monitor instances. Proper order: restart container → wait for new boot → reset monitor → verify new state.

5. **Understand MQTT retain semantics in multi-boot scenarios** — Any message published with `retain=True` persists across broker restarts and will be delivered to new subscribers. Safety-critical monitors must distinguish between stale retained state from previous boot cycles and fresh state from the current cycle. Add boot-cycle markers (e.g., timestamp, sequence number) to detect stale data.
