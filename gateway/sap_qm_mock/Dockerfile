FROM python:3.11-slim

WORKDIR /app

COPY gateway/sap_qm_mock/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/sap_qm_mock/ /app/sap_qm_mock/

RUN useradd -r -s /usr/sbin/nologin appuser && chown -R appuser /app

ENV SAP_PORT=8090
ENV MQTT_HOST=localhost
ENV MQTT_PORT=1883

USER appuser
CMD ["python", "-m", "sap_qm_mock.app"]

ARG GIT_SHA=unknown
LABEL org.opencontainers.image.revision=$GIT_SHA
