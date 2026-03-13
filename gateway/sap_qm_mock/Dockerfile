FROM python:3.11-slim

WORKDIR /app

COPY gateway/sap_qm_mock/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/sap_qm_mock/ /app/sap_qm_mock/

ENV SAP_PORT=8090
ENV MQTT_HOST=localhost
ENV MQTT_PORT=1883

CMD ["python", "-m", "sap_qm_mock.app"]
