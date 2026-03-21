"""Main plant simulator — reads actuator commands from CAN, runs physics,
writes sensor data back to CAN.

Runs at 100 Hz (10ms cycle). Uses python-can for CAN I/O and CanEncoder
(gateway.lib.dbc_encoder) for all CAN encoding — single source of truth
for signal packing and E2E protection.
"""

import asyncio
import json
import logging
import os
import struct
import time

import can
import paho.mqtt.client as paho_mqtt

from .motor_model import MotorModel
from .steering_model import SteeringModel
from .brake_model import BrakeModel
from .battery_model import BatteryModel
from .lidar_model import LidarModel

# CanEncoder lives in gateway/lib/ — use absolute import so it works both
# as a package (docker) and with PYTHONPATH pointing at repo root.
import sys as _sys
_repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if _repo not in _sys.path:
    _sys.path.insert(0, _repo)
from gateway.lib.dbc_encoder import CanEncoder

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [PLANT] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("plant_sim")

# CAN IDs we read (actuator commands from ECUs)
RX_TORQUE_REQUEST = 0x101
RX_STEER_COMMAND = 0x102
RX_BRAKE_COMMAND = 0x103
RX_ESTOP = 0x001
RX_SC_RELAY_STATUS = 0x013

# DTC codes — 24-bit big-endian, must match firmware Dem_SetDtcCode() values
# DBC DTC_Broadcast_Number is 16-bit, so only lower 16 bits are sent.
DTC_OVERCURRENT = 0x00E301   # RZC: Dem_SetDtcCode(RZC_DTC_OVERCURRENT, 0x00E301u)
DTC_STEER_FAULT = 0x00D001   # FZC: Dem_SetDtcCode(FZC_DTC_STEER_PLAUSIBILITY, 0x00D001u)
DTC_BRAKE_FAULT = 0x00E202   # FZC: no firmware Dem equivalent yet — keep for plant-sim
DTC_BATTERY_UV  = 0x00E401   # RZC: Dem_SetDtcCode(RZC_DTC_BATTERY, 0x00E401u)

# ECU source IDs
ECU_FZC = 2
ECU_RZC = 3

# Vehicle state codes
VS_INIT = 0
VS_RUN = 1
VS_DEGRADED = 2
VS_LIMP = 3
VS_SAFE_STOP = 4
VS_SHUTDOWN = 5


