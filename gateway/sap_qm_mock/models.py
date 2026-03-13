"""Pydantic models for SAP QM quality notifications."""

from __future__ import annotations

from datetime import datetime
from enum import Enum
from typing import Optional

from pydantic import BaseModel, Field


class NotificationStatus(str, Enum):
    """SAP QM notification status lifecycle.

    OSNO  ->  NOPR  ->  OSTS  ->  NOCO
    (Outstanding)  (In process)  (Outstanding task)  (Completed)
    """

    OSNO = "OSNO"  # Outstanding notification
    NOPR = "NOPR"  # Notification in process
    OSTS = "OSTS"  # Outstanding task
    NOCO = "NOCO"  # Notification completed


VALID_TRANSITIONS: dict[NotificationStatus, list[NotificationStatus]] = {
    NotificationStatus.OSNO: [NotificationStatus.NOPR],
    NotificationStatus.NOPR: [NotificationStatus.OSTS],
    NotificationStatus.OSTS: [NotificationStatus.NOCO],
    NotificationStatus.NOCO: [],
}


class NotificationItem(BaseModel):
    """Single defect item within a quality notification."""

    item_number: str = Field(..., description="Item sequence number, e.g. '0001'")
    defect_code: str = Field(..., description="SAP defect code, e.g. 'D-MOT-001'")
    defect_text: str = Field(..., description="Human-readable defect description")
    cause_code: Optional[str] = Field(None, description="Root-cause code")
    cause_text: Optional[str] = Field(None, description="Root-cause description")


class QualityNotificationCreate(BaseModel):
    """POST body for creating a new quality notification."""

    notification_type: str = Field(
        default="Q2",
        description="SAP notification type (Q1=Customer, Q2=Vendor, Q3=Internal)",
    )
    description: str = Field(
        ..., max_length=240, description="Short description of the quality issue"
    )
    material: Optional[str] = Field(
        None, description="Material number (SAP MATNR)"
    )
    plant: Optional[str] = Field(
        None, description="Plant code, e.g. 'CVC', 'FZC'"
    )
    defect_code: Optional[str] = Field(
        None, description="SAP defect catalog code"
    )
    dtc_code: Optional[str] = Field(
        None, description="Diagnostic Trouble Code from ECU, e.g. '0xE301'"
    )
    ecu_source: Optional[int] = Field(
        None, ge=1, le=7, description="ECU node ID (1-7)"
    )
    priority: str = Field(
        default="2", description="Priority: 1=Very High, 2=High, 3=Medium, 4=Low"
    )
    items: list[NotificationItem] = Field(
        default_factory=list, description="Defect items attached to this notification"
    )


class QualityNotification(BaseModel):
    """Full quality notification entity returned by the API."""

    notification_id: str = Field(
        ..., description="Formatted ID, e.g. 'QN-00001'"
    )
    notification_type: str = Field(
        ..., description="SAP notification type"
    )
    description: str = Field(
        ..., description="Short description"
    )
    material: Optional[str] = None
    plant: Optional[str] = None
    defect_code: Optional[str] = None
    dtc_code: Optional[str] = None
    ecu_source: Optional[int] = None
    priority: str = Field(default="2")
    status: NotificationStatus = Field(
        default=NotificationStatus.OSNO,
        description="Current lifecycle status",
    )
    created_at: datetime = Field(
        ..., description="ISO-8601 creation timestamp"
    )
    updated_at: datetime = Field(
        ..., description="ISO-8601 last-update timestamp"
    )
    items: list[NotificationItem] = Field(
        default_factory=list, description="Attached defect items"
    )


class NotificationPatch(BaseModel):
    """PATCH body for updating an existing notification."""

    status: Optional[NotificationStatus] = Field(
        None, description="New lifecycle status (must follow valid transitions)"
    )
    priority: Optional[str] = Field(
        None, description="Updated priority: 1=Very High, 2=High, 3=Medium, 4=Low"
    )
