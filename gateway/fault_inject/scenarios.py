"""Predefined fault scenarios for Taktflow CAN fault injection.

Each scenario sends specific CAN frames on vcan0 to trigger fault paths
through the real ECU firmware running in Docker containers.

CAN frame layout matches taktflow.dbc.  All E2E-protected messages use
CRC-8 SAE J1850 (poly 0x1D, init 0xFF) computed over [data_id] + payload[2:].
"""

import json
import logging
import os
import struct
import time
from typing import Optional

import can
import docker
import paho.mqtt.client as paho_mqtt

# SIL time acceleration: divide wall-clock sleeps by scale factor
_SIL_SCALE = max(1, min(100, int(os.environ.get("SIL_TIME_SCALE", "1"))))


def _scaled_sleep(seconds: float) -> None:
    """Sleep for seconds / SIL_TIME_SCALE wall-clock time."""
    time.sleep(seconds / _SIL_SCALE)

from .pedal_udp import (
    clear_pedal_override,
    pedal_pct_to_angle,
    send_estop_activate,
    send_estop_clear,
    send_pedal_override,
)
from .plant_inject import (
    inject_brake_fault,
    inject_creep_current as plant_inject_creep_current,
    inject_overcurrent as plant_inject_overcurrent,
    inject_stall as plant_inject_stall,
    inject_steer_fault,
    inject_temp as plant_inject_temp,
    inject_voltage as plant_inject_voltage,
    reset_plant_faults,
)

# Module-level MQTT client (set by app.py at startup)
_mqtt_client: paho_mqtt.Client | None = None


def set_mqtt_client(client: paho_mqtt.Client) -> None:
    """Set the module-level MQTT client (called by app.py on startup)."""
    global _mqtt_client
    _mqtt_client = client

# ---------------------------------------------------------------------------
# CAN IDs (from taktflow.dbc)
# ---------------------------------------------------------------------------
CAN_ESTOP = 0x001           # EStop_Broadcast  — 4 bytes
CAN_TORQUE_REQUEST = 0x101  # Torque_Request   — 8 bytes
CAN_STEER_COMMAND = 0x102   # Steer_Command    — 8 bytes
CAN_BRAKE_COMMAND = 0x103   # Brake_Command    — 8 bytes
CAN_BATTERY_STATUS = 0x303  # Battery_Status   — 4 bytes, no E2E
CAN_DTC_BROADCAST = 0x500   # DTC_Broadcast    — 8 bytes, no E2E

# DTC codes — 24-bit big-endian, must match firmware Dem_SetDtcCode() values
DTC_OVERCURRENT = 0x00E301
DTC_STEER_FAULT = 0x00D001
DTC_BRAKE_FAULT = 0x00E202
DTC_OVERTEMP = 0x00E302
DTC_BATTERY_UV = 0x00E401
ECU_RZC = 3
ECU_FZC = 2

# E2E Data IDs (lower 4 bits of byte 0, matches DBC DataID field values)
# These are fixed per message type by convention in the DBC/plant sim.
DATA_ID_ESTOP = 0x01
DATA_ID_TORQUE = 0x02
DATA_ID_STEER = 0x03
DATA_ID_BRAKE = 0x04


# Shared alive counters (per CAN ID, 4-bit wrapping)
_alive_counters: dict[int, int] = {}


# ---------------------------------------------------------------------------
# E2E helpers — matches plant_sim.simulator._crc8_j1850 exactly
# ---------------------------------------------------------------------------

def _crc8_j1850(data_id: int, payload: bytes, start: int = 2) -> int:
    """CRC-8 SAE J1850 over [data_id] + payload[start:]."""
    crc = 0xFF
    for byte in [data_id] + list(payload[start:]):
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x1D) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def _next_alive(msg_id: int) -> int:
    """Increment and return the 4-bit alive counter for a CAN ID."""
    val = _alive_counters.get(msg_id, 0)
    _alive_counters[msg_id] = (val + 1) & 0x0F
    return val


def _build_e2e_frame(msg_id: int, data_id: int,
                     payload: bytearray) -> bytes:
    """Insert E2E header (byte 0) and CRC (byte 1) into payload."""
    alive = _next_alive(msg_id)
    payload[0] = (alive << 4) | (data_id & 0x0F)
    crc = _crc8_j1850(data_id, bytes(payload))
    payload[1] = crc
    return bytes(payload)


def _get_bus() -> can.Bus:
    """Create a python-can Bus on the configured channel."""
    channel = os.environ.get("CAN_CHANNEL", "vcan0")
    return can.interface.Bus(channel=channel, interface="socketcan")


def _send(bus: can.Bus, arb_id: int, data: bytes) -> None:
    """Send a single CAN frame."""
    msg = can.Message(arbitration_id=arb_id, data=data,
                      is_extended_id=False)
    bus.send(msg)


# ---------------------------------------------------------------------------
# Helper frame builders
# ---------------------------------------------------------------------------

