from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
import asyncio
from datetime import datetime, timedelta
import os

from .api import alerts, nodes, telemetry, ws, history
from .config import get_database_url
from . import db

app = FastAPI(title="O&L RF Mesh Backend")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(telemetry.router, prefix="/telemetry", tags=["telemetry"])
app.include_router(nodes.router, prefix="/nodes", tags=["nodes"])
app.include_router(alerts.router, prefix="/alerts", tags=["alerts"])
app.include_router(ws.router, tags=["ws"])
app.include_router(history.router, prefix="/history", tags=["history"])

app.state.database_url = get_database_url()
db.init_db()

RETENTION_DAYS = int(os.getenv("RETENTION_DAYS", "7"))
ROLLUP_INTERVAL_SECONDS = int(os.getenv("ROLLUP_INTERVAL_SECONDS", str(3600)))


async def maintenance_loop():
    while True:
        now = datetime.utcnow()
        cutoff = now - timedelta(days=RETENTION_DAYS)
        pruned_rf = db.prune_rf_events(cutoff)
        pruned_gps = db.prune_gps_logs(cutoff)
        db.compute_rollups(now)
        # Sleep until next interval
        await asyncio.sleep(ROLLUP_INTERVAL_SECONDS)


@app.on_event("startup")
async def start_background_jobs():
    asyncio.create_task(maintenance_loop())


@app.get("/health")
def health():
    return {"status": "ok"}
