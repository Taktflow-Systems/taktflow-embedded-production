FROM python:3.11-slim

WORKDIR /app

COPY gateway/can_gateway/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/taktflow.dbc /app/taktflow.dbc
COPY gateway/can_gateway/ /app/can_gateway/

ENV DBC_PATH=/app/taktflow.dbc
ENV CAN_CHANNEL=vcan0
ENV MQTT_HOST=localhost
ENV MQTT_PORT=1883

CMD ["python", "-m", "can_gateway.main"]
