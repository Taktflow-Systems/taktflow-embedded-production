"""Motor + inverter model with FOC current control and thermal dynamics.

Models an automotive BLDC/PMSM with field-oriented control (FOC) inverter.
The FOC controller maintains motor current proportional to the torque command
regardless of RPM (closed-loop Iq regulation), matching the Safety Controller's
torque-vs-current plausibility lookup table.

Physics:
  dw/dt = (torque - friction * w) / inertia
  current = rated_current * (duty / 100)   [FOC closed-loop]
  dT/dt = (I^2 * R_thermal - (T - T_ambient) / R_cool) / C_thermal
"""

import time


class MotorModel:
    # Motor parameters (BLDC with FOC inverter, 12V, demo-quality)
    NO_LOAD_RPM = 4000.0
    RATED_CURRENT_MA = 25000.0  # 25A at 100% torque command
    FRICTION = 0.005            # friction coefficient (Nm/RPM)
    INERTIA = 0.002             # kg*m^2
    R_THERMAL = 0.001           # degC per mA^2 — equilibrium at 25A ≈ 75°C (below 100°C overtemp)
    R_COOL = 80.0               # thermal resistance degC/W
    C_THERMAL = 50.0            # thermal capacitance J/degC
    T_AMBIENT = 25.0            # ambient temperature degC

    def __init__(self):
        self.rpm = 0.0
        self.current_ma = 0.0
        self.temp_c = self.T_AMBIENT
        self.direction = 0       # 0=stop, 1=fwd, 2=rev
        self.enabled = False
        self.duty_pct = 0.0
        self.overcurrent = False
        self.overtemp = False
        self.stall_fault = False
        self._hw_disabled = False    # firmware overcurrent/overtemp disable
        self._overcurrent_latch = False  # injected overcurrent persists
        self._injected_current_ma = 0.0
        self._last_time = time.monotonic()

    def update(self, duty_pct: float, direction: int, dt: float = None,
               brake_load: float = 0.0):
        """Advance motor physics by dt seconds.

        brake_load: 0.0-1.0 mechanical resistance from brake (0=free, 1=locked).
        """
        now = time.monotonic()
        if dt is None:
            dt = now - self._last_time
        self._last_time = now

        if dt <= 0 or dt > 1.0:
            dt = 0.01

        self.duty_pct = max(0.0, min(100.0, duty_pct))
        self.direction = direction
        brake_load = max(0.0, min(1.0, brake_load))

        if direction == 0 or self.duty_pct < 1.0:
            self.enabled = False
        else:
            self.enabled = True

        if self._hw_disabled:
            self.enabled = False

        # RPM dynamics — brake opposes motor, reducing achievable speed
        if self.enabled and not self.stall_fault:
            target_rpm = self.NO_LOAD_RPM * (self.duty_pct / 100.0) * (1.0 - brake_load)
            # First-order approach to target
            tau = 0.3  # time constant in seconds
            self.rpm += (target_rpm - self.rpm) * (dt / tau)
        else:
            # Decelerate
            tau = 0.5
            self.rpm *= max(0.0, 1.0 - dt / tau)
            if self.rpm < 1.0:
                self.rpm = 0.0

        # Current model: FOC inverter maintains Iq ∝ torque command
        if self._overcurrent_latch:
            # Injected overcurrent - hold at injected level, motor disabled
            self.current_ma = self._injected_current_ma
        elif self.enabled:
            self.current_ma = self.RATED_CURRENT_MA * (self.duty_pct / 100.0)
        else:
            # FOC inverter actively drives current to zero when disabled
            self.current_ma = 0.0

        # Thermal model — use zero current for heating when HW disabled
        thermal_current = 0.0 if self._hw_disabled else self.current_ma
        heat_input = (thermal_current / 1000.0) ** 2 * self.R_THERMAL
        heat_loss = (self.temp_c - self.T_AMBIENT) / self.R_COOL
        self.temp_c += (heat_input - heat_loss) * dt  # real-time thermal (overtemp scenario uses direct MQTT injection)

        # Fault detection — threshold above rated max (25A) but below
        # injected fault level (28A) so natural operation never triggers
        self.overcurrent = self.current_ma > 26000.0
        self.overtemp = self.temp_c > 100.0

        if self.overcurrent or self.overtemp:
            self._hw_disabled = True

    def inject_overcurrent(self, current_ma: float = 28000.0):
        """Inject an overcurrent fault - latched until reset_faults()."""
        self.current_ma = current_ma
        self._injected_current_ma = current_ma
        self._overcurrent_latch = True
        self.overcurrent = True
        self._hw_disabled = True

    def inject_creep_current(self, current_ma: float = 1000.0):
        """Inject motor current without overcurrent flag — simulates FET short.

        BTS7960 FET gate-source short causes current flow despite zero torque
        command. The FOC inverter is NOT disabled (it's a hardware fault below
        the inverter control loop). SC creep guard detects this via
        torque=0 AND current>500mA cross-plausibility.
        """
        self.current_ma = current_ma
        self._injected_current_ma = current_ma
        self._overcurrent_latch = True  # hold current at injected level

    def inject_stall(self):
        """Inject a stall fault for demo."""
        self.stall_fault = True
        self.rpm = 0.0

    def reset_faults(self):
        """Clear injected faults."""
        self.stall_fault = False
        self.overcurrent = False
        self.overtemp = False
        self._hw_disabled = False
        self._overcurrent_latch = False
        self._injected_current_ma = 0.0

    def reset_state(self):
        """Full physics reset — return to power-on defaults."""
        self.rpm = 0.0
        self.current_ma = 0.0
        self.temp_c = self.T_AMBIENT
        self.direction = 0
        self.enabled = False
        self.duty_pct = 0.0
        self.reset_faults()

    @property
    def temp_c_int(self) -> int:
        return int(max(-40, min(215, self.temp_c)))

    @property
    def rpm_int(self) -> int:
        return int(max(0, min(10000, self.rpm)))

    @property
    def current_ma_int(self) -> int:
        return int(max(0, min(30000, self.current_ma)))
