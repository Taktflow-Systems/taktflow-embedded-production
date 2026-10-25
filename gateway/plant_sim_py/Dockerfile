FROM python:3.11-slim

WORKDIR /app

COPY gateway/plant_sim_py/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/taktflow_vehicle.dbc /app/taktflow_vehicle.dbc
COPY gateway/lib/ /app/gateway/lib/
COPY gateway/plant_sim_py/ /app/plant_sim/
RUN touch /app/gateway/__init__.py
RUN useradd -r -s /usr/sbin/nologin appuser && chown -R appuser /app

ENV DBC_PATH=/app/taktflow_vehicle.dbc
ENV CAN_CHANNEL=vcan0

USER appuser
CMD ["python", "-m", "plant_sim.simulator"]

ARG GIT_SHA=unknown
LABEL org.opencontainers.image.revision=$GIT_SHA
