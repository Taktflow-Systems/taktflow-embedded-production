"""Lidar distance model — configurable for scenario injection."""

import random


class LidarModel:
    def __init__(self):
        self._base_distance = 500  # default 5 meters
        self.distance_cm = 500
        self.signal_strength = 8000
        self.fault = False

    def update(self, dt: float):
        """Add sensor noise to prevent FZC stuck detection (50 identical readings)."""
        if not self.fault:
            self.distance_cm = self._base_distance + random.randint(-2, 2)

    def inject_distance(self, distance_cm: int):
        self._base_distance = max(0, min(1200, distance_cm))
        self.distance_cm = self._base_distance

    def inject_fault(self):
        self.fault = True
        self.signal_strength = 0

    def reset(self):
        self._base_distance = 500
        self.distance_cm = 500
        self.signal_strength = 8000
        self.fault = False

    @property
    def obstacle_zone(self) -> int:
        """0=emergency(<30cm), 1=braking(<100cm), 2=warning(<300cm), 3=clear."""
        if self.distance_cm < 30:
            return 0
        elif self.distance_cm < 100:
            return 1
        elif self.distance_cm < 300:
            return 2
        return 3
