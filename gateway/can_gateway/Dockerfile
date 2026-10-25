FROM python:3.11-slim

WORKDIR /app

COPY gateway/can_gateway/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/taktflow_vehicle.dbc /app/taktflow_vehicle.dbc
COPY gateway/can_gateway/ /app/can_gateway/

RUN useradd -r -s /usr/sbin/nologin appuser && chown -R appuser /app

ENV DBC_PATH=/app/taktflow_vehicle.dbc
ENV CAN_CHANNEL=vcan0
ENV MQTT_HOST=localhost
ENV MQTT_PORT=1883

USER appuser
CMD ["python", "-m", "can_gateway.main"]

ARG GIT_SHA=unknown
LABEL org.opencontainers.image.revision=$GIT_SHA
