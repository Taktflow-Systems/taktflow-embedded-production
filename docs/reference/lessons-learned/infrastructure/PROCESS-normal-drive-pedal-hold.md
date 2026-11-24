# A One-Shot Pedal Override Is Undone by the Idle Loop

## 2026-11-24

**Context**: Visitors of the public SIL demo saw the vehicle in RUN at 0 km/h, and Normal Drive moved it for only about 35 s. The CAN log showed Torque_Request 0 with all ECUs healthy.

**Mistake**: The idle cruise profile has been off in production since 2026-04-19 (`IDLE_CRUISE_ENABLE` unset) because it disturbed the fault scenarios. With cruise off, the idle loop in `gateway/fault_inject/app.py` pins the pedal to neutral at 10 Hz. Normal Drive sent its 50 % pedal only five times and returned; once the 35 s post-scenario pause ran out, the loop wrote neutral again and the vehicle coasted to a stop. RUN by itself never asks for torque.

**Fix**: Normal Drive now sets a drive hold (`DRIVE_HOLD_SCENARIOS`). The idle loop re-sends the held pedal every tick, also while a visitor holds the control lock, and Normal Drive skips the post-scenario pause. Any other scenario, a reset (dashboard or test runner) and a test-run start clear the hold, so the scenarios see the same pedal as before. Tests: `gateway/fault_inject/tests/test_drive_hold.py`.

**Principle**: When a background loop owns an actuator, a command that should last must be a state that loop applies, not a one-shot write it will overwrite.
