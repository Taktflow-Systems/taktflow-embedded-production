FROM python:3.11-slim

WORKDIR /app

COPY gateway/plant_sim_py/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY gateway/taktflow.dbc /app/taktflow.dbc
COPY gateway/plant_sim_py/ /app/plant_sim/

ENV DBC_PATH=/app/taktflow.dbc
ENV CAN_CHANNEL=vcan0

CMD ["python", "-m", "plant_sim.simulator"]
