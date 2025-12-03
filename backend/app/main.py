import time
import logging
import uuid
from fastapi import FastAPI, Request, Response
from fastapi.middleware.cors import CORSMiddleware
import asyncio
from datetime import datetime, timedelta, UTC
import os
from contextlib import asynccontextmanager, suppress

from .api import alerts, nodes, telemetry, ws, history, mock, spectrum
from .config import get_database_url
from . import db
from . import state
from .auth import API_TOKEN
from .logging_config import configure_logging
from .metrics import (
    REQUESTS_TOTAL,
    REQUEST_LATENCY_MS,
    NODES_KNOWN,
    RF_EVENTS_CACHED,
    GPS_LOGS_CACHED,
    ALERTS_CACHED,
    REQUEST_ERRORS_TOTAL,
    MOCK_QUEUE_DEPTH,
)
from prometheus_client import CONTENT_TYPE_LATEST, generate_latest

RETENTION_DAYS = int(os.getenv("RETENTION_DAYS", "7"))
ROLLUP_INTERVAL_SECONDS = int(os.getenv("ROLLUP_INTERVAL_SECONDS", str(3600)))


@asynccontextmanager
async def lifespan(app: FastAPI):
    task = asyncio.create_task(maintenance_loop())
    try:
        yield
    finally:
        task.cancel()
        with suppress(asyncio.CancelledError):
            await task


async def maintenance_loop():
    while True:
        now = datetime.now(datetime.UTC)
        cutoff = now - timedelta(days=RETENTION_DAYS)
        pruned_rf = db.prune_rf_events(cutoff)
        pruned_gps = db.prune_gps_logs(cutoff)
        db.compute_rollups(now)
        # Sleep until next interval
        await asyncio.sleep(ROLLUP_INTERVAL_SECONDS)


configure_logging()
logger = logging.getLogger("ol_rf_mesh")

app = FastAPI(title="O&L RF Mesh Backend", lifespan=lifespan)

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
app.include_router(mock.router, prefix="/mock", tags=["mock"])
app.include_router(spectrum.router, prefix="/spectrum", tags=["spectrum"])

app.state.database_url = get_database_url()
db.init_db()


@app.middleware("http")
async def add_request_id(request: Request, call_next):
    req_id = request.headers.get("x-request-id") or uuid.uuid4().hex
    request.state.request_id = req_id
    client_cn = request.headers.get("x-client-cn", "")
    auth_label = "token" if API_TOKEN else "none"
    if client_cn:
        auth_label = "mtls"
    start = time.perf_counter()
    response = await call_next(request)
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    response.headers["X-Request-ID"] = req_id

    path_label = request.url.path
    REQUESTS_TOTAL.labels(method=request.method, path=path_label, status=response.status_code, auth=auth_label).inc()
    REQUEST_LATENCY_MS.labels(method=request.method, path=path_label).observe(elapsed_ms)
    if response.status_code >= 400:
        REQUEST_ERRORS_TOTAL.labels(method=request.method, path=path_label, status=response.status_code).inc()

    content_length = request.headers.get("content-length")
    try:
        body_size = int(content_length) if content_length is not None else 0
    except ValueError:
        body_size = 0

    logger.info(
        "http_request",
        extra={
            "path": request.url.path,
            "method": request.method,
            "status": response.status_code,
            "request_id": req_id,
            "latency_ms": round(elapsed_ms, 2),
            "client_cn": client_cn,
            "body_size": body_size,
            "auth": auth_label,
        },
    )
    return response


@app.get("/health")
def health():
    return {"status": "ok"}


@app.get("/metrics")
def metrics():
    """Prometheus text metrics endpoint."""

    NODES_KNOWN.set(len(state.NODE_STATUS))
    RF_EVENTS_CACHED.set(len(state.RF_EVENTS))
    GPS_LOGS_CACHED.set(len(state.GPS_LOGS))
    ALERTS_CACHED.set(len(state.ALERTS))
    MOCK_QUEUE_DEPTH.set(len(state.RF_EVENTS) + len(state.ALERTS))
    payload = generate_latest()
    return Response(content=payload, media_type=CONTENT_TYPE_LATEST)
