#!/usr/bin/env python3
"""
Train an Isolation Forest model on synthetic normal CAN telemetry data.

Features (5):
  - motor_current_mean   (500–3000 mA, normal operation)
  - motor_current_std    (50–200 mA)
  - motor_temp           (25–70 °C, below overtemp threshold)
  - rpm                  (0–3500, normal operating range)
  - battery_voltage      (11500–13000 mV)

Outputs:
  anomaly_model.pkl  — fitted IsolationForest
  scaler.pkl         — fitted StandardScaler
"""

from __future__ import annotations

import os
import logging
from pathlib import Path

import numpy as np
import joblib
from sklearn.ensemble import IsolationForest
from sklearn.preprocessing import StandardScaler

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
FEATURE_NAMES = [
    "motor_current_mean",
    "motor_current_std",
    "motor_temp",
    "rpm",
    "battery_voltage",
]

N_SAMPLES = 5000
RANDOM_SEED = 42

MODEL_DIR = Path(__file__).resolve().parent
MODEL_PATH = MODEL_DIR / "anomaly_model.pkl"
SCALER_PATH = MODEL_DIR / "scaler.pkl"


# ---------------------------------------------------------------------------
# Synthetic data generation
# ---------------------------------------------------------------------------
def generate_normal_data(n_samples: int = N_SAMPLES, seed: int = RANDOM_SEED) -> np.ndarray:
    """Generate synthetic *normal* CAN telemetry features.

    Includes four operating regimes:
      - Idle (~25%): motor off / standby (0-50 mA, 0-100 RPM, cool temp)
      - Cooling (~10%): motor just stopped, temp still elevated (0-50 mA, 30-70°C)
      - Transition (~10%): ramp-up / ramp-down (50-500 mA)
      - Driving (~55%): normal operation (500-3000 mA, 0-3500 RPM)
    """
    rng = np.random.default_rng(seed)

    n_idle = int(n_samples * 0.25)
    n_cooling = int(n_samples * 0.10)
    n_transition = int(n_samples * 0.10)
    n_driving = n_samples - n_idle - n_cooling - n_transition

    # --- Idle state: motor off / standby, cool temperature ---
    idle_current_mean = rng.uniform(0, 50, size=n_idle)
    idle_current_std = rng.uniform(0, 10, size=n_idle)
    idle_temp = rng.uniform(20, 30, size=n_idle)
    idle_rpm = rng.uniform(0, 100, size=n_idle)
    idle_voltage = rng.uniform(12000, 13000, size=n_idle)

    # --- Cooling state: motor stopped but temp still elevated from driving ---
    cool_current_mean = rng.uniform(0, 50, size=n_cooling)
    cool_current_std = rng.uniform(0, 15, size=n_cooling)
    cool_temp = rng.uniform(30, 70, size=n_cooling)
    cool_rpm = rng.uniform(0, 100, size=n_cooling)
    cool_voltage = rng.uniform(12000, 13000, size=n_cooling)

    # --- Transition state: ramp-up / ramp-down ---
    trans_current_mean = rng.uniform(50, 500, size=n_transition)
    trans_current_std = rng.uniform(5, 80, size=n_transition)
    trans_temp = rng.uniform(25, 45, size=n_transition)
    trans_rpm = rng.uniform(0, 1500, size=n_transition)
    trans_voltage = rng.uniform(11500, 13000, size=n_transition)

    # --- Driving state: normal operation ---
    drive_current_mean = rng.uniform(500, 3000, size=n_driving)
    drive_current_std = rng.uniform(50, 200, size=n_driving)
    drive_temp = rng.uniform(25, 70, size=n_driving)
    drive_rpm = rng.uniform(0, 3500, size=n_driving)
    drive_voltage = rng.uniform(11500, 13000, size=n_driving)

    data = np.column_stack([
        np.concatenate([idle_current_mean, cool_current_mean, trans_current_mean, drive_current_mean]),
        np.concatenate([idle_current_std, cool_current_std, trans_current_std, drive_current_std]),
        np.concatenate([idle_temp, cool_temp, trans_temp, drive_temp]),
        np.concatenate([idle_rpm, cool_rpm, trans_rpm, drive_rpm]),
        np.concatenate([idle_voltage, cool_voltage, trans_voltage, drive_voltage]),
    ])
    # Shuffle so the model doesn't see regime-ordered data
    rng.shuffle(data)
    return data


# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------
def train_model(
    n_samples: int = N_SAMPLES,
    contamination: float = 0.05,
    n_estimators: int = 100,
    random_state: int = RANDOM_SEED,
) -> tuple[IsolationForest, StandardScaler]:
    """Train IsolationForest on synthetic normal data and return (model, scaler)."""

    logger.info("Generating %d synthetic normal samples …", n_samples)
    X = generate_normal_data(n_samples=n_samples, seed=random_state)

    logger.info("Fitting StandardScaler …")
    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X)

    logger.info(
        "Training IsolationForest (n_estimators=%d, contamination=%.2f) …",
        n_estimators,
        contamination,
    )
    model = IsolationForest(
        n_estimators=n_estimators,
        contamination=contamination,
        random_state=random_state,
    )
    model.fit(X_scaled)

    return model, scaler


def save_model(
    model: IsolationForest,
    scaler: StandardScaler,
    model_path: Path = MODEL_PATH,
    scaler_path: Path = SCALER_PATH,
) -> None:
    """Persist model and scaler to disk via joblib."""
    os.makedirs(model_path.parent, exist_ok=True)
    joblib.dump(model, model_path)
    joblib.dump(scaler, scaler_path)
    logger.info("Saved model  → %s", model_path)
    logger.info("Saved scaler → %s", scaler_path)


# ---------------------------------------------------------------------------
# CLI entry-point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
    )
    model, scaler = train_model()
    save_model(model, scaler)
    logger.info("Training complete.")
