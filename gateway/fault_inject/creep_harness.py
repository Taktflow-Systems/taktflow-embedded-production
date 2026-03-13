"""Creep-from-stop diagnostic harness.

Monitors CAN bus state transitions and pedal/torque signals during the
creep_from_stop scenario to identify why CVC does or doesn't reach SAFE_STOP.

Run on VPS:
    python3 -m fault_inject.creep_harness
"""

import can
import socket
import struct
import time

VEHICLE_STATES = {0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP", 4: "SAFE_STOP", 5: "SHUTDOWN"}

# CAN IDs to monitor
CAN_VEHICLE_STATE = 0x100    # CVC -> all: VehicleState byte 2, torque% byte 4
CAN_TORQUE_REQ    = 0x101    # CVC -> RZC: torque request
CAN_SC_RELAY      = 0x013    # SC: mode/fault/kill
CAN_MOTOR_CURRENT = 0x301    # plant-sim: motor current
CAN_BATTERY       = 0x303    # plant-sim: battery status
CAN_ESTOP         = 0x001    # E-Stop


def run_harness(duration: float = 30.0, inject_at: float = 10.0):
    """Monitor CAN and inject pedal override at inject_at seconds."""
    bus = can.interface.Bus(channel="vcan0", interface="socketcan")

    last_state = -1
    start = time.time()
    injected = False

    print(f"[CREEP HARNESS] Monitoring for {duration}s, pedal inject at t={inject_at}s")
    print(f"{'t(s)':>6} | {'Event':<60}")
    print("-" * 70)

    try:
        while (time.time() - start) < duration:
            msg = bus.recv(timeout=0.05)
            t = time.time() - start

            # Inject pedal at the right time
            if not injected and t >= inject_at:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                angle = (300 * 16383) // 1000  # 30% pedal
                sock.sendto(struct.pack("<H", angle & 0x3FFF), ("127.0.0.1", 9100))
                sock.close()
                injected = True
                print(f"{t:6.1f} | >>> PEDAL OVERRIDE 30% (angle={angle})")

            if msg is None:
                continue

            aid = msg.arbitration_id
            d = msg.data

            if aid == CAN_VEHICLE_STATE and len(d) >= 5:
                state = d[2]
                torque = d[4]
                if state != last_state:
                    sname = VEHICLE_STATES.get(state, f"?{state}")
                    print(f"{t:6.1f} | STATE: {VEHICLE_STATES.get(last_state, '?')} -> {sname}  (torque={torque}%)")
                    last_state = state
                elif torque > 0 and (int(t * 10) % 10 == 0):  # log torque every 1s if non-zero
                    print(f"{t:6.1f} |   torque={torque}%  state={VEHICLE_STATES.get(state, '?')}")

            elif aid == CAN_TORQUE_REQ and len(d) >= 5:
                torque_req = d[2]
                if torque_req > 0 and (int(t * 10) % 5 == 0):
                    print(f"{t:6.1f} |   TORQUE_REQ={torque_req}%")

            elif aid == CAN_SC_RELAY and len(d) >= 4:
                mode = d[0]
                fault = d[1]
                kill = d[2]
                alive = d[3]
                if kill > 0 or fault > 0:
                    print(f"{t:6.1f} | SC: mode={mode} fault=0x{fault:02X} kill={kill} alive={alive}")

            elif aid == CAN_MOTOR_CURRENT and len(d) >= 4:
                current = d[2] | (d[3] << 8)
                if current > 0 and (int(t * 10) % 10 == 0):
                    print(f"{t:6.1f} |   MOTOR_CURRENT={current}mA")

            elif aid == CAN_BATTERY and len(d) >= 4:
                voltage = d[0] | (d[1] << 8)
                soc = d[2]
                if voltage < 10000 and (int(t * 10) % 20 == 0):
                    print(f"{t:6.1f} |   BATTERY: {voltage}mV SOC={soc}%")

            elif aid == CAN_ESTOP and len(d) >= 3:
                active = d[2]
                if active:
                    print(f"{t:6.1f} | ESTOP ACTIVE!")
    finally:
        # Clear pedal override
        if injected:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.sendto(struct.pack("<H", 0xFFFF), ("127.0.0.1", 9100))
            sock.close()
            print(f"{time.time()-start:6.1f} | >>> PEDAL OVERRIDE CLEARED")
        bus.shutdown()

    print(f"\n[CREEP HARNESS] Final state: {VEHICLE_STATES.get(last_state, '?')}")
    if last_state == 4:
        print("[CREEP HARNESS] PASS — reached SAFE_STOP")
    else:
        print(f"[CREEP HARNESS] FAIL — expected SAFE_STOP(4), got {last_state}")


if __name__ == "__main__":
    run_harness()