class PlantSimulator:
    def __init__(self, dbc_path: str, channel: str = "vcan0"):
        self.encoder = CanEncoder(dbc_path)
        self.channel = channel
        self.bus = None

        # Physics models
        self.motor = MotorModel()
        self.steering = SteeringModel()
        self.brake = BrakeModel()
        self.battery = BatteryModel()
        self.lidar = LidarModel()

        # E-stop state
        self.estop_active = False

        # SC relay kill state (from CAN 0x013)
        self.sc_relay_killed = False

        # Vehicle state — starts INIT, transitions to RUN after startup delay
        self.vehicle_state = VS_INIT
        self._startup_ticks = 0  # counts ticks since start (300 = 3s)

        # DTC tracking — only fire each DTC once until cleared
        self._active_dtcs: set[int] = set()
        self._dtc_occurrence: dict[int, int] = {}

        # Timing counters (10ms base tick)
        self._tick = 0

        # MQTT client for fault injection commands
        self._mqtt: paho_mqtt.Client | None = None

    # ------------------------------------------------------------------
    # MQTT fault injection interface
    # ------------------------------------------------------------------

    def _init_mqtt(self):
        """Connect to MQTT broker and subscribe to plant injection commands.

        Uses synchronous connect with retry to guarantee the subscription is
        active before the main simulation loop starts.  This eliminates the
        race condition where fault-inject MQTT commands arrive before the
        plant-sim subscription is established (QoS 1 messages are dropped by
        the broker if no subscriber exists at publish time).
        """
        host = os.environ.get("MQTT_HOST", "localhost")
        port = int(os.environ.get("MQTT_PORT", "1883"))
        self._mqtt = paho_mqtt.Client(
            paho_mqtt.CallbackAPIVersion.VERSION2,
            client_id="taktflow-plant-sim",
        )
        self._mqtt.on_connect = self._on_mqtt_connect
        self._mqtt.message_callback_add(
            "taktflow/command/plant_inject", self._on_inject_cmd
        )
        # Synchronous connect with retry — broker may still be starting
        for attempt in range(15):
            try:
                self._mqtt.connect(host, port, keepalive=30)
                break
            except (ConnectionRefusedError, OSError) as exc:
                log.warning("MQTT connect attempt %d failed: %s", attempt + 1, exc)
                time.sleep(1)
        else:
            log.error("MQTT connection failed after 15 attempts")
        self._mqtt.loop_start()
        # Subscribe immediately (also done in _on_mqtt_connect for reconnects)
        self._mqtt.subscribe("taktflow/command/plant_inject", qos=1)
        log.info("MQTT connected to %s:%d — subscribed", host, port)

    def _on_mqtt_connect(self, client, userdata, flags, rc, properties=None):
        """Subscribe to injection topic on (re)connect."""
        client.subscribe("taktflow/command/plant_inject", qos=1)
        log.info("MQTT connected — subscribed to taktflow/command/plant_inject")

    def _on_inject_cmd(self, client, userdata, msg):
        """Handle MQTT fault injection commands from fault_inject service."""
        try:
            cmd = json.loads(msg.payload)
        except (json.JSONDecodeError, TypeError):
            log.warning("Invalid inject command payload: %s", msg.payload)
            return

        cmd_type = cmd.get("type", "")
        log.info("MQTT inject command: %s", cmd_type)

        if cmd_type == "overcurrent":
            self.motor.inject_overcurrent()
            log.info("Injected overcurrent fault (current=%.0fmA)", self.motor.current_ma)
        elif cmd_type == "creep_current":
            current_ma = float(cmd.get("current_ma", 1000.0))
            self.motor.inject_creep_current(current_ma)
            log.info("Injected creep current fault (%.0fmA, no OC flag)", current_ma)
        elif cmd_type == "stall":
            self.motor.inject_stall()
            log.info("Injected motor stall fault")
        elif cmd_type == "voltage":
            mv = int(cmd.get("mV", 8500))
            soc = int(cmd.get("soc", 10))
            self.battery.inject_voltage(mv, soc)
            log.info("Injected battery voltage: %dmV, %d%% SOC", mv, soc)
        elif cmd_type == "inject_temp":
            temp_c = float(cmd.get("temp_c", 95.0))
            self.motor.temp_c = temp_c
            log.info("Injected motor temperature: %.1f°C", temp_c)
        elif cmd_type == "steer_fault":
            self.steering.fault = True
            log.info("Injected steering fault")
        elif cmd_type == "brake_fault":
            self.brake.fault = True
            log.info("Injected brake fault")
        elif cmd_type == "reset":
            self.motor.reset_state()
            self.steering.reset_state()
            self.brake.reset_state()
            self.battery.reset_state()
            self._active_dtcs.clear()
            log.info("Plant FULL RESET — all physics returned to power-on defaults")

    def _process_rx(self, msg: can.Message):
        """Process a received CAN frame from ECU actuator commands."""
        arb_id = msg.arbitration_id
        data = msg.data

        if arb_id == RX_ESTOP:
            if len(data) >= 3:
                was_active = self.estop_active
                self.estop_active = bool(data[2] & 0x01)
                if self.estop_active and not was_active:
                    log.info("E-STOP received — all outputs disabled")
                elif not self.estop_active and was_active:
                    log.info("E-STOP cleared — resetting faults, state -> INIT")
                    self.motor.reset_faults()
                    self.steering.clear_fault()
                    self.brake.clear_fault()
                    self.battery.clear_override()
                    self._active_dtcs.clear()
                    self.sc_relay_killed = False
                    self.vehicle_state = VS_INIT
                    self._startup_ticks = 0
                # No action when estop stays inactive — normal heartbeat

        elif arb_id == RX_SC_RELAY_STATUS:
            if len(data) >= 4:
                # SC Status frame: [0]=E2E alive, [1]=CRC, [2]=mode|flags,
                # [3]= ecu_health(2:0) | fault_reason(6:3) | relay_state(7)
                relay_energized = bool(data[3] & 0x80)
                killed = not relay_energized
                if killed and not self.sc_relay_killed:
                    reason = (data[3] >> 3) & 0x0F
                    log.info("SC relay KILLED (reason=%d) — motor disabled", reason)
                    self.sc_relay_killed = True
                    self.motor._hw_disabled = True
                elif not killed and self.sc_relay_killed:
                    log.info("SC relay CLEARED — motor re-enabled")
                    self.sc_relay_killed = False
                    self.motor._hw_disabled = False

        # NOTE: CAN 0x303 handler removed — RZC is sole authority for
        # Battery_Status on the CAN bus. Processing 0x303 here created a
        # feedback loop (RZC TX -> plant-sim override -> plant-sim TX on
        # 0x601 -> RZC reads -> RZC TX). Battery overrides now use MQTT
        # only (taktflow/command/plant_inject {"type":"voltage"}).

        elif arb_id == RX_TORQUE_REQUEST:
            if len(data) >= 4 and not self.estop_active:
                # CVC sends torque as uint16 LE at bytes 2-3 (Com signal 5,
                # bitPos=16, bitSize=16).  Value 0-1000 from torque LUT.
                torque_raw = struct.unpack_from('<H', data, 2)[0]
                self.motor.duty_pct = min(100.0, float(torque_raw) / 10.0)
                self.motor.direction = 1 if torque_raw > 0 else 0

        elif arb_id == RX_STEER_COMMAND:
            if len(data) >= 4 and not self.estop_active:
                # CVC Com sends plain degrees as sint16 (no DBC scaling).
                # 0 = center, +10 = 10 deg right, -10 = 10 deg left.
                raw = struct.unpack_from('<h', data, 2)[0]  # sint16 LE
                angle = max(-45.0, min(45.0, float(raw)))
                self.steering.record_command(angle)

        elif arb_id == RX_BRAKE_COMMAND:
            if len(data) >= 3 and not self.estop_active:
                self.brake.record_command(float(data[2]))

    # ------------------------------------------------------------------
    # TX methods — all use self.encoder.encode() (DBC + E2E)
    # ------------------------------------------------------------------

    def _tx_motor_status(self):
        """Send Motor_Status (0x300) every 20ms."""
        fault_bits = 0
        if self.motor.overcurrent:
            fault_bits |= 0x01
        if self.motor.overtemp:
            fault_bits |= 0x02
        if self.motor.stall_fault:
            fault_bits |= 0x04

        data = self.encoder.encode("Motor_Status", {
            "Motor_Status_TorqueEcho": int(min(100, self.motor.duty_pct)),
            "Motor_Status_MotorSpeed_RPM": self.motor.rpm_int,
            "Motor_Status_MotorDirection": self.motor.direction & 0xFF,
            "Motor_Status_MotorEnable": 1 if self.motor.enabled else 0,
            "Motor_Status_MotorFaultStatus": fault_bits,
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("Motor_Status"),
            data=data, is_extended_id=False))

    def _tx_motor_current(self):
        """Send Motor_Current (0x301) every 10ms."""
        data = self.encoder.encode("Motor_Current", {
            "Motor_Current_Phase_mA": self.motor.current_ma_int,
            "Motor_Current_DirIsReverse": 1 if self.motor.direction == 2 else 0,
            "Motor_Current_MotorEnable": 1 if self.motor.enabled else 0,
            "Motor_Current_OvercurrentFlag": 1 if self.motor.overcurrent else 0,
            "Motor_Current_TorqueEcho": int(self.motor.duty_pct) & 0xFF,
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("Motor_Current"),
            data=data, is_extended_id=False))

    def _tx_motor_temp(self):
        """Send Motor_Temperature (0x302) every 100ms.

        DBC signals use factor 0.1 — pass physical deg C, cantools scales.
        """
        # Derating percent
        if self.motor.overtemp:
            derating = 0
        elif self.motor.temp_c > 80:
            derating = 50
        elif self.motor.temp_c > 60:
            derating = 75
        else:
            derating = 100

        data = self.encoder.encode("Motor_Temperature", {
            "Motor_Temperature_WindingTemp1_C": self.motor.temp_c,
            "Motor_Temperature_WindingTemp2_C": self.motor.temp_c * 0.8,
            "Motor_Temperature_DeratingPercent": derating,
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("Motor_Temperature"),
            data=data, is_extended_id=False))

    def _tx_battery_status(self):
        """Send Battery_Status (0x303) every 1000ms."""
        data = self.encoder.encode("Battery_Status", {
            "Battery_Status_BatteryVoltage_mV": min(20000, self.battery.voltage_mv),
            "Battery_Status_Level": self.battery.status,
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("Battery_Status"),
            data=data, is_extended_id=False))

    def _send_dtc(self, dtc_code: int, ecu_source: int):
        """Send DTC_Broadcast (0x500, 8 bytes, no E2E). Only fires once per DTC."""
        if dtc_code in self._active_dtcs:
            return
        self._active_dtcs.add(dtc_code)
        count = self._dtc_occurrence.get(dtc_code, 0) + 1
        self._dtc_occurrence[dtc_code] = count

        data = self.encoder.encode("DTC_Broadcast", {
            "DTC_Broadcast_Number": dtc_code & 0xFFFF,
            "DTC_Broadcast_Status": 0x01,
            "DTC_Broadcast_ECU_Source": ecu_source & 0xFF,
            "DTC_Broadcast_OccurrenceCount": min(255, count),
            "DTC_Broadcast_FreezeFrame0": 0,
            "DTC_Broadcast_FreezeFrame1": 0,
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("DTC_Broadcast"),
            data=data, is_extended_id=False))
        log.info("DTC 0x%06X from ECU %d (occurrence %d)", dtc_code, ecu_source, count)

    def _check_and_send_dtcs(self):
        """Check fault conditions and send DTCs for faults that have no
        firmware Dem equivalent yet.  DTCs handled by ECU firmware:
          - Overcurrent (0xE301): RZC Swc_Motor.c
          - Battery UV  (0xE401): RZC Swc_Battery.c
          - Steer fault (0xD001): FZC Swc_Steering.c
        Only brake fault still needs plant-sim DTC (no FZC Dem for it yet).
        """
        if self.brake.fault:
            self._send_dtc(DTC_BRAKE_FAULT, ECU_FZC)

    def _tx_steering_status(self):
        """Send Steering_Status (0x200) every 20ms.

        ServoCurrent_mA has DBC scale=10 — pass physical mA, cantools divides.
        """
        data = self.encoder.encode("Steering_Status", {
            "Steering_Status_ActualAngle": self.steering.actual_raw,
            "Steering_Status_CommandedAngle": self.steering.commanded_raw,
            "Steering_Status_SteerFaultStatus": 0x01 if self.steering.fault else 0x00,
            "Steering_Status_SteerMode": 0,
            "Steering_Status_ServoCurrent_mA": min(2550, self.steering.servo_current_ma),
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("Steering_Status"),
            data=data, is_extended_id=False))

    def _tx_brake_status(self):
        """Send Brake_Status (0x201) every 20ms."""
        data = self.encoder.encode("Brake_Status", {
            "Brake_Status_BrakePosition": self.brake.position_int,
            "Brake_Status_BrakeCommandEcho": int(self.brake.commanded_pct),
            "Brake_Status_ServoCurrent_mA": min(65535, self.brake.servo_current_ma),
            "Brake_Status_BrakeFaultStatus": 0x01 if self.brake.fault else 0x00,
            "Brake_Status_BrakeMode": 0,
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("Brake_Status"),
            data=data, is_extended_id=False))

    def _tx_brake_fault_event(self):
        """Send Brake_Fault event (0x210) when brake fault is active.

        CVC reads Brake_Fault_FaultType — any non-zero triggers
        EVT_BRAKE_FAULT -> SAFE_STOP.
        """
        if not self.brake.fault:
            return
        data = self.encoder.encode("Brake_Fault", {
            "Brake_Fault_FaultType": 1,  # deviation
            "Brake_Fault_CommandedBrake": 0,
            "Brake_Fault_MeasuredBrake": 0,
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("Brake_Fault"),
            data=data, is_extended_id=False))

    def _tx_vehicle_state(self):
        """Send Vehicle_State (0x100) every 100ms."""
        data = self.encoder.encode("Vehicle_State", {
            "Vehicle_State_Mode": self.vehicle_state & 0x0F,
            "Vehicle_State_FaultMask": 0,
            "Vehicle_State_TorqueLimit": (
                int(self.motor.duty_pct) & 0xFF
                if self.vehicle_state == VS_RUN else 0
            ),
            "Vehicle_State_SpeedLimit": 100 if self.vehicle_state == VS_RUN else 0,
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("Vehicle_State"),
            data=data, is_extended_id=False))

    def _tx_lidar_distance(self):
        """Send Lidar_Distance (0x220) every 10ms."""
        data = self.encoder.encode("Lidar_Distance", {
            "Lidar_Distance_Range_cm": self.lidar.distance_cm,
            "Lidar_Distance_SignalStrength": self.lidar.signal_strength,
            "Lidar_Distance_ObstacleZone": self.lidar.obstacle_zone,
            "Lidar_Distance_SensorStatus": 0x01 if self.lidar.fault else 0x00,
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("Lidar_Distance"),
            data=data, is_extended_id=False))

    def _tx_fzc_virtual_sensors(self):
        """Send FZC virtual sensor data (0x600) every 10ms. No E2E.

        Plant-sim physics -> DBC-encoded CAN message -> FZC sensor feeder SWC ->
        MCAL injection -> IoHwAb -> SWC fault detection.

        Signals are raw sensor format (not physical) — matches AS5048A SPI.
        """
        angle_deg = 45.0 if self.steering.fault else self.steering.actual_angle
        angle_raw = int((angle_deg + 45.0) / 90.0 * 16383.0)
        angle_raw = max(0, min(16383, angle_raw))

        if self.brake.fault:
            brake_adc = 500
        else:
            brake_adc = int(self.brake.position_int * 10)
        brake_adc = max(0, min(1000, brake_adc))

        data = self.encoder.encode("FZC_Virtual_Sensors", {
            "FZC_Virtual_Sensors_SteerAngle_Raw": angle_raw,
            "FZC_Virtual_Sensors_BrakePos_ADC": brake_adc,
            "FZC_Virtual_Sensors_BrakeCurrent_mA": min(65535, self.brake.servo_current_ma),
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("FZC_Virtual_Sensors"),
            data=data, is_extended_id=False))

    def _tx_rzc_virtual_sensors(self):
        """Send RZC virtual sensor data (0x601) every 10ms. No E2E.

        Plant-sim physics -> DBC-encoded CAN message -> RZC sensor feeder SWC ->
        ADC injection -> IoHwAb -> SWC fault detection.
        """
        data = self.encoder.encode("RZC_Virtual_Sensors", {
            "RZC_Virtual_Sensors_MotorCurrent_mA": min(65535, self.motor.current_ma_int),
            "RZC_Virtual_Sensors_MotorTemp_dC": int(self.motor.temp_c * 10),
            "RZC_Virtual_Sensors_BattVoltage_mV": min(20000, self.battery.voltage_mv),
            "RZC_Virtual_Sensors_MotorSpeed_RPM": max(0, min(10000, self.motor.rpm_int)),
        })
        self.bus.send(can.Message(
            arbitration_id=self.encoder.get_id("RZC_Virtual_Sensors"),
            data=data, is_extended_id=False))

    async def run(self):
        """Main simulation loop at 100 Hz (scaled by SIL_TIME_SCALE)."""
        self.bus = can.interface.Bus(channel=self.channel,
                                     interface="socketcan")
        self._init_mqtt()
        log.info("Plant simulator started on %s", self.channel)
        log.info("Loaded DBC with %d messages", len(self.encoder.db.messages))

        # SIL time acceleration: physics dt stays 10ms virtual, but wall-clock
        # sleep is divided by scale so physics runs Nx faster
        sil_scale = int(os.environ.get("SIL_TIME_SCALE", "1"))
        sil_scale = max(1, min(100, sil_scale))
        dt = 0.01  # 10ms virtual time step (physics always sees 10ms)
        wall_dt = dt / sil_scale  # actual wall-clock sleep per tick
        if sil_scale > 1:
            log.info("SIL time acceleration: %dx (wall sleep %.1fms per tick)",
                     sil_scale, wall_dt * 1000)
        self._tick = 0

        try:
            while True:
                loop_start = time.monotonic()

                # Read all available CAN frames (non-blocking)
                while True:
                    msg = self.bus.recv(timeout=0)
                    if msg is None:
                        break
                    self._process_rx(msg)

                # Update physics — model hardware capabilities, not policy.
                # Policy (when to brake, which state to enter) is CVC's job.
                # Plant-sim models: motor power limits, E-stop brake lock,
                # and natural physics.  Brake always follows CVC command
                # (via CAN 0x103) except E-stop which is a physical lock.
                if self.estop_active:
                    # E-stop: physical brake lock + motor kill
                    self.motor.update(0, 0, dt)
                    self.steering.update(0, dt)
                    self.brake.update(100, dt)
                elif self.sc_relay_killed or self.motor._hw_disabled:
                    # SC relay or overcurrent: motor power cut, actuators
                    # follow CVC commands (CVC will command 100% brake
                    # once it transitions to SAFE_STOP)
                    self.motor.update(0, 0, dt)
                    self.steering.update(self.steering.commanded_angle, dt)
                    self.brake.update(self.brake.commanded_pct, dt)
                else:
                    # Normal operation — apply torque cap from battery
                    # capacity (physics: less voltage = less torque available)
                    if self.battery.status == 0:       # critical UV
                        duty_cap = 15.0
                    elif self.battery.status == 1:     # UV warning
                        duty_cap = 50.0
                    else:
                        duty_cap = 100.0
                    capped_duty = min(self.motor.duty_pct, duty_cap)
                    brake_load = self.brake.actual_pct / 100.0
                    self.motor.update(capped_duty, self.motor.direction, dt,
                                      brake_load=brake_load)
                    self.steering.update(self.steering.commanded_angle, dt)
                    self.brake.update(self.brake.commanded_pct, dt)

                self.battery.update(self.motor.current_ma, dt)
                self.lidar.update(dt)

                # Vehicle state machine
                self._startup_ticks += 1
                if self.estop_active:
                    if self.vehicle_state != VS_SAFE_STOP:
                        self.vehicle_state = VS_SAFE_STOP
                        log.info("Vehicle state -> SAFE_STOP (E-Stop)")
                elif self.vehicle_state == VS_INIT and self._startup_ticks >= 300:
                    # Transition to RUN after 3 seconds of startup
                    self.vehicle_state = VS_RUN
                    log.info("Vehicle state -> RUN (startup complete)")
                elif (self.vehicle_state == VS_SAFE_STOP
                      and not self.estop_active
                      and not self.brake.fault
                      and not self.steering.fault
                      and not self.motor.overcurrent
                      and not self.motor._hw_disabled
                      and not self.sc_relay_killed):
                    # Exit SAFE_STOP only when ALL safety triggers cleared
                    self.vehicle_state = VS_INIT
                    self._startup_ticks = 0
                    log.info("Vehicle state -> INIT (safety triggers cleared, re-initializing)")

                # Transition to SAFE_STOP/DEGRADED/LIMP on faults
                # SG-003 (ASIL D): steer fault -> SAFE_STOP (SS-MOTOR-OFF)
                # SG-004 (ASIL D): brake fault -> SAFE_STOP (SS-MOTOR-OFF)
                # SG-006 (ASIL A): overcurrent -> SAFE_STOP (SS-MOTOR-OFF)
                safe_stop_fault = (
                    self.brake.fault or self.steering.fault
                    or self.motor.overcurrent
                    or self.motor._hw_disabled
                )
                battery_critical = self.battery.status == 0  # critical UV
                battery_warn = self.battery.status == 1      # UV warning

                if safe_stop_fault and self.vehicle_state in (
                        VS_RUN, VS_DEGRADED):
                    self.vehicle_state = VS_SAFE_STOP
                    log.info("Vehicle state -> SAFE_STOP (safety-critical fault)")
                elif battery_critical and self.vehicle_state in (
                        VS_RUN, VS_DEGRADED):
                    self.vehicle_state = VS_LIMP
                    log.info("Vehicle state -> LIMP (battery critical UV)")
                elif battery_warn and self.vehicle_state == VS_RUN:
                    self.vehicle_state = VS_DEGRADED
                    log.info("Vehicle state -> DEGRADED (battery undervoltage warning)")
                elif (not safe_stop_fault
                      and not battery_warn and not battery_critical
                      and self.vehicle_state in (VS_DEGRADED, VS_LIMP)):
                    self.vehicle_state = VS_RUN
                    log.info("Vehicle state -> RUN (faults cleared)")

                # TX schedule
                # Every 10ms: lidar, virtual sensors
                self._tx_lidar_distance()
                self._tx_fzc_virtual_sensors()
                self._tx_rzc_virtual_sensors()

                # Note: Motor status (0x300), motor current (0x301), motor
                # temperature (0x302), and battery status (0x303) broadcasts
                # removed — RZC firmware is the sole authority for these CAN
                # IDs.  Plant-sim sends virtual sensor data (0x601) which the
                # RZC sensor feeder injects into MCAL ADC stubs, so RZC SWCs
                # read real physics values and transmit correct telemetry.

                # Every tick (10ms): DTC check — must be frequent enough
                # to catch transient faults before physics model clears them
                self._check_and_send_dtcs()

                self._tick += 1

                # Log status every 5 seconds
                if self._tick % 500 == 0:
                    log.info(
                        "RPM=%d  I=%dmA  T=%.1f°C  V=%dmV  steer=%.1f°  brake=%d%%",
                        self.motor.rpm_int,
                        self.motor.current_ma_int,
                        self.motor.temp_c,
                        self.battery.voltage_mv,
                        self.steering.actual_angle,
                        self.brake.position_int,
                    )

                # Sleep remainder of cycle (wall_dt = dt / SIL_TIME_SCALE)
                elapsed = time.monotonic() - loop_start
                sleep_time = wall_dt - elapsed
                if sleep_time > 0:
                    await asyncio.sleep(sleep_time)

        except KeyboardInterrupt:
            log.info("Plant simulator stopped")
        finally:
            if self._mqtt:
                self._mqtt.loop_stop()
                self._mqtt.disconnect()
            if self.bus:
                self.bus.shutdown()


def main():
    dbc_path = os.environ.get(
        "DBC_PATH",
        os.path.join(os.path.dirname(__file__), "..", "taktflow_vehicle.dbc"),
    )
    channel = os.environ.get("CAN_CHANNEL", "vcan0")

    sim = PlantSimulator(dbc_path, channel)
    asyncio.run(sim.run())


if __name__ == "__main__":
    main()
