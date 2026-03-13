FROM python:3.11-slim

WORKDIR /app

COPY gateway/cloud_connector/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/cloud_connector/ /app/cloud_connector/

ENV MQTT_HOST=localhost
ENV MQTT_PORT=1883
ENV AWS_IOT_ENDPOINT=""
ENV AWS_IOT_PORT=8883
ENV AWS_CERT_DIR=/certs
ENV DEVICE_ID=taktflow-pi-001

CMD ["python", "-m", "cloud_connector.main"]
