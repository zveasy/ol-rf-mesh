from datetime import datetime
from typing import List

from fastapi import APIRouter, Depends

from ..schemas import Alert, AlertIn
from .ws import manager
from .. import db
from ..auth import require_token

router = APIRouter()


async def store_and_broadcast_alert(alert: AlertIn) -> Alert:
    alert_obj = db.insert_alert(alert)
    await manager.broadcast(
        {
            "type": "alert",
            "id": alert_obj.id,
            "severity": alert_obj.severity,
            "message": alert_obj.message,
            "node_ids": alert_obj.node_ids,
            "created_at": alert_obj.created_at.isoformat(),
        }
    )
    return alert_obj


@router.post("/", response_model=Alert, dependencies=[Depends(require_token)])
async def create_alert(alert: AlertIn) -> Alert:
    """Create an alert (from backend logic or an external source)."""
    return await store_and_broadcast_alert(alert)


@router.get("/", response_model=List[Alert], dependencies=[Depends(require_token)])
async def list_alerts() -> List[Alert]:
    """List recent alerts."""
    return db.recent_alerts()
