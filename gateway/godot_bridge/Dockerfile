# =============================================================================
# Dockerfile — Godot Bridge (GIL: Godot-in-the-Loop)
#
# CAN↔UDP relay between vECU firmware (SocketCAN) and Godot (UDP on LAN).
# Replaces plant-sim in the GIL stack: Godot provides physics + user input
# (pedal/steering), bridge relays sensor/actuator data to/from vECUs.
#
# System: GIL (Godot-in-the-Loop) on Raspberry Pi
# Counterpart: plant-sim (SIL on VPS — autonomous physics, no Godot)
# =============================================================================

FROM python:3.11-slim

WORKDIR /app

COPY gateway/godot_bridge/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/godot_bridge/bridge.py /app/bridge.py

ENV CAN_INTERFACE=can0
ENV GODOT_HOST=192.168.0.158
ENV BRIDGE_CARS=1
ENV BRIDGE_RATE=100
ENV BRIDGE_VCAN_START=0

CMD python bridge.py \
    --cars ${BRIDGE_CARS} \
    --rate ${BRIDGE_RATE} \
    --vcan-start ${BRIDGE_VCAN_START} \
    --godot-host ${GODOT_HOST}
