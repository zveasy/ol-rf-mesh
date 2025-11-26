# O&L RF Mesh Production-Ready Checklist

Use this as a living document in GitHub Projects / Notion. Each item is a concrete milestone toward a demo/production-ready system.

---

## 1. Firmware / Embedded

- [ ] Project builds cleanly from a single command (`cmake --build` or `platformio run`)
- [ ] Clear hardware abstraction layer (HAL) for SPI, ADC, timers, mesh
- [ ] ADF4351 driver:
  - [ ] Can set frequency reliably
  - [ ] Can set output power / profiles
  - [ ] Has error handling and retries
- [ ] Sensor stack:
  - [ ] RF power detector driver stable
  - [ ] Battery voltage/current readings validated vs multimeter
  - [ ] Temperature sensor validated
- [ ] Node config:
  - [ ] Node ID stored in non-volatile memory
  - [ ] Configurable report interval
  - [ ] Configurable anomaly thresholds
- [ ] Telemetry encoding:
  - [ ] Well-defined, versioned packet format
  - [ ] Tested for endianness and field alignment
- [ ] Mesh / networking:
  - [ ] Nodes automatically join and rejoin the network
  - [ ] Graceful handling of dropped packets and timeouts
  - [ ] Ability to route to a gateway node or cloud directly
- [ ] Edge AI integration:
  - [ ] Tiny model or rule-based anomaly detection integrated
  - [ ] Inference runs within timing + memory constraints
  - [ ] Fails open (if model fails, node still sends raw data)
- [ ] Power behavior:
  - [ ] Safe boot on low battery
  - [ ] No brown-out boot loops
  - [ ] Measured power consumption within target
- [ ] Logging & debug:
  - [ ] UART/log mode for development
  - [ ] Build flag to disable logs for production
- [ ] Firmware update path:
  - [ ] Manual update documented
  - [ ] (Optional) OTA workflow defined

---

## 2. Backend / API

- [ ] FastAPI service runs behind a single entry command
- [ ] Config handled via `.env` (no secrets hardcoded)
- [ ] Health endpoint (`/health`) for monitoring
- [ ] Telemetry ingest:
  - [ ] Validates payloads (schema: node_id, timestamp, rf_power, etc.)
  - [ ] Handles malformed input gracefully
  - [ ] Idempotent enough not to break on duplicates
- [ ] Database:
  - [ ] Postgres/Timescale schema designed for telemetry
  - [ ] Migration files exist (Alembic or similar)
  - [ ] Indexes tuned for queries used by the dashboard
- [ ] Node status:
  - [ ] API to retrieve a list of nodes and last seen data
  - [ ] Node “health” logic (online/offline/stale)
- [ ] Config API:
  - [ ] Endpoint to get/set node config (report interval, thresholds, etc.)
  - [ ] Access control defined (even if basic)
- [ ] Security baseline:
  - [ ] HTTPS/TLS story documented (even if only for deployed version)
  - [ ] Input sanitization & basic auth story in place
- [ ] Observability:
  - [ ] Basic logs (structured)
  - [ ] Error logs and tracebacks captured
  - [ ] Metrics endpoints defined for later (Prometheus etc.)
- [ ] Tests:
  - [ ] Unit tests for core logic
  - [ ] API tests for telemetry ingest and node listing
  - [ ] Minimal load test performed (e.g., simulated many nodes)

---

## 3. AI / DSP

- [ ] Data pipeline:
  - [ ] Scripts to fetch/export node telemetry for training
  - [ ] Storage structure for raw and processed RF samples
- [ ] Feature extraction:
  - [ ] FFT pipeline implemented and reproducible
  - [ ] Feature definitions documented (bandpower, spectral shape, etc.)
- [ ] Model training:
  - [ ] Baseline anomaly detection model trained and evaluated
  - [ ] Metrics captured (precision/recall, false positive rate)
  - [ ] Model selection rationale documented
- [ ] Edge deployment:
  - [ ] Model converted to TFLite/TinyML format
  - [ ] On-device inference tested with real signals
  - [ ] Model size + latency within device constraints
- [ ] Versioning:
  - [ ] Model artifacts versioned (e.g., `models/v1`, `v2`, etc.)
  - [ ] Mapping from node → model version (in backend)
- [ ] Fallback:
  - [ ] Defined behavior if model not present or fails
  - [ ] Threshold-only mode still works

---

## 4. Dashboard / UI

- [ ] Auth story defined (even if basic)
- [ ] Node list view:
  - [ ] Shows node ID, last seen, battery, RF level, anomaly score
  - [ ] Filter/search by node ID or status
- [ ] RF activity view:
  - [ ] Time-series chart per node
  - [ ] Ability to change time window
- [ ] Alerts view:
  - [ ] List of anomalies detected
  - [ ] Ability to click through to see raw/processed signal context
- [ ] Config view:
  - [ ] Show node config settings
  - [ ] UI to request config changes (wired to backend)
- [ ] UX polish:
  - [ ] Basic responsive layout
  - [ ] Clear loading/error states
  - [ ] No hardcoded URLs (uses config)
- [ ] Tests:
  - [ ] Smoke test – app builds and loads cleanly
  - [ ] Critical components tested (optional)

---

## 5. Infra / DevOps

- [ ] `docker-compose` for local dev:
  - [ ] DB + backend + AI stack can be spun up with one command
- [ ] CI:
  - [ ] Lint + test pipeline for backend
  - [ ] (Optional) Firmware build check
  - [ ] Basic formatting checks (Black/isort, etc.)
- [ ] Environments:
  - [ ] Clear separation of dev vs prod configs
- [ ] Secrets:
  - [ ] No secrets in repo
  - [ ] Documented approach for env vars and sensitive keys

---

## 6. Documentation & IP

- [ ] High-level architecture diagram in `docs/architecture/`
- [ ] Firmware design doc (modules, data flow, timing)
- [ ] Backend API spec (OpenAPI auto-generated, plus docs)
- [ ] AI pipeline doc (data sources, features, model types)
- [ ] Deployment guide (local dev + "production" sim)
- [ ] IP separation notes:
  - [ ] What’s commercial O&L vs what would be defense-specific
  - [ ] Any constraints if collaborating with a prime (e.g., Raytheon)
