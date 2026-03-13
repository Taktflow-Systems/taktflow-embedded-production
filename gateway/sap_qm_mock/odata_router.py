"""FastAPI router implementing OData-style SAP QM notification endpoints.

Routes mirror the S/4HANA OData V2 API_QUALITYNOTIFICATION service,
wrapped in standard OData ``{"d": ...}`` response envelopes.
"""

from __future__ import annotations

import logging
from typing import Any, Optional

from fastapi import APIRouter, HTTPException, Query, Response, status

from .database import (
    create_notification,
    get_notification,
    list_notifications,
    update_notification,
)
from .defect_catalog import ECU_PLANTS, map_dtc_to_defect
from .models import (
    NotificationPatch,
    NotificationStatus,
    QualityNotificationCreate,
    VALID_TRANSITIONS,
)

logger = logging.getLogger("sap_qm_mock.odata")

ODATA_BASE = "/sap/opu/odata/sap/API_QUALITYNOTIFICATION"

router = APIRouter(prefix=ODATA_BASE, tags=["QualityNotification"])


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _odata_single(entity: dict[str, Any]) -> dict[str, Any]:
    """Wrap a single entity in OData ``{"d": {...}}`` format."""
    return {"d": entity}


def _odata_collection(results: list[dict[str, Any]]) -> dict[str, Any]:
    """Wrap a collection in OData ``{"d": {"results": [...]}}`` format."""
    return {"d": {"results": results}}


def _enrich_from_dtc(data: dict[str, Any]) -> dict[str, Any]:
    """If a dtc_code is provided, auto-fill defect info from the catalog."""
    dtc_code = data.get("dtc_code")
    if dtc_code:
        defect = map_dtc_to_defect(dtc_code)
        if defect:
            if not data.get("defect_code"):
                data["defect_code"] = defect["code"]
            if not data.get("priority"):
                data["priority"] = defect["priority"]
            # Auto-add an item if none provided
            if not data.get("items"):
                data["items"] = [
                    {
                        "item_number": "0001",
                        "defect_code": defect["code"],
                        "defect_text": defect["text"],
                        "cause_code": dtc_code,
                        "cause_text": f"DTC {dtc_code} from ECU",
                    }
                ]

    # Resolve plant from ecu_source if not explicitly set
    ecu_source = data.get("ecu_source")
    if ecu_source and not data.get("plant"):
        data["plant"] = ECU_PLANTS.get(ecu_source)

    return data


# ---------------------------------------------------------------------------
# GET  — list notifications
# ---------------------------------------------------------------------------

@router.get("/QualityNotification")
async def list_quality_notifications(
    filter: Optional[str] = Query(None, alias="$filter"),
    top: int = Query(50, alias="$top", ge=1, le=500),
    skip: int = Query(0, alias="$skip", ge=0),
    orderby: Optional[str] = Query(None, alias="$orderby"),
) -> dict[str, Any]:
    """List quality notifications with OData-style query options.

    Supports ``$filter`` (e.g. ``status eq 'OSNO'``), ``$top``, ``$skip``,
    and ``$orderby`` (e.g. ``created_at desc``).
    """
    results = await list_notifications(
        filter_expr=filter,
        top=top,
        skip=skip,
        orderby=orderby,
    )
    logger.info("Listed %d notifications (filter=%s)", len(results), filter)
    return _odata_collection(results)


# ---------------------------------------------------------------------------
# GET  — single notification by ID
# ---------------------------------------------------------------------------

@router.get("/QualityNotification('{notification_id}')")
async def get_quality_notification(
    notification_id: str,
) -> dict[str, Any]:
    """Retrieve a single quality notification by its formatted ID (e.g. ``QN-00001``)."""
    record = await get_notification(notification_id)
    if record is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Notification {notification_id!r} not found",
        )
    logger.info("Retrieved notification %s", notification_id)
    return _odata_single(record)


# ---------------------------------------------------------------------------
# POST — create notification
# ---------------------------------------------------------------------------

@router.post(
    "/QualityNotification",
    status_code=status.HTTP_201_CREATED,
)
async def create_quality_notification(
    body: QualityNotificationCreate,
) -> dict[str, Any]:
    """Create a new quality notification.

    If ``dtc_code`` is provided, defect details are auto-populated from the
    DTC-to-defect catalog.  If ``ecu_source`` is provided, the plant is
    resolved from the ECU-to-plant mapping.
    """
    data = body.model_dump()
    data = _enrich_from_dtc(data)

    record = await create_notification(data)
    logger.info(
        "Created notification %s (dtc=%s, plant=%s)",
        record["notification_id"],
        record.get("dtc_code"),
        record.get("plant"),
    )
    return _odata_single(record)


# ---------------------------------------------------------------------------
# PATCH — update notification (status transition, priority)
# ---------------------------------------------------------------------------

@router.patch("/QualityNotification('{notification_id}')")
async def patch_quality_notification(
    notification_id: str,
    body: NotificationPatch,
) -> dict[str, Any]:
    """Update a quality notification's status or priority.

    Status transitions must follow the valid lifecycle:
    ``OSNO -> NOPR -> OSTS -> NOCO``.
    """
    # Validate status transition if a new status is requested
    if body.status is not None:
        current = await get_notification(notification_id)
        if current is None:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Notification {notification_id!r} not found",
            )
        current_status = NotificationStatus(current["status"])
        allowed = VALID_TRANSITIONS.get(current_status, [])
        if body.status not in allowed:
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail=(
                    f"Invalid status transition: {current_status.value} -> "
                    f"{body.status.value}. Allowed: "
                    f"{[s.value for s in allowed]}"
                ),
            )

    patch_data = body.model_dump(exclude_none=True)
    if not patch_data:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="No fields to update",
        )

    # Convert enum to string value for DB storage
    if "status" in patch_data and patch_data["status"] is not None:
        patch_data["status"] = patch_data["status"].value

    record = await update_notification(notification_id, patch_data)
    if record is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Notification {notification_id!r} not found",
        )

    logger.info(
        "Patched notification %s -> %s",
        notification_id,
        patch_data,
    )
    return _odata_single(record)
