FROM python:3.11-slim

WORKDIR /app

COPY gateway/fault_inject/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/fault_inject/ /app/fault_inject/
COPY gateway/lib/ /app/lib/
COPY gateway/taktflow_vehicle.dbc /app/taktflow_vehicle.dbc
RUN touch /app/__init__.py

RUN useradd -r -s /usr/sbin/nologin appuser && chown -R appuser /app

ENV CAN_CHANNEL=vcan0
ENV FAULT_PORT=8091
ENV DBC_PATH=/app/taktflow_vehicle.dbc

USER appuser
CMD ["python", "-m", "fault_inject.app"]

ARG GIT_SHA=unknown
LABEL org.opencontainers.image.revision=$GIT_SHA
