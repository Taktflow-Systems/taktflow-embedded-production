FROM python:3.11-slim

WORKDIR /app

COPY gateway/fault_inject/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/fault_inject/ /app/fault_inject/

ENV CAN_CHANNEL=vcan0
ENV FAULT_PORT=8091

CMD ["python", "-m", "fault_inject.app"]
