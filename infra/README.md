Infrastructure and local dev environment for O&L RF Mesh Node.

## Run backend + Postgres with Docker

```bash
cd infra
docker compose up --build
# backend reachable at http://localhost:8000/health
# dashboard reachable at http://localhost:3000
```

Services:
- `db`: Postgres 16, credentials `mesh/mesh`, DB `mesh`
- `backend`: builds from `../backend` and exposes port 8000
- `ai-worker`: builds from `../ai`, runs synthetic data + training, persists to named volumes
- `dashboard`: builds from `../dashboard` (Vite/React, then nginx), serves UI on port 3000