def _torque_frame(duty_pct: int, direction: int) -> bytes:
    """Build a Torque_Request frame (0x101, 8 bytes).

    Byte layout (little-endian, from DBC):
      [0] E2E: AliveCounter[7:4] | DataID[3:0]
      [1] E2E: CRC8
      [2] TorqueRequest  (0-100 %)
      [3] Direction[1:0]  (0=stop, 1=fwd, 2=rev)
      [4..7] PedalPosition1/2, PedalFault (zeroed for injection)
    """
    payload = bytearray(8)
    payload[2] = max(0, min(100, duty_pct))
    payload[3] = direction & 0x03
    return _build_e2e_frame(CAN_TORQUE_REQUEST, DATA_ID_TORQUE, payload)


def _steer_frame(angle_deg: float, rate_limit: float = 10.0,
                 vehicle_state: int = 1) -> bytes:
    """Build a Steer_Command frame (0x102, 8 bytes).

    SteerAngleCmd: sint16 at bytes 2-3, plain degrees (no DBC scaling).
    CVC Com sends 0 = center, +10 = 10° right, -10 = 10° left.
    """
    payload = bytearray(8)
    raw = max(-45, min(45, int(angle_deg)))
    struct.pack_into('<h', payload, 2, raw)
    # SteerRateLimit: scale 0.2, byte 4
    payload[4] = max(0, min(255, int(rate_limit / 0.2)))
    # VehicleState: 4 bits at byte 5
    payload[5] = vehicle_state & 0x0F
    return _build_e2e_frame(CAN_STEER_COMMAND, DATA_ID_STEER, payload)


def _brake_frame(brake_pct: int, brake_mode: int = 1,
                 vehicle_state: int = 1) -> bytes:
    """Build a Brake_Command frame (0x103, 8 bytes).

    BrakeForceCmd: 8-bit (0-100 %)
    BrakeMode: 4 bits at bits 24-27  (0=release, 1=normal, 2=emergency, 3=auto)
    VehicleState: 4 bits at bits 28-31
    """
    payload = bytearray(8)
    payload[2] = max(0, min(100, brake_pct))
    payload[3] = (brake_mode & 0x0F) | ((vehicle_state & 0x0F) << 4)
    return _build_e2e_frame(CAN_BRAKE_COMMAND, DATA_ID_BRAKE, payload)


def _battery_frame(voltage_mv: int, soc_pct: int, status: int) -> bytes:
    """Build a Battery_Status frame (0x303, 4 bytes, no E2E).

    Byte layout (from DBC):
      [0..1] BatteryVoltage_mV  (16-bit LE, 0-20000)
      [2]    BatterySOC          (0-100 %)
      [3]    BatteryStatus[3:0]  (0=critical_UV, 1=UV_warn, 2=normal)
    """
    payload = bytearray(4)
    voltage_mv = max(0, min(20000, voltage_mv))
    payload[0] = voltage_mv & 0xFF
    payload[1] = (voltage_mv >> 8) & 0xFF
    payload[2] = max(0, min(100, soc_pct))
    payload[3] = status & 0x0F
    return bytes(payload)


def _dtc_frame(dtc_code: int, ecu_source: int, occurrence: int = 1) -> bytes:
    """Build a DTC_Broadcast frame (0x500, 8 bytes, no E2E).

    Byte layout (matches firmware Dem.c 24-bit big-endian):
      [0]    DTC_Number high byte
      [1]    DTC_Number mid byte
      [2]    DTC_Number low byte
      [3]    DTC_Status      (0x01 = active)
      [4]    ECU_Source       (1=CVC, 2=FZC, 3=RZC, 4=SC)
      [5]    OccurrenceCount
      [6..7] Reserved
    """
    payload = bytearray(8)
    payload[0] = (dtc_code >> 16) & 0xFF   # DTC high byte
    payload[1] = (dtc_code >> 8) & 0xFF    # DTC mid byte
    payload[2] = dtc_code & 0xFF           # DTC low byte
    payload[3] = 0x01  # active
    payload[4] = ecu_source & 0xFF
    payload[5] = min(255, occurrence)
    return bytes(payload)


def _estop_frame(active: bool, source: int = 1) -> bytes:
    """Build an EStop_Broadcast frame (0x001, 4 bytes).

    Byte layout:
      [0] E2E: AliveCounter[7:4] | DataID[3:0]
      [1] E2E: CRC8
      [2] EStop_Active  (uint8: 0=inactive, 1=active)
      [3] EStop_Source   (uint8: 0=CVC_button, 1=CAN_request, 2=SC_relay)
    """
    payload = bytearray(4)
    payload[2] = 1 if active else 0
    payload[3] = source & 0xFF
    return _build_e2e_frame(CAN_ESTOP, DATA_ID_ESTOP, payload)


# ---------------------------------------------------------------------------
# Scenario functions
# ---------------------------------------------------------------------------

