from fastapi import APIRouter, Query

from ..schemas import PaginatedGps, PaginatedRfEvents, Rollup
from .. import db

router = APIRouter()


@router.get("/rf-events", response_model=PaginatedRfEvents)
async def list_rf_events(limit: int = Query(50, ge=1, le=500), offset: int = Query(0, ge=0)) -> PaginatedRfEvents:
    events, total = db.list_rf_events(limit=limit, offset=offset)
    return PaginatedRfEvents(items=events, total=total, limit=limit, offset=offset)


@router.get("/gps", response_model=PaginatedGps)
async def list_gps_logs(limit: int = Query(50, ge=1, le=500), offset: int = Query(0, ge=0)) -> PaginatedGps:
    samples, total = db.list_gps_logs(limit=limit, offset=offset)
    return PaginatedGps(items=samples, total=total, limit=limit, offset=offset)


@router.get("/rollups", response_model=list[Rollup])
async def get_rollups() -> list[Rollup]:
    rows = db.list_rollups()
    return [
        Rollup(
            node_id=r.node_id,
            last_updated=r.last_updated,
            rf_events_24h=r.rf_events_24h,
            avg_anomaly_24h=r.avg_anomaly_24h,
            gps_jam_events_24h=r.gps_jam_events_24h,
        )
        for r in rows
    ]
