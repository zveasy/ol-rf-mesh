from datetime import datetime, UTC
from typing import List

from fastapi import APIRouter

from .. import state, db
from ..schemas import SpectrumSnapshot, SpectrumPoint
from ..auth import require_token
from fastapi import Depends

router = APIRouter()


@router.get("/latest", response_model=List[SpectrumSnapshot], dependencies=[Depends(require_token)])
async def latest_spectrum() -> List[SpectrumSnapshot]:
    """Return the most recent spectrum snapshots per band.

    Falls back to synthesizing a baseline if nothing has been ingested.
    """
    db_latest = db.list_latest_spectrum()
    if db_latest:
        return db_latest
    if state.SPECTRUM_BY_BAND:
        return list(state.SPECTRUM_BY_BAND.values())

    # Fallback: synthesize a simple spectrum so dashboards have something to show.
    now = datetime.now(UTC)
    points = [SpectrumPoint(freq_hz=915_000_000 + i * 10_000, power_dbm=-70 + (i % 10)) for i in range(32)]
    return [
        SpectrumSnapshot(
            band_id="mock",
            captured_at=now,
            points=points,
        )
    ]


@router.get("/history", response_model=List[SpectrumSnapshot], dependencies=[Depends(require_token)])
async def spectrum_history(band_id: str | None = None, limit: int = 50, offset: int = 0, since: datetime | None = None) -> List[SpectrumSnapshot]:
    return db.list_spectrum_history(band_id=band_id, since=since, limit=limit, offset=offset)
