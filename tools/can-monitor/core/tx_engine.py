"""Waveshare TX frame builder and UDS helpers."""
from .waveshare import build_tx_frame

# UDS service IDs
UDS_SERVICES = {
    "DiagnosticSessionControl": 0x10,
    "ECUReset": 0x11,
    "ReadDTCInformation": 0x19,
    "ClearDTCInformation": 0x14,
    "TesterPresent": 0x3E,
    "ReadDataByIdentifier": 0x22,
}

# UDS request CAN IDs
UDS_REQ_IDS = {
    "CVC": 0x7E0,
    "FZC": 0x7E1,
    "RZC": 0x7E2,
    "Broadcast": 0x7DF,
}


def build_uds_request(ecu, service_id, sub_function=None, data=None):
    """Build a UDS single-frame request wrapped in Waveshare TX frame.

    Args:
        ecu: ECU name ("CVC", "FZC", "RZC", "Broadcast").
        service_id: UDS service byte (e.g. 0x10).
        sub_function: Optional sub-function byte.
        data: Optional additional data bytes.

    Returns:
        (can_id, tx_bytes) tuple.
    """
    can_id = UDS_REQ_IDS.get(ecu, 0x7DF)
    payload = bytearray()
    uds_data = bytearray([service_id])
    if sub_function is not None:
        uds_data.append(sub_function)
    if data:
        uds_data.extend(data)
    # ISO-TP single frame: PCI = length
    payload.append(len(uds_data))
    payload.extend(uds_data)
    # Pad to 8 bytes
    while len(payload) < 8:
        payload.append(0xAA)  # padding
    return can_id, build_tx_frame(can_id, bytes(payload))


def build_default_session(ecu="CVC"):
    return build_uds_request(ecu, 0x10, 0x01)


def build_extended_session(ecu="CVC"):
    return build_uds_request(ecu, 0x10, 0x03)


def build_ecu_reset(ecu="CVC", reset_type=0x01):
    return build_uds_request(ecu, 0x11, reset_type)


def build_read_dtc(ecu="CVC"):
    return build_uds_request(ecu, 0x19, 0x02, bytes([0xFF]))


def build_clear_dtc(ecu="CVC"):
    return build_uds_request(ecu, 0x14, data=bytes([0xFF, 0xFF, 0xFF]))


def build_tester_present(ecu="CVC"):
    return build_uds_request(ecu, 0x3E, 0x00)