def normal_drive() -> str:
    """Set pedal to 50% via SPI override, steer 0 deg, brake 0%.

    Injects pedal sensor value at the MCAL layer so the CVC processes
    it through its full pipeline (plausibility, ramp limit, torque lookup).
    The CVC naturally generates the Torque_Request CAN frame.

    Sends 5 UDP pedal override packets at 50ms intervals to ensure
    the CVC SPI stub reliably receives the override despite CI jitter.
    """
    angle = pedal_pct_to_angle(50)
    for _ in range(5):
        send_pedal_override(angle)
        _scaled_sleep(0.05)
    bus = _get_bus()
    try:
        _send(bus, CAN_STEER_COMMAND, _steer_frame(0.0))
        _send(bus, CAN_BRAKE_COMMAND, _brake_frame(0))
    finally:
        bus.shutdown()
    return "Normal drive: pedal 50% (SPI override), steer 0 deg, brake 0%"


def overcurrent() -> str:
    """Inject motor overcurrent via plant-sim MQTT command.

    Sends SPI pedal override at 95% so the CVC drives the motor at
    high duty, then tells the plant-sim to inject an overcurrent fault
    directly.  This bypasses the CAN cyclic-TX override problem (CVC
    cyclically sends 0x101/0x103, overriding any injected frames).

    The plant-sim sets motor.overcurrent = True, which:
      1. Appears on Motor_Current (0x301) OvercurrentFlag
      2. Triggers DTC 0xE301 broadcast
      3. Plant-sim physics -> SAFE_STOP (motor disabled)
      4. CVC reads overcurrent from CAN -> transitions to SAFE_STOP

    Also sends DTC 0xE301 directly on CAN 0x500 as belt-and-suspenders
    in case the plant-sim MQTT subscription isn't ready yet.
    """
    if _mqtt_client is None:
        return "Overcurrent: MQTT client not available"
    send_pedal_override(pedal_pct_to_angle(95))
    _scaled_sleep(0.5)  # let motor spin up
    plant_inject_overcurrent(_mqtt_client)
    _scaled_sleep(1.0)  # allow plant-sim to process + send physics update
    # DTC 0xE301 now sent by RZC firmware (Swc_Motor.c) when it detects
    # overcurrent via the real signal path — no belt-and-suspenders needed.
    return ("Overcurrent: pedal 95% (SPI) + MQTT inject_overcurrent.  "
            "RZC detects overcurrent -> DTC 0xE301 -> SAFE_STOP.")


def steer_fault() -> str:
    """Inject a steering fault via plant-sim MQTT command.

    Directly sets steering.fault = True in the plant-sim, bypassing
    the CAN cyclic-TX override problem (CVC cyclically sends 0x102,
    overriding any injected steer commands within one tick).

    The plant-sim:
      1. Sets steering fault flag
      2. Sends DTC 0xD001 (steer plausibility fault) on CAN 0x500
      3. Transitions to SAFE_STOP (motor off, brake applied)
      4. Motor_Status CAN frames reflect motor disabled

    Also sends DTC 0xD001 directly on CAN 0x500 as belt-and-suspenders.
    """
    if _mqtt_client is None:
        return "Steer fault: MQTT client not available"
    inject_steer_fault(_mqtt_client)
    _scaled_sleep(0.5)  # allow plant-sim to process
    # DTC 0xD001 now sent by FZC firmware when it detects steering
    # plausibility fault via the real signal path.
    return ("Steer fault: MQTT inject_steer_fault.  "
            "FZC detects plausibility fault -> DTC 0xD001 -> SAFE_STOP.")


def brake_fault() -> str:
    """Inject a brake fault via plant-sim MQTT virtual sensor deviation.

    Plant-sim sets brake.fault, overrides FZC virtual sensor to report 50%
    position (deviation from cmd=0%).  FZC firmware detects PWM deviation
    (>2% for 50 consecutive cycles = 500ms SIL), sets Brake_Fault=1 in RTE.
    Swc_FzcCom_TransmitSchedule reads RTE and sends E2E-protected frame on
    CAN 0x210 with FaultType=1.  CVC reads byte 2 -> EVT_BRAKE_FAULT ->
    SAFE_STOP.
    """
    if _mqtt_client is None:
        return "Brake fault: MQTT client not available"
    inject_brake_fault(_mqtt_client)
    _scaled_sleep(1.5)  # FZC needs 500ms debounce + 100ms TX cycle + margin
    return ("Brake fault: MQTT inject_brake_fault.  "
            "FZC detects PWM deviation -> CAN 0x210 FaultType=1 -> SAFE_STOP.")


