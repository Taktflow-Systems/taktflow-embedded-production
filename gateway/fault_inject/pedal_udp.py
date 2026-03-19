"""UDP overrides for CVC SPI stub (pedal angle + E-Stop DIO pin).

Sends 2-byte UDP packets to the CVC's Spi_Posix UDP socket to override
the simulated AS5048A pedal sensor angle or trigger the E-Stop DIO pin.

Protocol:
  2 bytes, uint16 LE
  - 0x0000..0x3FFF: target pedal angle (14-bit AS5048A range)
  - 0xE500: activate E-Stop (IoHwAb_Inject_SetDigitalPin → STD_HIGH)
  - 0xE5FF: clear E-Stop (IoHwAb_Inject_SetDigitalPin → STD_LOW)
  - 0xFFFF: clear pedal override (revert to dead-zone oscillation)
"""

import os
import socket
import struct

PEDAL_UDP_PORT = int(os.environ.get("SPI_PEDAL_UDP_PORT", "9100"))
PEDAL_OVERRIDE_CLEAR = 0xFFFF
ESTOP_ACTIVATE = 0xE500
ESTOP_CLEAR = 0xE5FF

# Pedal constants (from Swc_Pedal.c / Cvc_Cfg.h)
_SENSOR_MAX = 16383     # 14-bit AS5048A full range
_POSITION_MAX = 1000    # Internal scaled position (0-1000)
_DEAD_ZONE = 67         # Position below this = torque 0


def pedal_pct_to_angle(pct: float) -> int:
    """Convert pedal percentage (0-100%) to AS5048A raw angle.

    The CVC maps angle -> position: position = (angle * 1000) / 16383
    Then position -> torque via lookup table with dead zone at position < 67.

    Examples:
      10% pedal -> position 100 -> angle 1638
      50% pedal -> position 500 -> angle 8191
      95% pedal -> position 950 -> angle 15563
    """
    position = int(pct * _POSITION_MAX / 100.0)
    # Ensure above dead zone so torque is non-zero
    position = max(_DEAD_ZONE + 1, min(_POSITION_MAX, position))
    angle = (position * _SENSOR_MAX) // _POSITION_MAX
    return min(angle, _SENSOR_MAX)


def send_pedal_override(angle: int,
                        host: str = "127.0.0.1",
                        port: int | None = None) -> None:
    """Send pedal angle override to CVC SPI stub via UDP."""
    if port is None:
        port = PEDAL_UDP_PORT
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(struct.pack("<H", angle & 0x3FFF), (host, port))
    finally:
        sock.close()


def clear_pedal_override(host: str = "127.0.0.1",
                         port: int | None = None) -> None:
    """Clear pedal angle override — revert to dead-zone oscillation."""
    if port is None:
        port = PEDAL_UDP_PORT
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(struct.pack("<H", PEDAL_OVERRIDE_CLEAR), (host, port))
    finally:
        sock.close()


def send_estop_activate(host: str = "127.0.0.1",
                        port: int | None = None) -> None:
    """Activate E-Stop via UDP → CVC IoHwAb DIO pin injection."""
    if port is None:
        port = PEDAL_UDP_PORT
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(struct.pack("<H", ESTOP_ACTIVATE), (host, port))
    finally:
        sock.close()


def send_estop_clear(host: str = "127.0.0.1",
                     port: int | None = None) -> None:
    """Clear E-Stop via UDP → CVC IoHwAb DIO pin injection."""
    if port is None:
        port = PEDAL_UDP_PORT
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(struct.pack("<H", ESTOP_CLEAR), (host, port))
    finally:
        sock.close()
