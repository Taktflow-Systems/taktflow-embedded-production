FROM python:3.11-slim

WORKDIR /app

COPY gateway/ml_inference/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/ml_inference/ /app/ml_inference/

ENV MQTT_HOST=localhost
ENV MQTT_PORT=1883

CMD ["python", "-m", "ml_inference.detector"]
