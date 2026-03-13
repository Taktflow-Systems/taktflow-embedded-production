#!/usr/bin/env python3
"""
HIL Rest-Bus Simulation — sends nominal virtual sensor CAN frames to ECUs.

Sends CAN 0x600 (FZC virtual sensors) and CAN 0x601 (RZC virtual sensors)
at 10ms intervals with nominal values so that firmware built with
-DPLATFORM_HIL receives safe sensor readings via IoHwAb_Hil_SetOverride().

No E2E (CRC8/alive) needed — these PDUs bypass E2E on the ECU side.

Signal layout (little-endian uint16):
  CAN 0x600:  [steer_angle:16][brake_pos:16][brake_current:16][reserved:16]
  CAN 0x601:  [motor_current:16][motor_temp:16][battery_voltage:16][motor_rpm:16]

Usage:
  python3 hil_restbus.py                     # defaults: can0, nominal values
  python3 hil_restbus.py --interface can0
  python3 hil_restbus.py --battery 11000     # custom battery voltage (mV)

Requires: python-can (pip install python-can)
"""

import argparse
import struct
import time
import signal
import sys

import can


def build_fzc_frame(steer_angle, brake_pos, brake_current):
    """Build CAN 0x600 payload — FZC virtual sensors."""
    return struct.pack('<HHHxx', steer_angle, brake_pos, brake_current)


def build_rzc_frame(motor_current, motor_temp, battery_voltage, motor_rpm):
    """Build CAN 0x601 payload — RZC virtual sensors."""
    return struct.pack('<HHHH', motor_current, motor_temp, battery_voltage, motor_rpm)


def main():
    parser = argparse.ArgumentParser(description='HIL rest-bus simulation')
    parser.add_argument('--interface', default='can0', help='CAN interface (default: can0)')
    parser.add_argument('--period-ms', type=int, default=10, help='TX period in ms (default: 10)')
    parser.add_argument('--steer-angle', type=int, default=8191, help='Steering angle 14-bit raw (default: 8191 = center)')
    parser.add_argument('--brake-pos', type=int, default=0, help='Brake position 0-1000 (default: 0 = no brake)')
    parser.add_argument('--motor-current', type=int, default=0, help='Motor current mA (default: 0)')
    parser.add_argument('--motor-temp', type=int, default=250, help='Motor temp deci-C (default: 250 = 25.0C)')
    parser.add_argument('--battery', type=int, default=12600, help='Battery voltage mV (default: 12600)')
    parser.add_argument('--motor-rpm', type=int, default=0, help='Motor RPM (default: 0)')
    args = parser.parse_args()

    bus = can.interface.Bus(channel=args.interface, interface='socketcan')

    fzc_msg = can.Message(
        arbitration_id=0x600,
        data=build_fzc_frame(args.steer_angle, args.brake_pos, 0),
        is_extended_id=False
    )

    rzc_msg = can.Message(
        arbitration_id=0x601,
        data=build_rzc_frame(args.motor_current, args.motor_temp, args.battery, args.motor_rpm),
        is_extended_id=False
    )

    period_s = args.period_ms / 1000.0
    running = True

    def stop(sig, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    print(f"HIL rest-bus: sending on {args.interface} every {args.period_ms}ms")
    print(f"  0x600 FZC: steer={args.steer_angle}, brake={args.brake_pos}")
    print(f"  0x601 RZC: current={args.motor_current}mA, temp={args.motor_temp}dC, "
          f"batt={args.battery}mV, rpm={args.motor_rpm}")
    print("Press Ctrl+C to stop.")

    count = 0
    while running:
        t0 = time.monotonic()
        try:
            bus.send(fzc_msg)
            bus.send(rzc_msg)
        except can.CanError as e:
            print(f"CAN send error: {e}", file=sys.stderr)

        count += 1
        if count % 1000 == 0:
            print(f"  ... {count} frames sent")

        elapsed = time.monotonic() - t0
        sleep_time = period_s - elapsed
        if sleep_time > 0:
            time.sleep(sleep_time)

    bus.shutdown()
    print(f"\nStopped after {count} frames.")


if __name__ == '__main__':
    main()
