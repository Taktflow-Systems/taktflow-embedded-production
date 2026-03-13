"""FastAPI main application for the SAP QM Mock API.

Simulates S/4HANA OData quality notification endpoints for the
Taktflow embedded gateway demo.

Usage:
    python -m sap_qm_mock.app
"""

from __future__ import annotations

import logging
import os
from contextlib import asynccontextmanager
from typing import AsyncIterator

import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .database import close_db, init_db
from .dtc_connector import DTCConnector
from .odata_router import router as odata_router

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
)
logger = logging.getLogger("sap_qm_mock.app")

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

SAP_PORT = int(os.environ.get("SAP_PORT", "8090"))

# ---------------------------------------------------------------------------
# DTC connector singleton
# ---------------------------------------------------------------------------

dtc_connector = DTCConnector()

# ---------------------------------------------------------------------------
# Application lifespan
# ---------------------------------------------------------------------------


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    """Startup/shutdown lifecycle manager."""
    # Startup
    logger.info("Initialising SAP QM Mock API...")
    await init_db()
    logger.info("Database ready")

    await dtc_connector.start()
    logger.info("DTC connector started")

    yield

    # Shutdown
    logger.info("Shutting down SAP QM Mock API...")
    await dtc_connector.stop()
    await close_db()
    logger.info("Shutdown complete")


# ---------------------------------------------------------------------------
# FastAPI application
# ---------------------------------------------------------------------------

app = FastAPI(
    title="SAP QM Mock API",
    description=(
        "Simulates S/4HANA OData V2 quality notification endpoints "
        "(API_QUALITYNOTIFICATION) for the Taktflow embedded gateway."
    ),
    version="0.1.0",
    docs_url="/api/sap/docs",
    redoc_url="/api/sap/redoc",
    openapi_url="/api/sap/openapi.json",
    lifespan=lifespan,
)

# CORS — allow all origins for demo purposes
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Mount the OData-style router
app.include_router(odata_router)


# ---------------------------------------------------------------------------
# Health check
# ---------------------------------------------------------------------------


@app.get("/health", tags=["System"])
async def health_check() -> dict[str, str]:
    """Simple health-check endpoint."""
    return {"status": "ok", "service": "sap-qm-mock"}


# ---------------------------------------------------------------------------
# Entrypoint
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    uvicorn.run(
        "sap_qm_mock.app:app",
        host="0.0.0.0",
        port=SAP_PORT,
        reload=False,
        log_level="info",
    )