def motor_overtemp() -> str:
    """Ramp motor temperature via MQTT plant-sim injection.

    Phase 1 (2.0 s): 25 C -> 85 C  (derating zone, above 80 C threshold)
    Phase 2 (2.0 s): 85 C -> 110 C (overtemp zone, above 100 C threshold)

    This bypasses the physics model heating (which requires sustained current
    and would trigger overcurrent detection before overtemp).  Direct temp
    injection lets us test the RZC thermal protection chain in isolation.
    """
    if _mqtt_client is None:
        return "Motor overtemp: MQTT client not available"

    # Phase 1: ramp from 25 C to 85 C over 2.0 s (derating zone)
    for i in range(20):
        frac = i / 19.0
        temp = 25.0 + (85.0 - 25.0) * frac
        plant_inject_temp(_mqtt_client, temp)
        _scaled_sleep(0.1)

    # Phase 2: ramp from 85 C to 110 C over 2.0 s (overtemp zone)
    for i in range(20):
        frac = i / 19.0
        temp = 85.0 + (110.0 - 85.0) * frac
        plant_inject_temp(_mqtt_client, temp)
        _scaled_sleep(0.1)

    # DTC 0xE302 now sent by RZC firmware (Swc_Motor.c) when it detects
    # overtemp via the real signal path — no belt-and-suspenders needed.
    return ("Motor overtemp: 25 C -> 110 C over 4 s via MQTT.  "
            "RZC detects overtemp -> DTC 0xE302.  Derating at 80 C, shutdown at 100 C.")


def battery_low() -> str:
    """Simulate battery undervoltage via MQTT plant-sim injection.

    Sends progressive voltage drops via MQTT to the plant-sim battery model.
    The plant-sim then sends low voltage on virtual sensor CAN (0x401),
    which the RZC reads via its sensor feeder → IoHwAb → SWC → CAN 0x303.
    After the drain sequence, sends a DTC_BATTERY_UV broadcast.

    Phase 1 (1.5 s): 12.6 V -> 10.4 V (approach UV_warn zone)
    Phase 2 (3.5 s): 10.4 V ->  9.2 V (UV_warn zone — status=1, RZC threshold 10.5 V)
    Phase 3 (2.5 s): 9.2 V  ->  7.0 V (critical_UV zone — status=0, RZC threshold 9.0 V) + DTC
    """
    if _mqtt_client is None:
        return "Battery low: MQTT client not available"

    bus = _get_bus()
    try:
        # Phase 1: voltage drops from 12.6 V to 10.4 V over 1.5 s
        for i in range(15):
            frac = i / 14.0
            v = int(12600 - (12600 - 10400) * frac)
            soc = int(100 - (100 - 30) * frac)
            plant_inject_voltage(_mqtt_client, v, soc)
            _scaled_sleep(0.1)

        # Phase 2: voltage drops from 10.4 V to 9.2 V (UV_warn) over 3.5 s
        # Dwell in warn zone (10500..9000 mV) long enough for RZC's 100ms
        # sample + 4-sample average to settle, and CVC to read battery_status=1
        for i in range(35):
            frac = i / 34.0
            v = int(10400 - (10400 - 9200) * frac)
            soc = int(30 - (30 - 15) * frac)
            plant_inject_voltage(_mqtt_client, v, soc)
            _scaled_sleep(0.1)

        # Phase 3: voltage drops from 9.2 V to 7.0 V (critical_UV) over 2.5 s
        # Crosses plant-sim critical threshold (9000 mV) for status=0
        for i in range(25):
            frac = i / 24.0
            v = int(9200 - (9200 - 7000) * frac)
            soc = int(15 - (15 - 1) * frac)
            plant_inject_voltage(_mqtt_client, v, soc)
            _scaled_sleep(0.1)

        # DTC 0xE401 now sent by RZC firmware (Swc_Battery.c) when it
        # detects DISABLE_LOW status — no belt-and-suspenders needed.
    finally:
        bus.shutdown()
    return ("Battery drain: 12.6 V -> 7.0 V over 7.5 s via MQTT.  "
            "RZC detects undervoltage -> DTC 0xE401.  "
            "Plant sim restores normal values within ~8 s after scenario ends.")


def motor_reversal() -> str:
    """Inject motor fault while driving forward via plant-sim MQTT.

    Establishes forward drive via SPI pedal override at 80%, lets the
    motor spin up, then injects a motor stall + overcurrent fault via
    MQTT.  This simulates a sudden mechanical blockage (as would occur
    during an attempted reversal under load).

    Bypasses CAN cyclic-TX override problem — CVC owns 0x101 and
    overwrites any injected Torque_Request frames within one tick.

    Maps to HE-014 (ASIL C): Motor direction reversal during forward motion.
    """
    if _mqtt_client is None:
        return "Motor reversal: MQTT client not available"
    send_pedal_override(pedal_pct_to_angle(80))
    _scaled_sleep(1.0)  # let motor reach speed
    plant_inject_stall(_mqtt_client)
    _scaled_sleep(0.1)
    plant_inject_overcurrent(_mqtt_client)
    return ("Motor reversal: pedal 80% (SPI) + MQTT inject_stall + "
            "inject_overcurrent.  Simulates mechanical blockage -> SAFE_STOP.")


