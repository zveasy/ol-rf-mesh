from fastapi import APIRouter
from ..schemas import TelemetryIn

router = APIRouter()

TELEMETRY_BUFFER: list[TelemetryIn] = []


@router.post("/")
async def ingest_telemetry(data: TelemetryIn):
    TELEMETRY_BUFFER.append(data)
    return {"status": "ok"}
