# O&L RF Mesh Node

The O&L RF Mesh Node is a low-power, multi-node RF sensing system with edge AI and a cloud/backend dashboard.

Each node:
- Monitors RF activity and environment (RF power, battery, temperature)
- Runs lightweight anomaly detection locally
- Sends telemetry over a mesh/network back to a central backend
- Is designed to eventually integrate with energy harvesting and defense / perimeter sensing use cases.

This repo is a **monorepo** that contains:

- `firmware/` – Embedded C/C++ code for the RF node (Pico/ESP32 + sensors + mesh + TinyML)
- `backend/` – FastAPI service for telemetry ingest, node status, and configuration APIs
- `ai/` – Signal processing + model training code (FFT, feature extraction, anomaly detection, TFLite export)
- `dashboard/` – Web UI (React) for visualizing nodes, RF activity, and alerts
- `infra/` – Local dev & deployment config (docker-compose, DB, services)
- `docs/` – Architecture diagrams, specs, and design notes

---

## Vision

The long-term vision is a family of O&L RF nodes that can:

- Form a **mesh sensing network** for RF activity
- Use **energy harvesting** (rectennas, solar, etc.) to stay low-maintenance
- Run **edge AI** to flag anomalies and threats
- Integrate into both:
  - Commercial energy / monitoring systems
  - Defense / perimeter sensing systems (NASAMS-like architectures)

This repo focuses on the **core software + AI stack** needed to make that possible.

---

## Architecture Overview

High-level data flow:

1. **Node firmware**
   - Samples RF detector + sensors
   - Runs FFT / feature extraction
   - Optionally runs edge ML model (TinyML)
   - Packages telemetry + anomaly score into messages
   - Sends messages over Wi-Fi / mesh to backend

2. **Backend (FastAPI)**
   - Exposes `/telemetry` endpoint to ingest node data
   - Stores data in a time-series database (Postgres/Timescale or Postgres + retention)
   - Exposes `/nodes` and config APIs for the dashboard + future control flows

3. **AI / DSP**
   - Ingests raw and processed RF data
   - Performs FFT + feature engineering
   - Trains and evaluates anomaly detection models
   - Exports compact models for edge deployment (TFLite Micro or similar)

4. **Dashboard**
   - Displays list of nodes and their health
   - Visualizes RF activity and anomalies over time
   - Provides basic controls for configuration and monitoring

---

## Repository Structure

```text
ol-rf-mesh/
  README.md
  LICENSE
  .gitignore

  firmware/
    include/
    src/
    tests/            # planned

  backend/
    app/
    tests/             # planned

  ai/
    scripts/
    notebooks/         # planned
    data/
    models/

  dashboard/
    src/
    public/            # or Vite index.html

  infra/
    docker-compose.yml

  docs/
    overview.md
    checklist.md
```

---

## Getting Started (Conceptual)

For concrete commands, see the root `README.md` quick start. Conceptually:

- **Firmware**: builds as a native C++ test binary today, later targets Pico/ESP32
- **Backend**: FastAPI app served by `uvicorn` and/or Docker (via `infra/docker-compose.yml`)
- **AI**: Python scripts for synthetic data generation and baseline anomaly model training
- **Dashboard**: React-based UI (Vite) to talk to the backend
- **Infra**: `docker compose up --build` to spin up DB, backend, and AI worker
