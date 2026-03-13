"""MQTT helpers for plant-sim fault injection.

Publishes JSON commands to taktflow/command/plant_inject, which the
plant simulator subscribes to and routes to its internal fault methods.

This avoids the CAN cyclic-TX override problem: instead of injecting
CAN frames on IDs that CVC cyclically transmits (0x101/0x102/0x103),
we tell the plant-sim to inject faults at the physics level.
"""

import json
import logging
import time

import paho.mqtt.client as paho_mqtt

log = logging.getLogger("fault_inject")

TOPIC = "taktflow/command/plant_inject"


def inject_overcurrent(mqtt_client: paho_mqtt.Client) -> None:
    """Tell plant-sim to inject motor overcurrent fault."""
    mqtt_client.publish(
        TOPIC, json.dumps({"type": "overcurrent"}), qos=1,
    )
    log.info("Plant inject: overcurrent")


def inject_creep_current(mqtt_client: paho_mqtt.Client,
                         current_ma: float = 1000.0) -> None:
    """Tell plant-sim to inject motor current without overcurrent flag.

    Simulates BTS7960 FET gate-source short: motor draws current
    despite zero torque command.  SC creep guard (SSR-SC-018) detects
    torque=0 AND current>500mA → kill relay.
    """
    mqtt_client.publish(
        TOPIC, json.dumps({"type": "creep_current", "current_ma": current_ma}),
        qos=1,
    )
    log.info("Plant inject: creep_current %.0fmA", current_ma)


def inject_stall(mqtt_client: paho_mqtt.Client) -> None:
    """Tell plant-sim to inject motor stall fault."""
    mqtt_client.publish(
        TOPIC, json.dumps({"type": "stall"}), qos=1,
    )
    log.info("Plant inject: stall")


def inject_voltage(mqtt_client: paho_mqtt.Client, mv: int,
                   soc: int = 10) -> None:
    """Tell plant-sim to override battery voltage."""
    mqtt_client.publish(
        TOPIC, json.dumps({"type": "voltage", "mV": mv, "soc": soc}), qos=1,
    )
    log.info("Plant inject: voltage %dmV, %d%% SOC", mv, soc)


def inject_temp(mqtt_client: paho_mqtt.Client, temp_c: float) -> None:
    """Tell plant-sim to set motor temperature directly."""
    mqtt_client.publish(
        TOPIC, json.dumps({"type": "inject_temp", "temp_c": temp_c}), qos=1,
    )
    log.info("Plant inject: motor temp %.1f°C", temp_c)


def inject_steer_fault(mqtt_client: paho_mqtt.Client) -> None:
    """Tell plant-sim to set steering fault flag."""
    mqtt_client.publish(
        TOPIC, json.dumps({"type": "steer_fault"}), qos=1,
    )
    log.info("Plant inject: steer_fault")


def inject_brake_fault(mqtt_client: paho_mqtt.Client) -> None:
    """Tell plant-sim to set brake fault flag."""
    mqtt_client.publish(
        TOPIC, json.dumps({"type": "brake_fault"}), qos=1,
    )
    log.info("Plant inject: brake_fault")


def reset_plant_faults(mqtt_client: paho_mqtt.Client) -> None:
    """Tell plant-sim to clear all injected faults."""
    mqtt_client.publish(
        TOPIC, json.dumps({"type": "reset"}), qos=1,
    )
    log.info("Plant inject: reset")