def unintended_braking() -> str:
    """Inject unexpected emergency braking while cruising.

    Sends a Brake_Command with 100% force in emergency mode while the
    vehicle is driving at normal speed.  Simulates a brake actuator
    fault or spurious brake command.

    Maps to HE-006 (ASIL A): Unintended braking during normal driving.
    """
    bus = _get_bus()
    try:
        # Establish normal drive first
        _send(bus, CAN_TORQUE_REQUEST, _torque_frame(50, 1))
        _send(bus, CAN_BRAKE_COMMAND, _brake_frame(0, brake_mode=0))
        _scaled_sleep(0.5)
        # Sudden emergency brake
        _send(bus, CAN_BRAKE_COMMAND, _brake_frame(100, brake_mode=2))
    finally:
        bus.shutdown()
    return ("Unintended braking: emergency brake at 100% while cruising.  "
            "Simulates spurious brake actuator activation.")


def torque_loss() -> str:
    """Inject sudden loss of drive torque while driving.

    Sends torque request dropping to 0% with direction=stop while the
    vehicle is in motion.  Simulates motor controller failure or
    power stage dropout.

    Maps to HE-002 (ASIL B): Loss of drive torque during driving.
    """
    bus = _get_bus()
    try:
        # Establish normal drive
        _send(bus, CAN_TORQUE_REQUEST, _torque_frame(50, 1))
        _scaled_sleep(0.5)
        # Sudden torque loss
        _send(bus, CAN_TORQUE_REQUEST, _torque_frame(0, 0))
    finally:
        bus.shutdown()
    return ("Torque loss: torque dropped to 0% mid-drive.  "
            "Simulates motor controller failure / power stage dropout.")


def runaway_accel() -> str:
    """Inject runaway acceleration via SPI pedal override at 100%.

    Overrides the pedal sensor at the MCAL layer with maximum position,
    so the CVC processes it through its full pipeline (plausibility
    check, ramp limit, torque lookup).  The CVC naturally generates
    the Torque_Request CAN frame.

    This bypasses the CAN cyclic-TX override problem — the pedal value
    enters at the SPI layer, not via CAN frame injection.

    Maps to HE-016 (ASIL C): Unintended acceleration at high speed.
    """
    send_pedal_override(pedal_pct_to_angle(100))
    return ("Runaway acceleration: pedal 100% (SPI override).  "
            "CVC processes through full pipeline -> torque limiting -> DEGRADED.")


def creep_from_stop() -> str:
    """Inject motor current at standstill to simulate BTS7960 FET short.

    Fault model MB-006: BTS7960 FET gate-source short causes motor
    current flow despite zero torque command from CVC.  Pedal stays
    in dead-zone (torque=0), but plant-sim injects 1000mA motor current.

    SC creep guard (SSR-SC-018, SM-024) detects torque=0 AND current>500mA
    for 2 cycles (20ms) → kill relay → SAFE_STOP.

    Maps to HE-017 (ASIL D): Unintended vehicle motion from stationary.
    SG-001: Prevent unintended acceleration/motion from motor control fault.
    """
    if _mqtt_client is None:
        return "Creep from stop: MQTT client not available"
    # No pedal input — torque command stays at 0
    # Inject 1000mA motor current via plant-sim (simulates FET short)
    plant_inject_creep_current(_mqtt_client, 1000.0)
    _scaled_sleep(0.5)  # allow plant-sim to process and SC to detect
    return ("Creep from stop: 1000mA motor current injected via plant-sim "
            "(BTS7960 FET short). Torque=0, SC creep guard → kill relay → SAFE_STOP.")


def babbling_node() -> str:
    """Flood the CAN bus with rapid frames simulating a babbling node.

    Sends 200 frames at maximum rate on the Torque_Request CAN ID,
    saturating bus bandwidth.  A babbling node can prevent legitimate
    messages from being transmitted, starving safety-critical
    communication.

    Maps to HE-020 (ASIL B): CAN bus babbling node.
    """
    bus = _get_bus()
    try:
        for _ in range(200):
            _send(bus, CAN_TORQUE_REQUEST, _torque_frame(0, 0))
    finally:
        bus.shutdown()
    return ("Babbling node: 200 rapid frames flooded on 0x101.  "
            "Simulates CAN bus saturation from faulty ECU.")


def heartbeat_loss() -> str:
    """Stop the FZC container to trigger SC heartbeat timeout.

    The Safety Controller monitors FZC/RZC heartbeats (0x011, 0x012).
    Stopping the FZC container causes its heartbeat to cease.  After
    ~150 ms (3 missed beats), the SC:
      1. Detects heartbeat timeout
      2. De-energizes the safety relay (kill_reason=HB_TIMEOUT)
      3. Broadcasts relay status on CAN 0x013 (SIL)
      4. Plant-sim disables motor, CVC transitions to SAFE_STOP

    Use 'reset' to restart all containers and recover.
    """
    client = docker.from_env()
    target = "docker-fzc-1"
    try:
        c = client.containers.get(target)
        c.stop(timeout=3)
    except docker.errors.NotFound:
        return f"Heartbeat loss: container {target} not found"
    except Exception as exc:
        return f"Heartbeat loss: failed to stop {target}: {exc}"
    return ("Heartbeat loss: FZC container stopped.  "
            "SC detects HB timeout -> relay kill -> CAN 0x013 -> "
            "plant-sim motor off + CVC SAFE_STOP.")


