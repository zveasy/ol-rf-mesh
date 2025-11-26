# O&L RF Mesh Node

Multi-node RF sensing + energy harvesting mesh system with:

- Edge firmware (Pico/ESP32)
- RF control (ADF4351)
- Telemetry backend (FastAPI + Postgres/Timescale)
- AI anomaly detection (TinyML)
- Web dashboard (React)

This repo is organized as a monorepo:

- `firmware/` – embedded C/C++
- `backend/` – data ingest + config APIs
- `ai/` – signal processing + training
- `dashboard/` – UI for nodes and RF activity
- `infra/` – local dev environment (docker-compose, etc.)
- `docs/` – diagrams, specs, design notes

## Quick start

- Backend (FastAPI): `cd backend && source .venv/bin/activate && uvicorn app.main:app --reload`, then verify with `curl http://127.0.0.1:8000/health` → expect 200 and `{"status":"ok"}`.
- AI scripts: `cd ai && source .venv/bin/activate && python scripts/generate_synthetic_data.py && python scripts/train_baseline_model.py`; check `data/raw/` for `.npy` outputs and `models/rf_baseline.joblib` for the trained model. If editable install fails, simply `pip install numpy scikit-learn joblib` in that venv.
