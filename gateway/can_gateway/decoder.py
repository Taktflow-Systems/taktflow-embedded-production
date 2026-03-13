"""CAN frame decoder using cantools + taktflow.dbc.

Decodes raw CAN frames into named signal dictionaries.
"""

import logging
from typing import Optional

import can
import cantools

log = logging.getLogger("can_gateway.decoder")

# Vehicle state enum for human-readable names
VEHICLE_STATES = {
    0: "INIT",
    1: "RUN",
    2: "DEGRADED",
    3: "LIMP",
    4: "SAFE_STOP",
    5: "SHUTDOWN",
}

# Messages that use E2E protection (first 2 bytes are E2E header)
E2E_MESSAGES = {
    0x001, 0x010, 0x011, 0x012,  # EStop, heartbeats
    0x100, 0x101, 0x102, 0x103,  # Vehicle state, torque, steer, brake
    0x200, 0x201,                 # Steering/brake status
    0x210, 0x220,                 # Brake fault, lidar
    0x300, 0x301, 0x302,          # Motor status/current/temp
}


class CanDecoder:
    """Decodes CAN frames using the DBC file."""

    def __init__(self, dbc_path: str):
        self.db = cantools.database.load_file(dbc_path)
        self._msg_cache: dict[int, cantools.database.Message] = {}

        # Build lookup cache
        for msg in self.db.messages:
            self._msg_cache[msg.frame_id] = msg

        log.info("Loaded DBC with %d messages", len(self.db.messages))

    def decode(self, frame: can.Message) -> Optional[dict]:
        """Decode a CAN frame into a dict with message name and signals.

        Returns None if the frame ID is not in the DBC or decoding fails.
        """
        msg_def = self._msg_cache.get(frame.arbitration_id)
        if msg_def is None:
            return None

        try:
            signals = msg_def.decode(frame.data, decode_choices=False)
        except Exception as e:
            log.debug("Decode error for 0x%03X: %s", frame.arbitration_id, e)
            return None

        return {
            "msg_name": msg_def.name,
            "msg_id": frame.arbitration_id,
            "signals": signals,
        }

    def get_message_name(self, arb_id: int) -> Optional[str]:
        """Get the DBC message name for a CAN arbitration ID."""
        msg_def = self._msg_cache.get(arb_id)
        return msg_def.name if msg_def else None
