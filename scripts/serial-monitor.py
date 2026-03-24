#!/usr/bin/env python3
"""Simple serial monitor — reads and prints lines from a COM port.

Usage:
    python scripts/serial-monitor.py                    # COM11 @ 115200 (default)
    python scripts/serial-monitor.py COM3 9600          # custom port and baud
    python scripts/serial-monitor.py --log sc_debug.log # save to file
"""
import argparse
import sys
import time
import serial


def main():
    parser = argparse.ArgumentParser(description="Serial Monitor")
    parser.add_argument("port", nargs="?", default="COM11", help="Serial port (default: COM11)")
    parser.add_argument("baud", nargs="?", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--log", default=None, help="Log to file")
    parser.add_argument("--timeout", type=float, default=None, help="Stop after N seconds")
    args = parser.parse_args()

    log_file = None
    if args.log:
        log_file = open(args.log, "a", encoding="utf-8")
        print(f"Logging to {args.log}")

    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)

    print(f"Connected to {args.port} @ {args.baud}. Press Ctrl+C to exit.\n")

    start = time.time()
    try:
        while True:
            if args.timeout and (time.time() - start) > args.timeout:
                break
            line = ser.readline().decode(errors="replace").strip()
            if line:
                ts = time.strftime("%H:%M:%S")
                output = f"[{ts}] {line}"
                print(output)
                if log_file:
                    log_file.write(output + "\n")
                    log_file.flush()
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()
        if log_file:
            log_file.close()


if __name__ == "__main__":
    main()
