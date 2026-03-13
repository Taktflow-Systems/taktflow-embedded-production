"""SQLite database layer for SAP QM mock notifications (via aiosqlite)."""

from __future__ import annotations

import json
from datetime import datetime, timezone
from typing import Any, Optional

import aiosqlite

DB_PATH = "sap_qm_mock.db"

_db: Optional[aiosqlite.Connection] = None


async def _get_db() -> aiosqlite.Connection:
    """Return the shared database connection, opening it if needed."""
    global _db
    if _db is None:
        _db = await aiosqlite.connect(DB_PATH)
        _db.row_factory = aiosqlite.Row
    return _db


async def init_db() -> None:
    """Create tables if they do not exist."""
    db = await _get_db()
    await db.execute(
        """
        CREATE TABLE IF NOT EXISTS notifications (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            notification_type TEXT NOT NULL DEFAULT 'Q2',
            description     TEXT NOT NULL,
            material        TEXT,
            plant           TEXT,
            defect_code     TEXT,
            dtc_code        TEXT,
            ecu_source      INTEGER,
            priority        TEXT NOT NULL DEFAULT '2',
            status          TEXT NOT NULL DEFAULT 'OSNO',
            created_at      TEXT NOT NULL,
            updated_at      TEXT NOT NULL
        )
        """
    )
    await db.execute(
        """
        CREATE TABLE IF NOT EXISTS notification_items (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            notification_id INTEGER NOT NULL,
            item_number     TEXT NOT NULL,
            defect_code     TEXT NOT NULL,
            defect_text     TEXT NOT NULL,
            cause_code      TEXT,
            cause_text      TEXT,
            FOREIGN KEY (notification_id) REFERENCES notifications(id)
        )
        """
    )
    await db.commit()


def _format_id(raw_id: int) -> str:
    """Format a raw auto-increment ID as QN-NNNNN."""
    return f"QN-{raw_id:05d}"


def _parse_id(notification_id: str) -> int:
    """Parse 'QN-00001' back to integer 1."""
    return int(notification_id.replace("QN-", ""))


async def create_notification(data: dict[str, Any]) -> dict[str, Any]:
    """Insert a new notification and its items. Returns the full record."""
    db = await _get_db()
    now = datetime.now(timezone.utc).isoformat()

    cursor = await db.execute(
        """
        INSERT INTO notifications
            (notification_type, description, material, plant,
             defect_code, dtc_code, ecu_source, priority, status,
             created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'OSNO', ?, ?)
        """,
        (
            data.get("notification_type", "Q2"),
            data["description"],
            data.get("material"),
            data.get("plant"),
            data.get("defect_code"),
            data.get("dtc_code"),
            data.get("ecu_source"),
            data.get("priority", "2"),
            now,
            now,
        ),
    )
    row_id = cursor.lastrowid
    assert row_id is not None

    # Insert items
    items: list[dict[str, Any]] = data.get("items", [])
    for idx, item in enumerate(items, start=1):
        await db.execute(
            """
            INSERT INTO notification_items
                (notification_id, item_number, defect_code, defect_text,
                 cause_code, cause_text)
            VALUES (?, ?, ?, ?, ?, ?)
            """,
            (
                row_id,
                item.get("item_number", f"{idx:04d}"),
                item["defect_code"],
                item["defect_text"],
                item.get("cause_code"),
                item.get("cause_text"),
            ),
        )

    await db.commit()
    return await get_notification(_format_id(row_id))  # type: ignore[return-value]


async def get_notification(notification_id: str) -> dict[str, Any] | None:
    """Fetch a single notification by its formatted ID (e.g. 'QN-00001')."""
    db = await _get_db()
    raw_id = _parse_id(notification_id)

    cursor = await db.execute(
        "SELECT * FROM notifications WHERE id = ?", (raw_id,)
    )
    row = await cursor.fetchone()
    if row is None:
        return None

    record = dict(row)
    record["notification_id"] = _format_id(record.pop("id"))

    # Fetch items
    item_cursor = await db.execute(
        "SELECT * FROM notification_items WHERE notification_id = ?", (raw_id,)
    )
    item_rows = await item_cursor.fetchall()
    record["items"] = [
        {k: v for k, v in dict(r).items() if k not in ("id", "notification_id")}
        for r in item_rows
    ]

    return record


async def list_notifications(
    *,
    filter_expr: Optional[str] = None,
    top: int = 50,
    skip: int = 0,
    orderby: Optional[str] = None,
) -> list[dict[str, Any]]:
    """List notifications with OData-style filtering, paging, and ordering."""
    db = await _get_db()

    query = "SELECT * FROM notifications"
    params: list[Any] = []

    # Basic OData $filter support (status eq 'OSNO', plant eq 'CVC')
    if filter_expr:
        clauses: list[str] = []
        for part in filter_expr.split(" and "):
            part = part.strip()
            if " eq " in part:
                field, value = part.split(" eq ", 1)
                field = field.strip()
                value = value.strip().strip("'\"")
                clauses.append(f"{field} = ?")
                params.append(value)
        if clauses:
            query += " WHERE " + " AND ".join(clauses)

    # Ordering
    if orderby:
        col = orderby.replace(" desc", "").replace(" asc", "").strip()
        direction = "DESC" if "desc" in orderby else "ASC"
        query += f" ORDER BY {col} {direction}"
    else:
        query += " ORDER BY id DESC"

    query += " LIMIT ? OFFSET ?"
    params.extend([top, skip])

    cursor = await db.execute(query, params)
    rows = await cursor.fetchall()

    results: list[dict[str, Any]] = []
    for row in rows:
        record = dict(row)
        raw_id = record.pop("id")
        record["notification_id"] = _format_id(raw_id)

        # Fetch items for each notification
        item_cursor = await db.execute(
            "SELECT * FROM notification_items WHERE notification_id = ?",
            (raw_id,),
        )
        item_rows = await item_cursor.fetchall()
        record["items"] = [
            {k: v for k, v in dict(r).items() if k not in ("id", "notification_id")}
            for r in item_rows
        ]
        results.append(record)

    return results


async def update_notification(
    notification_id: str, patch: dict[str, Any]
) -> dict[str, Any] | None:
    """Update status and/or priority on an existing notification."""
    db = await _get_db()
    raw_id = _parse_id(notification_id)

    # Check existence
    cursor = await db.execute(
        "SELECT * FROM notifications WHERE id = ?", (raw_id,)
    )
    existing = await cursor.fetchone()
    if existing is None:
        return None

    now = datetime.now(timezone.utc).isoformat()
    sets: list[str] = ["updated_at = ?"]
    params: list[Any] = [now]

    if "status" in patch and patch["status"] is not None:
        sets.append("status = ?")
        params.append(patch["status"])
    if "priority" in patch and patch["priority"] is not None:
        sets.append("priority = ?")
        params.append(patch["priority"])

    params.append(raw_id)
    await db.execute(
        f"UPDATE notifications SET {', '.join(sets)} WHERE id = ?",
        params,
    )
    await db.commit()

    return await get_notification(notification_id)


async def close_db() -> None:
    """Close the database connection."""
    global _db
    if _db is not None:
        await _db.close()
        _db = None
