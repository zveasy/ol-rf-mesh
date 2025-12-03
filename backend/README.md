FastAPI backend for O&L RF Mesh Node telemetry and configuration APIs.

## Quick start

```bash
cd backend
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
uvicorn app.main:app --reload

# Run tests / smoke checks
python -m pytest -q
```

Set `DATABASE_URL` (default: `sqlite:///backend.db`). For Postgres/Timescale:
`export DATABASE_URL=postgresql://user:pass@localhost:5432/ol_rf_mesh`

Auth stub (optional): set `API_TOKEN=secret` and include `Authorization: Bearer secret` on protected endpoints (telemetry ingest and history).
All write/read endpoints except `/health` and `/metrics` require the token when set.

## API surface (MVP)

- `GET /health` – simple healthcheck
- `POST /nodes/` – register a node (in-memory)
- `GET /nodes/` – list latest status snapshots
- `POST /telemetry/` – legacy single telemetry sample
- `POST /telemetry/rf-events` – batch RF events
- `POST /telemetry/gps` – batch GNSS quality samples
- (token-protected if `API_TOKEN` set)
- `POST /alerts/` – create an alert; `GET /alerts/` – list recent alerts
- `GET /ws/rf-stream` – WebSocket endpoint for RF/telemetry/alert pushes
- `GET /spectrum/latest` – latest per-band FFT/spectrum snapshot (derived from ingested RF features or mock fallback)
- `GET /spectrum/history` – time/paged spectrum snapshots (`band_id`, `since`, `limit`, `offset`)
- `GET /history/rf-events` – paged RF event history (limit/offset)
- `GET /history/gps` – paged GNSS history (limit/offset)
- `GET /history/rollups` – 24h rollups per node (events, avg anomaly, jam counts)
- `GET /metrics` – Prometheus text metrics (requests, latency, caches)
- `GET /mock/nodes`, `GET /mock/events` – static data for dashboard smoke tests

## Migrations

Alembic is configured for SQLite and Postgres-compatible schemas.

```bash
cd backend
alembic upgrade head
```

CI note: GitHub Actions runs migrations against SQLite and Postgres service before pytest.

## Load testing (Locust)

Quick smoke load:

```bash
cd backend
source .venv/bin/activate
pip install locust
locust -f load_tests/locustfile.py --headless -u 5 -r 2 --run-time 30s --host http://localhost:8000
```

Pytest load test (`tests/test_load_rf_events.py`) is skipped by default; set `RUN_LOAD_TEST=1` and ensure a local backend is running to exercise it. In CI, leave it off so the suite remains green without a live service.

## Persistence

- SQLAlchemy over `DATABASE_URL` (defaults to SQLite `backend.db`), ready for Postgres/Timescale. Tables: nodes, rf_events, gps_logs, alerts (`app/models.py`).
- Alerts are generated automatically on RF anomaly scores ≥0.8 and GNSS interference flags; stored + broadcast over WebSocket.
- Background maintenance (`app.main`):
  - Prunes RF/GPS older than `RETENTION_DAYS` (default 7).
  - Computes 24h rollups every `ROLLUP_INTERVAL_SECONDS` (default 3600).