def sc_relay_kill() -> str:
    """Stop the FZC container to trigger the full SC relay kill chain.

    Identical mechanism to heartbeat_loss, but named to emphasize the
    relay kill path:
      FZC stopped -> SC HB timeout -> relay de-energized ->
      CAN 0x013 broadcast -> plant-sim motor disabled ->
      CVC reads 0x013 via BSW -> EVT_SC_KILL -> SAFE_STOP

    This is the primary demo scenario for the simulated relay feature.
    """
    client = docker.from_env()
    target = "docker-fzc-1"
    try:
        c = client.containers.get(target)
        c.stop(timeout=3)
    except docker.errors.NotFound:
        return f"SC relay kill: container {target} not found"
    except Exception as exc:
        return f"SC relay kill: failed to stop {target}: {exc}"
    return ("SC relay kill: FZC stopped -> SC HB timeout -> relay killed -> "
            "CAN 0x013 -> motor off + SAFE_STOP.  Use 'reset' to recover.")


def can_loss() -> str:
    """Document CAN bus loss injection.

    CAN loss for a specific ECU is triggered by stopping its container:

        docker stop cvc   # CVC goes offline

    This causes all CAN messages from that ECU to cease, which the
    remaining ECUs and the Safety Controller detect via E2E timeout
    monitoring and heartbeat loss.

    For a full bus failure, bring down the virtual CAN interface:

        sudo ip link set vcan0 down

    This scenario is infrastructure-level, not injectable via CAN frames.
    """
    return ("CAN loss: triggered by `docker stop <ecu>` or "
            "`sudo ip link set vcan0 down`.  "
            "Not injectable via CAN frames.")


def estop() -> str:
    """Activate E-Stop via UDP DIO pin injection to CVC.

    CVC reads E-Stop from IoHwAb DIO pin, not from CAN RX.
    The UDP packet triggers IoHwAb_Inject_SetDigitalPin(ESTOP, HIGH)
    in the CVC's Spi_Posix UDP listener (port 9100).
    CVC then broadcasts EStop_Broadcast (0x001) to all ECUs.
    """
    send_estop_activate()
    return "E-Stop activated via UDP DIO injection"


log = logging.getLogger("fault_inject")

# Containers to restart on power-cycle reset.
# Order matters — each restart() blocks until the container is running,
# so sequential restarts accumulate delay.  The SC has a 5-second
# heartbeat grace period, so it MUST restart after the other zone
# controllers (whose heartbeats it monitors) but close enough to the
# CVC restart that its grace hasn't expired by the time CVC boots.
#
# Phase 1: FZC/RZC + simulated ECUs + plant-sim (heartbeat senders first)
# Phase 2: SC (needs FZC/RZC heartbeats during its 5s grace)
# Phase 3: 2s sleep (let SC stabilize)
# Phase 4: CVC (needs SC relay OK + FZC/RZC heartbeats)
_PLANT_CONTAINER = "docker-plant-sim-1"
_ZONE_CONTAINERS = [
    "docker-fzc-1", "docker-rzc-1",
    "docker-bcm-1", "docker-icu-1", "docker-tcu-1",
]
_SC_CONTAINER = "docker-sc-1"
_CVC_CONTAINER = "docker-cvc-1"


def _clear_nvm_files() -> None:
    """Clear NvM persistence files from all ECU containers before restart.

    The POSIX NvM stub stores DTC occurrence counters in /tmp/nvm_block_*.bin.
    These files persist across container restarts (Docker named volumes).
    On boot, Dem_Init reads NVM_BLOCK_SIZE (1024) bytes into the ~224-byte
    dem_events buffer, overflowing 800 bytes into adjacent BSS statics.
    If those statics include Com shadow buffers or RTE signal buffers,
    stale fault values from the previous scenario are restored — causing
    persistent brake_fault=1 / motor_cutoff=1 that never clears.

    Deleting the NvM files ensures a truly clean reset with no DTC carryover.
    """
    client = docker.from_env()
    all_containers = _ZONE_CONTAINERS + [_PLANT_CONTAINER, _SC_CONTAINER, _CVC_CONTAINER]
    for name in all_containers:
        try:
            c = client.containers.get(name)
            c.exec_run("rm -f /tmp/nvm_block_1.bin /tmp/nvm_block_2.bin")
        except Exception:
            pass  # Container may already be stopped; best-effort


