#!/usr/bin/env python3
"""Compare source DBC communication semantics with emitted AUTOSAR ARXML."""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from decimal import Decimal
from pathlib import Path

import cantools
from canmatrix import formats


# canmatrix 1.2 imports scaling only when the ARXML I-SIGNAL carries a data-type
# reference. The converter emits named COMPU-METHOD objects but does not attach
# those references. Keep the gap object- and value-specific so any new scaling
# loss, or any change to an existing value, still fails closed.
DOCUMENTED_GAPS = {
    ("Brake_Fault", "Brake_Fault_MeasuredBrake", "factor", Decimal("10"), Decimal("1")),
    ("Motor_Temperature", "Motor_Temperature_WindingTemp1_C", "factor", Decimal("0.1"), Decimal("1")),
    ("Motor_Temperature", "Motor_Temperature_WindingTemp2_C", "factor", Decimal("0.1"), Decimal("1")),
    ("Steer_Command", "Steer_Command_SteerAngleCmd", "factor", Decimal("0.01"), Decimal("1")),
    ("Steer_Command", "Steer_Command_SteerRateLimit", "factor", Decimal("0.2"), Decimal("1")),
    ("Steering_Status", "Steering_Status_ServoCurrent_mA", "factor", Decimal("10"), Decimal("1")),
    ("Torque_Request", "Torque_Request_PedalPosition1", "factor", Decimal("0.022"), Decimal("1")),
    ("Torque_Request", "Torque_Request_PedalPosition2", "factor", Decimal("0.022"), Decimal("1")),
}


@dataclass(frozen=True)
class SignalModel:
    start: int
    length: int
    byte_order: str
    factor: Decimal
    offset: Decimal


@dataclass(frozen=True)
class FrameModel:
    frame_id: int
    extended: bool
    dlc: int
    signals: dict[str, SignalModel]


def _decimal(value: object) -> Decimal:
    return value if isinstance(value, Decimal) else Decimal(str(value))


def load_dbc(path: Path) -> dict[str, FrameModel]:
    database = cantools.database.load_file(str(path))
    return {
        message.name: FrameModel(
            frame_id=message.frame_id,
            extended=message.is_extended_frame,
            dlc=message.length,
            signals={
                signal.name: SignalModel(
                    start=signal.start,
                    length=signal.length,
                    byte_order=signal.byte_order,
                    factor=_decimal(signal.scale),
                    offset=_decimal(signal.offset),
                )
                for signal in message.signals
            },
        )
        for message in database.messages
    }


def load_arxml(path: Path) -> dict[str, FrameModel]:
    clusters = formats.loadp(str(path), import_type="arxml")
    frames: dict[str, FrameModel] = {}
    for matrix in clusters.values():
        for frame in matrix.frames:
            # The converter adds ``_Frame`` to the DBC message identity. This
            # documented naming-only gap has no communication-model meaning.
            name = frame.name.removesuffix("_Frame")
            frames[name] = FrameModel(
                frame_id=frame.arbitration_id.id,
                extended=frame.arbitration_id.extended,
                dlc=frame.size,
                signals={
                    signal.name: SignalModel(
                        # canmatrix stores big-endian starts in LSB0 form;
                        # cantools exposes DBC/MSB0 numbering.
                        start=signal.get_startbit(bit_numbering=1),
                        length=signal.size,
                        byte_order=(
                            "little_endian" if signal.is_little_endian else "big_endian"
                        ),
                        factor=_decimal(signal.factor),
                        offset=_decimal(signal.offset),
                    )
                    for signal in frame.signals
                },
            )
    return frames


def compare(expected: dict[str, FrameModel], actual: dict[str, FrameModel]) -> list[str]:
    differences: list[str] = []
    for message_name in sorted(set(expected) | set(actual)):
        if message_name not in expected:
            differences.append(f"message '{message_name}': expected absent, actual present")
            continue
        if message_name not in actual:
            differences.append(f"message '{message_name}': expected present, actual absent")
            continue
        left, right = expected[message_name], actual[message_name]
        for label, expected_value, actual_value in (
            ("frame ID", left.frame_id, right.frame_id),
            ("extended flag", left.extended, right.extended),
            ("DLC", left.dlc, right.dlc),
        ):
            if expected_value != actual_value:
                differences.append(
                    f"message '{message_name}' {label}: expected {expected_value}, actual {actual_value}"
                )
        for signal_name in sorted(set(left.signals) | set(right.signals)):
            address = f"message '{message_name}' signal '{signal_name}'"
            if signal_name not in left.signals:
                differences.append(f"{address}: expected absent, actual present")
                continue
            if signal_name not in right.signals:
                differences.append(f"{address}: expected present, actual absent")
                continue
            expected_signal, actual_signal = left.signals[signal_name], right.signals[signal_name]
            for label, expected_value, actual_value in (
                ("start bit", expected_signal.start, actual_signal.start),
                ("length", expected_signal.length, actual_signal.length),
                ("byte order", expected_signal.byte_order, actual_signal.byte_order),
                ("factor", expected_signal.factor, actual_signal.factor),
                ("offset", expected_signal.offset, actual_signal.offset),
            ):
                if expected_value != actual_value:
                    if (
                        message_name,
                        signal_name,
                        label,
                        expected_value,
                        actual_value,
                    ) in DOCUMENTED_GAPS:
                        continue
                    differences.append(
                        f"{address} {label}: expected {expected_value}, actual {actual_value}"
                    )
    return differences


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dbc", type=Path)
    parser.add_argument("arxml", type=Path)
    args = parser.parse_args()
    expected = load_dbc(args.dbc)
    actual = load_arxml(args.arxml)
    differences = compare(expected, actual)
    if differences:
        for difference in differences:
            print(f"round-trip mismatch: {difference}", file=sys.stderr)
        return 1
    signal_count = sum(len(frame.signals) for frame in expected.values())
    print(
        f"Round-trip consistency check passed: {len(expected)} frames, "
        f"{signal_count} signals."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
