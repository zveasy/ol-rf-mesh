FastAPI backend for O&L RF Mesh Node telemetry and configuration APIs.

## Quick start

```bash
cd backend
python -m venv .venv && source .venv/bin/activate
pip install -e .
uvicorn app.main:app --reload
```

Set `DATABASE_URL` (default: `sqlite:///backend.db`). For Postgres/Timescale:
`export DATABASE_URL=postgresql://user:pass@localhost:5432/ol_rf_mesh`

## API surface (MVP)

- `GET /health` – simple healthcheck
- `POST /nodes/` – register a node (in-memory)
- `GET /nodes/` – list latest status snapshots
- `POST /telemetry/` – legacy single telemetry sample
- `POST /telemetry/rf-events` – batch RF events
- `POST /telemetry/gps` – batch GNSS quality samples
- `POST /alerts/` – create an alert; `GET /alerts/` – list recent alerts
- `GET /ws/rf-stream` – WebSocket endpoint for RF/telemetry/alert pushes
- `GET /history/rf-events` – paged RF event history (limit/offset)
- `GET /history/gps` – paged GNSS history (limit/offset)
- `GET /history/rollups` – 24h rollups per node (events, avg anomaly, jam counts)

## Persistence

- SQLAlchemy over `DATABASE_URL` (defaults to SQLite `backend.db`), ready for Postgres/Timescale. Tables: nodes, rf_events, gps_logs, alerts (`app/models.py`).
- Alerts are generated automatically on RF anomaly scores ≥0.8 and GNSS interference flags; stored + broadcast over WebSocket.
- Background maintenance (`app.main`):
  - Prunes RF/GPS older than `RETENTION_DAYS` (default 7).
  - Computes 24h rollups every `ROLLUP_INTERVAL_SECONDS` (default 3600).
