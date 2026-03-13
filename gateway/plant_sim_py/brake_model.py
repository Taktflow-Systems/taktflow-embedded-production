"""Brake servo model — rate-limited tracking of commanded force.

Fault detection:
  Rapid alternation between 0% and high brake force triggers fault
  (conflicting brake commands).

  NOTE: Fault detection runs in record_command() (called per CAN frame)
  not in update() (called per physics tick), so rapid alternations at
  5ms intervals are captured even with 10ms physics ticks.
"""

import time
from collections import deque


class BrakeModel:
    RATE_LIMIT_PCT_S = 200.0  # can go 0-100% in 0.5s

    # Fault detection: rapid 0%/100% alternation
    CONFLICT_WINDOW_S = 0.5
    CONFLICT_THRESHOLD = 4  # large swings in window to trigger fault
    CONFLICT_SWING_PCT = 50.0  # minimum change to count as a swing

    def __init__(self):
        self.actual_pct = 0.0
        self.commanded_pct = 0.0
        self.servo_current_ma = 0
        self.fault = False

        # Conflict detection (runs per CAN frame via record_command)
        self._prev_cmd = 0.0
        self._large_swings: deque[float] = deque()  # monotonic timestamps

    def record_command(self, commanded_pct: float):
        """Record a brake command from a CAN frame — called per frame, not per tick.

        This captures rapid alternations that would be lost if only checked
        once per physics tick (10ms), since fault scenarios send at 5ms.
        """
        clamped = max(0.0, min(100.0, commanded_pct))
        self.commanded_pct = clamped

        cmd_delta = abs(clamped - self._prev_cmd)
        if cmd_delta >= self.CONFLICT_SWING_PCT:
            self._large_swings.append(time.monotonic())
        self._prev_cmd = clamped

        # Expire old swings outside window
        cutoff = time.monotonic() - self.CONFLICT_WINDOW_S
        while self._large_swings and self._large_swings[0] < cutoff:
            self._large_swings.popleft()

        # Set fault if too many large swings in window
        if len(self._large_swings) >= self.CONFLICT_THRESHOLD:
            self.fault = True

    def update(self, commanded_pct: float, dt: float):
        """Advance physics by dt seconds. Fault detection is in record_command()."""
        self.commanded_pct = max(0.0, min(100.0, commanded_pct))

        # Physics
        error = self.commanded_pct - self.actual_pct
        max_step = self.RATE_LIMIT_PCT_S * dt

        if abs(error) <= max_step:
            self.actual_pct = self.commanded_pct
        elif error > 0:
            self.actual_pct += max_step
        else:
            self.actual_pct -= max_step

        self.servo_current_ma = int(abs(error) * 15.0)
        self.servo_current_ma = min(3000, self.servo_current_ma)

    def clear_fault(self):
        """Clear fault state (called on reset)."""
        self.fault = False
        self._large_swings.clear()

    @property
    def position_int(self) -> int:
        return int(max(0, min(100, self.actual_pct)))
