"""DBC-based CAN message encoder/decoder with E2E protection.

Single source of truth for encoding CAN messages in plant-sim, fault-inject,
and test scripts. Uses cantools for signal packing (handles sub-byte, signed,
big/little endian automatically). E2E follows AUTOSAR Profile P01:
  - CRC-8 SAE-J1850: poly=0x1D, init=0xFF, xor_out=0xFF
  - Byte 0: [AliveCounter:4 | DataId:4]
  - Byte 1: CRC-8 over (payload[2:] + DataId)

Usage:
    encoder = CanEncoder("gateway/taktflow_vehicle.dbc")
    data = encoder.encode("Motor_Status", {"Motor_Status_MotorRPM": 1500, ...})
    bus.send(can.Message(arbitration_id=encoder.get_id("Motor_Status"), data=data))
"""

import os
from typing import Dict, Optional

import cantools


def crc8_j1850(data: bytes, init: int = 0xFF, poly: int = 0x1D) -> int:
    """CRC-8 SAE-J1850 with XOR-out = 0xFF.

    Matches firmware E2E_ComputePduCrc (E2E.c) and SC sc_crc8 (sc_e2e.c).
    """
    crc = init
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ poly) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc ^ 0xFF  # XOR-out per SAE-J1850


class CanEncoder:
    """Encode/decode CAN messages using DBC + E2E protection."""

    def __init__(self, dbc_path: Optional[str] = None):
        if dbc_path is None:
            # Auto-find DBC relative to this file
            repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
            dbc_path = os.path.join(repo, "gateway", "taktflow_vehicle.dbc")
        self.db = cantools.database.load_file(dbc_path)
        self._alive_counters: Dict[int, int] = {}
        self._e2e_data_ids: Dict[int, int] = {}

        # Extract E2E DataIDs from DBC BA_ attributes
        for msg in self.db.messages:
            if hasattr(msg, 'dbc') and msg.dbc and msg.dbc.attributes:
                did = msg.dbc.attributes.get('E2E_DataID')
                if did is not None:
                    # cantools may return Attribute object or raw value
                    val = did.value if hasattr(did, 'value') else did
                    self._e2e_data_ids[msg.frame_id] = int(val)

    def get_id(self, message_name: str) -> int:
        """Get CAN arbitration ID for a message name."""
        return self.db.get_message_by_name(message_name).frame_id

    def get_dlc(self, message_name: str) -> int:
        """Get DLC for a message name."""
        return self.db.get_message_by_name(message_name).length

    def get_cycle_ms(self, message_name: str) -> float:
        """Get cycle time in ms for a message name."""
        return self.db.get_message_by_name(message_name).cycle_time or 0

    def is_e2e(self, message_name: str) -> bool:
        """Check if a message has E2E protection."""
        msg = self.db.get_message_by_name(message_name)
        return msg.frame_id in self._e2e_data_ids

    def _next_alive(self, can_id: int) -> int:
        """Increment and return 4-bit alive counter for a CAN ID."""
        val = self._alive_counters.get(can_id, 0)
        self._alive_counters[can_id] = (val + 1) & 0x0F
        return self._alive_counters[can_id]

    def encode(self, message_name: str, signals: Dict[str, float],
               corrupt_crc: bool = False, replay_counter: bool = False) -> bytes:
        """Encode a CAN message with signal values and optional E2E.

        Args:
            message_name: DBC message name (e.g., "Motor_Status")
            signals: Dict of signal_name → value (only non-E2E signals)
            corrupt_crc: If True, flip CRC byte (for fault injection)
            replay_counter: If True, don't increment alive counter

        Returns:
            Encoded bytes (DLC length) with E2E header if applicable.
        """
        msg = self.db.get_message_by_name(message_name)
        can_id = msg.frame_id
        data_id = self._e2e_data_ids.get(can_id)

        # Build full signal dict including E2E fields
        full_signals = dict(signals)

        if data_id is not None:
            # E2E-protected message
            if replay_counter:
                alive = self._alive_counters.get(can_id, 0)  # Don't increment
            else:
                alive = self._next_alive(can_id)

            # Add E2E signal values
            for sig in msg.signals:
                if 'E2E_DataID' in sig.name or 'E2E_Data_ID' in sig.name:
                    full_signals[sig.name] = data_id
                elif 'E2E_AliveCounter' in sig.name or 'E2E_Alive_Counter' in sig.name:
                    full_signals[sig.name] = alive
                elif 'E2E_CRC8' in sig.name or 'E2E_CRC_8' in sig.name:
                    full_signals[sig.name] = 0  # Placeholder, computed below

        # Encode via cantools (handles sub-byte packing)
        raw = bytearray(msg.encode(full_signals))

        # Compute and insert E2E CRC
        if data_id is not None:
            # CRC over payload bytes [2:] + DataId
            crc_data = bytes(raw[2:]) + bytes([data_id & 0xFF])
            crc = crc8_j1850(crc_data)
            if corrupt_crc:
                crc ^= 0xFF  # Flip all bits
            raw[1] = crc

        return bytes(raw)

    def decode(self, can_id: int, data: bytes) -> Dict[str, float]:
        """Decode a CAN frame using DBC."""
        msg = self.db.get_message_by_frame_id(can_id)
        return msg.decode(bytes(data), decode_choices=False)

    def verify_e2e(self, can_id: int, data: bytes) -> bool:
        """Verify E2E CRC of a received frame."""
        data_id = self._e2e_data_ids.get(can_id)
        if data_id is None:
            return True  # Not E2E-protected

        if len(data) < 2:
            return False

        # Check DataId
        if (data[0] & 0x0F) != (data_id & 0x0F):
            return False

        # Compute expected CRC
        crc_data = bytes(data[2:]) + bytes([data_id & 0xFF])
        expected_crc = crc8_j1850(crc_data)
        return data[1] == expected_crc

    def all_messages(self):
        """Return list of all message names."""
        return [m.name for m in self.db.messages]

    def e2e_messages(self):
        """Return list of E2E-protected message names."""
        return [m.name for m in self.db.messages if m.frame_id in self._e2e_data_ids]
