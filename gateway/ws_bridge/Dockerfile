FROM python:3.11-slim

WORKDIR /app

COPY gateway/ws_bridge/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/ws_bridge/ /app/ws_bridge/

RUN useradd -r -s /usr/sbin/nologin appuser && chown -R appuser /app

ENV MQTT_HOST=localhost
ENV MQTT_PORT=1883
ENV WS_PORT=8080

USER appuser
CMD ["python", "-m", "ws_bridge.bridge"]

ARG GIT_SHA=unknown
LABEL org.opencontainers.image.revision=$GIT_SHA
