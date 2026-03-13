"""CAN-to-MQTT Gateway — main entry point.

Reads CAN frames from vcan0, decodes via DBC, publishes to MQTT.
"""

import asyncio
import logging
import os
import signal
import sys
import time

import can

from .decoder import CanDecoder
from .mqtt_publisher import MqttPublisher

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [CAN-GW] %(levelname)s %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("can_gateway")

# Linux SocketCAN error class bits (include/uapi/linux/can/error.h)
CAN_ERR_FLAG = 0x20000000
CAN_ERR_TX_TIMEOUT = 0x00000001
CAN_ERR_LOSTARB = 0x00000002
CAN_ERR_CRTL = 0x00000004
CAN_ERR_PROT = 0x00000008
CAN_ERR_TRX = 0x00000010
CAN_ERR_ACK = 0x00000020
CAN_ERR_BUSOFF = 0x00000040
CAN_ERR_BUSERROR = 0x00000080
CAN_ERR_RESTARTED = 0x00000100
CAN_ERR_CNT = 0x00000200


def _decode_can_error(msg: can.Message) -> dict:
    """Build a structured error object from a SocketCAN error frame."""
    can_id = int(msg.arbitration_id)
    flags = can_id & ~CAN_ERR_FLAG
    classes: list[str] = []
    if flags & CAN_ERR_TX_TIMEOUT:
        classes.append("TX_TIMEOUT")
    if flags & CAN_ERR_LOSTARB:
        classes.append("LOST_ARB")
    if flags & CAN_ERR_CRTL:
        classes.append("CONTROLLER")
    if flags & CAN_ERR_PROT:
        classes.append("PROTOCOL")
    if flags & CAN_ERR_TRX:
        classes.append("TRANSCEIVER")
    if flags & CAN_ERR_ACK:
        classes.append("ACK")
    if flags & CAN_ERR_BUSOFF:
        classes.append("BUS_OFF")
    if flags & CAN_ERR_BUSERROR:
        classes.append("BUS_ERROR")
    if flags & CAN_ERR_RESTARTED:
        classes.append("RESTARTED")
    if flags & CAN_ERR_CNT:
        classes.append("ERROR_COUNTER")
    if not classes:
        classes.append("UNKNOWN")

    return {
        "raw_can_id": f"0x{can_id:08X}",
        "flags": f"0x{flags:08X}",
        "classes": classes,
        "data_hex": msg.data.hex(),
        "dlc": int(msg.dlc),
        "channel": str(msg.channel) if msg.channel is not None else "",
    }


async def run_gateway():
    dbc_path = os.environ.get(
        "DBC_PATH",
        os.path.join(os.path.dirname(__file__), "..", "taktflow.dbc"),
    )
    can_channel = os.environ.get("CAN_CHANNEL", "vcan0")
    mqtt_host = os.environ.get("MQTT_HOST", "localhost")
    mqtt_port = int(os.environ.get("MQTT_PORT", "1883"))

    # Initialize decoder and publisher
    decoder = CanDecoder(dbc_path)
    publisher = MqttPublisher(mqtt_host, mqtt_port)
    publisher.connect()

    # Wait for MQTT connection
    await asyncio.sleep(1.0)

    # Open CAN bus
    bus = can.interface.Bus(channel=can_channel, interface="socketcan")
    log.info("CAN gateway started on %s -> MQTT %s:%d", can_channel, mqtt_host, mqtt_port)

    total_frames = 0
    decoded_frames = 0
    stats_tick = 0

    try:
        while True:
            loop_start = time.monotonic()

            # Read all available CAN frames (non-blocking)
            batch_count = 0
            while batch_count < 100:  # Process up to 100 frames per tick
                msg = bus.recv(timeout=0)
                if msg is None:
                    break

                total_frames += 1
                batch_count += 1

                if msg.is_error_frame:
                    publisher.publish_can_error(_decode_can_error(msg))
                    continue

                result = decoder.decode(msg)
                if result is not None:
                    decoded_frames += 1
                    result["signals"]["_raw"] = msg.data.hex()
                    publisher.publish_signals(result["msg_name"], result["signals"])

            # Publish stats every tick
            publisher.publish_stats()

            # Log periodic summary
            stats_tick += 1
            if stats_tick >= 500:  # Every 5 seconds
                log.info(
                    "CAN frames: %d total, %d decoded | MQTT rate: %.0f msg/s",
                    total_frames, decoded_frames, publisher.msgs_per_sec,
                )
                stats_tick = 0

            # 10ms cycle
            elapsed = time.monotonic() - loop_start
            sleep_time = 0.01 - elapsed
            if sleep_time > 0:
                await asyncio.sleep(sleep_time)

    except KeyboardInterrupt:
        log.info("CAN gateway stopped")
    finally:
        bus.shutdown()
        publisher.stop()


def main():
    asyncio.run(run_gateway())


if __name__ == "__main__":
    main()