def _reset_all_containers() -> list[str]:
    """Kill ALL ECU containers, then start them in correct order.

    Two-phase kill-then-start eliminates the timing race where an old
    container (e.g. CVC in SAFE_STOP sending brake_cmd=100%) keeps
    running while freshly restarted containers (e.g. FZC) receive its
    stale commands — causing spurious PWM deviation faults.

    Uses kill() instead of stop() — these are stateless firmware processes
    with no persistent state that needs graceful shutdown.  Reduces reset
    time from ~40s to ~5s.

    Phase 1: Kill all ECU + plant-sim containers in parallel
    Phase 2: Start zone controllers + plant-sim (heartbeat senders)
    Phase 3: Start SC (needs heartbeats during its 5s grace)
    Phase 4: Wait 2s, then start CVC (needs SC relay OK + heartbeats)
    """
    import concurrent.futures

    client = docker.from_env()
    # Plant-sim IS restarted — MQTT reset alone isn't sufficient because
    # CVC may still be sending stale commands (e.g. steer=-45° from SAFE_STOP)
    # between the MQTT reset and the container kill, causing plant-sim to
    # re-track to stale values. Restarting plant-sim ensures clean physics.
    all_ecu_names = _ZONE_CONTAINERS + [_PLANT_CONTAINER, _SC_CONTAINER, _CVC_CONTAINER]
    restarted: list[str] = []

    # Phase 1: stop ALL ECU containers in parallel.
    # Plant-sim stays running — it has already received the MQTT reset
    # and is publishing neutral CAN frames (brake=0%, steer=0°).
    def _stop_container(name: str) -> None:
        try:
            c = client.containers.get(name)
            c.stop(timeout=2)
        except docker.errors.NotFound:
            log.warning("Container %s not found — skipping stop", name)
        except docker.errors.APIError:
            pass  # Container may already be stopped
        except Exception as exc:
            log.warning("Failed to stop %s: %s", name, exc)

    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        pool.map(_stop_container, all_ecu_names)

    # Phase 2a: start plant-sim FIRST so it publishes neutral sensors
    # before zone ECUs boot and read them
    try:
        c = client.containers.get(_PLANT_CONTAINER)
        c.start()
        restarted.append(_PLANT_CONTAINER)
    except docker.errors.NotFound:
        log.warning("Container %s not found — skipping start", _PLANT_CONTAINER)
    except Exception as exc:
        log.warning("Failed to start %s: %s", _PLANT_CONTAINER, exc)

    # Brief pause — let plant-sim's neutral frames propagate on CAN bus
    _scaled_sleep(1)

    # Phase 2b: start zone controllers (heartbeat senders)
    for name in _ZONE_CONTAINERS:
        try:
            c = client.containers.get(name)
            c.start()
            restarted.append(name)
        except docker.errors.NotFound:
            log.warning("Container %s not found — skipping start", name)
        except Exception as exc:
            log.warning("Failed to start %s: %s", name, exc)

    # Phase 3: start SC (after heartbeat senders are running)
    try:
        c = client.containers.get(_SC_CONTAINER)
        c.start()
        restarted.append(_SC_CONTAINER)
    except docker.errors.NotFound:
        log.warning("Container %s not found — skipping start", _SC_CONTAINER)
    except Exception as exc:
        log.warning("Failed to start %s: %s", _SC_CONTAINER, exc)

    # Phase 4: wait for SC to boot, then start CVC last
    _scaled_sleep(2)
    try:
        c = client.containers.get(_CVC_CONTAINER)
        c.start()
        restarted.append(_CVC_CONTAINER)
    except docker.errors.NotFound:
        log.warning("Container %s not found — skipping start", _CVC_CONTAINER)
    except Exception as exc:
        log.warning("Failed to start %s: %s", _CVC_CONTAINER, exc)

    return restarted


def reset() -> str:
    """Power-cycle reset: restart ECU containers to clear all latched faults.

    Order is critical — plant-sim must be at neutral BEFORE any ECU boots:
    1. Kill ALL ECU containers — stops stale CAN commands (brake=100%, steer=-45°)
    2. Reset plant-sim via MQTT — with no ECUs sending, physics settles to neutral
    3. Wait 1s for plant-sim to publish neutral CAN frames on vcan0
    4. Start ECU containers in phased order — they read neutral on first cycle
    """
    import concurrent.futures
    clear_pedal_override()
    send_estop_clear()
    _clear_nvm_files()

    client = docker.from_env()
    all_ecu_names = _ZONE_CONTAINERS + [_SC_CONTAINER, _CVC_CONTAINER]

    # Phase 1: Kill ALL ECU containers (plant-sim stays running)
    def _stop(name):
        try:
            client.containers.get(name).stop(timeout=2)
        except Exception:
            pass
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        pool.map(_stop, all_ecu_names)

    # Phase 2: Reset plant-sim physics — ECUs are dead, no one fights the reset
    if _mqtt_client is not None:
        reset_payload = json.dumps({"action": "reset", "ts": time.time()})
        _mqtt_client.publish("taktflow/command/reset", reset_payload, qos=1)
        reset_plant_faults(_mqtt_client)

    # Phase 3: Wait for plant-sim to settle at neutral (brake=0%, steer=0°)
    _scaled_sleep(2)

    # Phase 4: Start zone controllers (heartbeat senders)
    restarted = []
    for name in _ZONE_CONTAINERS:
        try:
            client.containers.get(name).start()
            restarted.append(name)
        except Exception as exc:
            log.warning("Failed to start %s: %s", name, exc)

    # Phase 5: Start SC (needs heartbeats)
    try:
        client.containers.get(_SC_CONTAINER).start()
        restarted.append(_SC_CONTAINER)
    except Exception as exc:
        log.warning("Failed to start %s: %s", _SC_CONTAINER, exc)

    # Phase 6: Wait for SC to boot, then start CVC
    _scaled_sleep(2)
    try:
        client.containers.get(_CVC_CONTAINER).start()
        restarted.append(_CVC_CONTAINER)
    except Exception as exc:
        log.warning("Failed to start %s: %s", _CVC_CONTAINER, exc)

    log.info("Power-cycle reset: restarted %d containers", len(restarted))

    return f"Power-cycle reset: {len(restarted)} containers restarted"


