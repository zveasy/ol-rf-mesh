# Backend Deployment Checklist (stub)

- **Prereqs**: Postgres/Timescale reachable; `DATABASE_URL` + `API_TOKEN` available; TLS certs in place for gateway ingress if exposed.
- **Build/test**: `cd backend && python -m venv .venv && source .venv/bin/activate && pip install -r requirements.txt && python -m pytest -q`.
- **DB prep**: Create database + user; run `app.db.init_db()` via app startup (tables auto-create). Configure backups/retention.
- **Run**: `uvicorn app.main:app --host 0.0.0.0 --port 8000 --workers 2`. Confirm `GET /health`, `/metrics`, `/mock/nodes`.
- **Auth/ingest**: Set `API_TOKEN` and verify `Authorization: Bearer <token>` on `/telemetry/*`, `/history/*`, `/alerts`, `/spectrum/*`. Leave `/health`/`/metrics` open for probes.
- **Observability**: Forward logs to central sink; scrape `/metrics` or attach Prometheus/Otel later. Add uptime probe on `/health`.
- **Gateway config**: Point gateways/agents at backend URL + token; confirm WebSocket `/ws/rf-stream` receives alerts when ingesting a sample.
