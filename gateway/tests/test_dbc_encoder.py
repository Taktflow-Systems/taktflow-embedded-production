"""Unit tests for DBC encoder + E2E.

Verifies Python encoder produces bytes identical to firmware E2E_Protect.
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lib.dbc_encoder import CanEncoder, crc8_j1850


class TestCrc8J1850(unittest.TestCase):
    """CRC-8 must match firmware E2E_ComputePduCrc."""

    def test_known_cvc_heartbeat(self):
        """CVC Heartbeat: payload=[ECU_ID=1, Mode=0], DataId=2 → CRC=0x44."""
        # CRC input: payload[2:] + DataId = [1, 0, 2]
        # Confirmed from CAN capture: byte1=0x44
        self.assertEqual(crc8_j1850(bytes([1, 0, 2])), 0x44)

    def test_empty_payload(self):
        """CRC of just DataId byte."""
        crc = crc8_j1850(bytes([0x05]))
        self.assertIsInstance(crc, int)
        self.assertTrue(0 <= crc <= 255)

    def test_xor_out_applied(self):
        """Without XOR-out, result would differ."""
        # Compute without XOR-out
        data = bytes([1, 2, 3])
        crc_no_xor = 0xFF
        for b in data:
            crc_no_xor ^= b
            for _ in range(8):
                if crc_no_xor & 0x80:
                    crc_no_xor = ((crc_no_xor << 1) ^ 0x1D) & 0xFF
                else:
                    crc_no_xor = (crc_no_xor << 1) & 0xFF

        crc_with_xor = crc8_j1850(data)
        self.assertEqual(crc_with_xor, crc_no_xor ^ 0xFF)

    def test_different_data_different_crc(self):
        self.assertNotEqual(crc8_j1850(bytes([1, 2])), crc8_j1850(bytes([3, 4])))


class TestCanEncoder(unittest.TestCase):
    """DBC encoder produces correct CAN frames."""

    def setUp(self):
        self.enc = CanEncoder()

    def test_load_dbc(self):
        self.assertGreater(len(self.enc.db.messages), 0)

    def test_get_id(self):
        self.assertEqual(self.enc.get_id("CVC_Heartbeat"), 0x010)
        self.assertEqual(self.enc.get_id("Motor_Status"), 0x300)

    def test_get_dlc(self):
        self.assertEqual(self.enc.get_dlc("CVC_Heartbeat"), 4)
        self.assertEqual(self.enc.get_dlc("Vehicle_State"), 6)

    def test_is_e2e(self):
        self.assertTrue(self.enc.is_e2e("CVC_Heartbeat"))
        self.assertFalse(self.enc.is_e2e("Body_Control_Cmd"))

    def test_encode_heartbeat_dlc(self):
        data = self.enc.encode("CVC_Heartbeat", {
            "CVC_Heartbeat_ECU_ID": 1,
            "CVC_Heartbeat_OperatingMode": 0,
            "CVC_Heartbeat_FaultStatus": 0,
        })
        self.assertEqual(len(data), 4)

    def test_encode_heartbeat_e2e_header(self):
        data = self.enc.encode("CVC_Heartbeat", {
            "CVC_Heartbeat_ECU_ID": 1,
            "CVC_Heartbeat_OperatingMode": 0,
            "CVC_Heartbeat_FaultStatus": 0,
        })
        # DataId=2 → byte0 low nibble = 2
        self.assertEqual(data[0] & 0x0F, 2)
        # Alive counter = 1 (first call) → byte0 high nibble = 1
        self.assertEqual((data[0] >> 4) & 0x0F, 1)
        # CRC non-zero
        self.assertNotEqual(data[1], 0)

    def test_encode_heartbeat_crc_matches_firmware(self):
        """CVC Heartbeat with ECU_ID=1, Mode=0 → CRC=0x44."""
        data = self.enc.encode("CVC_Heartbeat", {
            "CVC_Heartbeat_ECU_ID": 1,
            "CVC_Heartbeat_OperatingMode": 0,
            "CVC_Heartbeat_FaultStatus": 0,
        })
        self.assertEqual(data[1], 0x44)

    def test_encode_alive_counter_increments(self):
        d1 = self.enc.encode("Motor_Status", {
            "Motor_Status_TorqueEcho": 0,
            "Motor_Status_MotorSpeed_RPM": 0,
            "Motor_Status_MotorDirection": 0,
            "Motor_Status_MotorEnable": 0,
            "Motor_Status_MotorFaultStatus": 0,
        })
        d2 = self.enc.encode("Motor_Status", {
            "Motor_Status_TorqueEcho": 0,
            "Motor_Status_MotorSpeed_RPM": 0,
            "Motor_Status_MotorDirection": 0,
            "Motor_Status_MotorEnable": 0,
            "Motor_Status_MotorFaultStatus": 0,
        })
        c1 = (d1[0] >> 4) & 0x0F
        c2 = (d2[0] >> 4) & 0x0F
        self.assertEqual((c1 + 1) & 0x0F, c2)

    def test_encode_non_e2e_message(self):
        data = self.enc.encode("Body_Control_Cmd", {
            "Body_Control_Cmd_HeadlightCmd": 1,
            "Body_Control_Cmd_TailLightOn": 0,
            "Body_Control_Cmd_HazardActive": 0,
            "Body_Control_Cmd_TurnSignalCmd": 0,
            "Body_Control_Cmd_DoorLockCmd": 0,
        })
        self.assertEqual(len(data), 4)

    def test_corrupt_crc(self):
        d_good = self.enc.encode("CVC_Heartbeat", {
            "CVC_Heartbeat_ECU_ID": 1,
            "CVC_Heartbeat_OperatingMode": 0,
            "CVC_Heartbeat_FaultStatus": 0,
        })
        # Reset counter for fair comparison
        self.enc._alive_counters[0x010] = 0
        d_bad = self.enc.encode("CVC_Heartbeat", {
            "CVC_Heartbeat_ECU_ID": 1,
            "CVC_Heartbeat_OperatingMode": 0,
            "CVC_Heartbeat_FaultStatus": 0,
        }, corrupt_crc=True)
        self.assertNotEqual(d_good[1], d_bad[1])

    def test_verify_e2e_good(self):
        data = self.enc.encode("CVC_Heartbeat", {
            "CVC_Heartbeat_ECU_ID": 1,
            "CVC_Heartbeat_OperatingMode": 0,
            "CVC_Heartbeat_FaultStatus": 0,
        })
        self.assertTrue(self.enc.verify_e2e(0x010, data))

    def test_verify_e2e_bad_crc(self):
        data = bytearray(self.enc.encode("CVC_Heartbeat", {
            "CVC_Heartbeat_ECU_ID": 1,
            "CVC_Heartbeat_OperatingMode": 0,
            "CVC_Heartbeat_FaultStatus": 0,
        }))
        data[1] ^= 0xFF  # Corrupt CRC
        self.assertFalse(self.enc.verify_e2e(0x010, bytes(data)))

    def test_decode_roundtrip(self):
        signals_in = {
            "Vehicle_State_Mode": 2,
            "Vehicle_State_FaultMask": 0x0C,
            "Vehicle_State_TorqueLimit": 50,
            "Vehicle_State_SpeedLimit": 80,
        }
        data = self.enc.encode("Vehicle_State", signals_in)
        signals_out = self.enc.decode(0x100, data)
        self.assertEqual(signals_out["Vehicle_State_Mode"], 2)
        self.assertEqual(signals_out["Vehicle_State_FaultMask"], 0x0C)
        self.assertEqual(signals_out["Vehicle_State_TorqueLimit"], 50)


class TestSubBytePacking(unittest.TestCase):
    """Verify sub-byte signals pack correctly (matches firmware fix)."""

    def setUp(self):
        self.enc = CanEncoder()

    def test_4bit_mode_and_4bit_fault_in_same_byte(self):
        """CVC_Heartbeat: OperatingMode (4-bit) + FaultStatus (4-bit) share byte 3."""
        data = self.enc.encode("CVC_Heartbeat", {
            "CVC_Heartbeat_ECU_ID": 1,
            "CVC_Heartbeat_OperatingMode": 5,
            "CVC_Heartbeat_FaultStatus": 3,
        })
        decoded = self.enc.decode(0x010, data)
        self.assertEqual(decoded["CVC_Heartbeat_OperatingMode"], 5)
        self.assertEqual(decoded["CVC_Heartbeat_FaultStatus"], 3)

    def test_12bit_faultmask_and_4bit_mode(self):
        """Vehicle_State: Mode (4-bit) + FaultMask (12-bit) share byte 2-3."""
        data = self.enc.encode("Vehicle_State", {
            "Vehicle_State_Mode": 3,
            "Vehicle_State_FaultMask": 0xC0,  # FZC+RZC timeout
            "Vehicle_State_TorqueLimit": 0,
            "Vehicle_State_SpeedLimit": 0,
        })
        decoded = self.enc.decode(0x100, data)
        self.assertEqual(decoded["Vehicle_State_Mode"], 3)
        self.assertEqual(decoded["Vehicle_State_FaultMask"], 0xC0)


if __name__ == "__main__":
    unittest.main()