# ---------------------------------------------------------------------------
# Registry — maps scenario name to (function, description)
# ---------------------------------------------------------------------------

SCENARIOS: dict[str, dict] = {
    "normal_drive": {
        "fn": normal_drive,
        "description": (
            "Normal drive: pedal 50% (SPI override), steer 0 deg, brake 0%.  "
            "CVC processes pedal through full pipeline -> Torque_Request CAN."
        ),
    },
    "overcurrent": {
        "fn": overcurrent,
        "description": (
            "Overcurrent: pedal 95% (SPI) + MQTT inject_overcurrent to plant-sim.  "
            "Plant sim reports overcurrent on CAN -> DTC 0xE301 -> SAFE_STOP."
        ),
    },
    "steer_fault": {
        "fn": steer_fault,
        "description": (
            "Steering fault: MQTT inject to plant-sim sets steering fault.  "
            "DTC 0xD001 broadcast + physics cascade -> SAFE_STOP."
        ),
    },
    "brake_fault": {
        "fn": brake_fault,
        "description": (
            "Brake fault: MQTT inject to plant-sim sets brake fault.  "
            "DTC 0xE202 broadcast + physics cascade -> SAFE_STOP."
        ),
    },
    "motor_overtemp": {
        "fn": motor_overtemp,
        "description": (
            "Motor overtemp: ramp temp 25->110 C over 4s via MQTT.  "
            "Derating at 80 C, shutdown at 100 C."
        ),
    },
    "battery_low": {
        "fn": battery_low,
        "description": (
            "Battery drain: voltage 12.6 V -> 7.0 V over 7.5 s via MQTT.  "
            "RZC detects undervoltage -> DTC 0xE401 -> DEGRADED/LIMP."
        ),
    },
    "motor_reversal": {
        "fn": motor_reversal,
        "description": (
            "Motor reversal: pedal 80% (SPI) + MQTT inject stall+overcurrent.  "
            "Simulates mechanical blockage at speed -> SAFE_STOP."
        ),
    },
    "unintended_braking": {
        "fn": unintended_braking,
        "description": (
            "Unintended braking: emergency brake at 100% while cruising.  "
            "Simulates spurious brake actuator activation."
        ),
    },
    "torque_loss": {
        "fn": torque_loss,
        "description": (
            "Torque loss: torque dropped to 0% mid-drive.  "
            "Simulates motor controller failure / power stage dropout."
        ),
    },
    "runaway_accel": {
        "fn": runaway_accel,
        "description": (
            "Runaway acceleration: pedal 100% (SPI override).  "
            "CVC processes through full pipeline -> torque limiting -> DEGRADED."
        ),
    },
    "creep_from_stop": {
        "fn": creep_from_stop,
        "description": (
            "Creep from stop: BTS7960 FET short — 1000mA motor current with "
            "torque=0.  SC creep guard detects cross-plausibility → kill relay → SAFE_STOP."
        ),
    },
    "babbling_node": {
        "fn": babbling_node,
        "description": (
            "Babbling node: 200 rapid frames flooded on 0x101.  "
            "Simulates CAN bus saturation from faulty ECU."
        ),
    },
    "heartbeat_loss": {
        "fn": heartbeat_loss,
        "description": (
            "Heartbeat loss: stops FZC container -> SC detects HB timeout "
            "-> relay kill (CAN 0x013) -> motor off + SAFE_STOP."
        ),
    },
    "sc_relay_kill": {
        "fn": sc_relay_kill,
        "description": (
            "SC relay kill: stops FZC -> SC HB timeout -> relay de-energized "
            "-> CAN 0x013 -> motor off + CVC SAFE_STOP.  Full safety chain demo."
        ),
    },
    "can_loss": {
        "fn": can_loss,
        "description": (
            "CAN bus loss: stop a container or bring down vcan0.  "
            "Not injectable via CAN."
        ),
    },
    "estop": {
        "fn": estop,
        "description": (
            "Emergency stop: sends EStop_Active=1 on 0x001.  "
            "All actuators disabled, vehicle enters SAFE_STOP."
        ),
    },
    "reset": {
        "fn": reset,
        "description": (
            "Power-cycle reset: restarts all ECU + plant-sim containers "
            "to clear latched firmware faults. Full clean boot."
        ),
    },
}
