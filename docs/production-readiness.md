# Production Readiness & Completion Roadmap

This document summarizes the current production-readiness criteria and remaining tasks for the distributed, low-power RF sensing mesh system. The system is explicitly commercial, passive, and unclassified.

## Scope
- Multi-node, low-power RF mesh sensors (ESP32/RP2040-class MCUs running FreeRTOS).
- Capabilities: RF anomaly sensing across multiple bands, GPS jamming/spoofing indicators, drone/UAS control signal detection (general, non-classified characteristics), interference detection, and unknown transmitter flagging.
- Communications: Wi-Fi/ESP-NOW/LoRa mesh with AES-256 encrypted packets and gateway uplink to a FastAPI or Go backend.
- Edge AI: Embedded FFT + anomaly detection via TFLite Micro.
- Backend: Telemetry ingestion, event storage, alerts, health monitoring, and real-time dashboards.

## Production Readiness Checklist
### Firmware & Edge AI
- **FreeRTOS task design**: Separate, bounded tasks for RF scanning, ADC sampling, GNSS monitoring, mesh networking, transport (LoRa/Wi-Fi), TFLite Micro inference, packet formation, encryption, OTA, and fault detection. Confirm stack sizes, priorities, and watchdog coverage.
- **RF front-end & ADC pipeline**: Validate RF switch control, per-band gain tables, ADC calibration, anti-aliasing filters, and sampling schedules that respect duty-cycle and power budgets.
- **Feature extraction**: Verified FFT windowing/overlap, noise floor tracking, and per-band feature normalization aligned with training data.
- **Inference runtime**: Benchmarked TFLite Micro model latency/throughput, static tensor arena sizing, and deterministic execution under ISR load.
- **Security**: AES-256-GCM (preferred) with hardware acceleration where available; per-node keys from secure provisioning; message counters and replay protection; secure boot and flash encryption where supported.
- **Resilience**: Brownout detection, tamper triggers (IMU/accelerometer events), thermal limits, and battery/charger safeguards; graceful degradation on RF front-end or sensor failure.
- **OTA safety**: Dual-partition or A/B image with rollback, signature verification, download chunking with CRCs, and rate limits; field-recovery procedure.

### Mesh & Transport
- **Mesh self-heal**: Auto-discovery, link-quality-based parent selection, hop count/TTL limits, and blacklisting of unstable links.
- **Routing state**: Periodic route table exchange with versioning and loop detection; convergence tests under churn; bounded memory usage.
- **Transport**: Reliable retry with backoff, fragmentation/reassembly, and QoS tiers (telemetry vs. alerts). Fallback between Wi-Fi/ESP-NOW/LoRa based on link budget and latency needs.
- **Time sync**: GNSS-backed or gateway-backed time synchronization for coherent FFT timestamps and multi-node correlation.

### Backend & Data Layer
- **Services**: Node registry (provisioning + config), telemetry ingestion API, RF event store, GPS interference log, health monitor, alert engine, WebSocket/Server-Sent Events for real-time graphs, and triangulation service for multi-node events.
- **Storage**: Schemas for node configs, RF bands, FFT snapshots, GPS quality metrics, alerts, and mesh routing snapshots. Indexing for time-series queries and geo lookups.
- **Scalability & durability**: Load test ingest/websocket paths; retention policies; backups; infrastructure-as-code for reproducible environments.
- **Security & IAM**: mTLS or token auth per node, RBAC for operators, per-tenant isolation, audit logs, and secret rotation. No classified data stored or processed.

### Observability & Testing
- **Metrics**: RF scan cadence, packet loss, routing changes, inference latency, battery/temp trends, GNSS lock quality, OTA success rate.
- **Tracing/logging**: Structured logs with event IDs; radio driver debug levels; correlation IDs from node to backend.
- **Test coverage**: HIL tests for RF front-end paths, fuzzing of packet parser/serializer, soak tests for mesh stability, and regression tests for FFT/anomaly pipeline.
- **Field validation**: Drive tests for coverage, multipath environments, and EMI-rich sites; thermal/heat testing; battery run-time; enclosure ingress protection checks.

### Deployment & Operations
- **Provisioning**: Secure key injection, unique IDs, and config templates per site. Factory test harness for RF, GNSS, sensors, and mesh comms.
- **Gateway role**: Deterministic gateway election or static assignment; graceful failover; buffering when uplink is down.
- **Alerts & workflows**: Severity scoring, deduplication, rate limits, and operator runbooks for RF anomaly, GPS spoof/jam indicators, and drone-control signatures (unclassified, generic characteristics only).
- **Compliance & safety**: Regional RF exposure and duty-cycle compliance; battery and electrical safety certifications as required; ensure no intentional emissions beyond protocol requirements.

## Remaining Work to Reach Production
- Finalize FreeRTOS task map with concrete priorities/stack sizes and validate under load with watchdogs.
- Lock down AES-256-GCM key management and secure boot/flash encryption per MCU capability; document provisioning steps.
- Complete packet schema (CBOR or protobuf) with routing headers, counters, and OTA command messages; implement fuzz tests.
- Implement mesh route convergence tests (mobility, node loss) and quantify healing time; tune link-quality thresholds.
- Benchmark TFLite Micro model on target MCUs; optimize tensor arena and quantization; align feature scaling between training and inference.
- Build backend ingest + WebSocket path load tests; confirm alert latency budgets and storage retention policies.
- Ship dashboard views for node health, RF waterfall/FFT snapshots, GPS interference timelines, and alert triage.
- Validate OTA rollback and field-recovery procedures end-to-end; add staged rollout controls.
- Complete field deployment checklist (mounting, antennas, grounding, thermal, tamper seals, SIM/Wi-Fi credentials, coverage survey) and operator runbooks.

## Reference Examples (to be expanded in repo)
- **Firmware (FreeRTOS/C++)**: Modular tasks for RF scan, ADC sampling to ring buffer, inference invocation, encrypted mesh packet send, GNSS/time-sync, and OTA handler.
- **Backend (Python FastAPI or Go)**: Node registry CRUD, authenticated telemetry ingest, RF event persistence, alert engine with severity scoring, WebSocket real-time FFT feed, and triangulation stub.
- **Dashboard (React/Next.js)**: Live RF spectrum charts, node health cards, alert list with severity, and GPS interference timeline.

## Readiness Score & Gaps
- **Score (0–100): 50/100.** The firmware, backend, and dashboard all build and pass smoke tests (host CMake/CTest, FastAPI pytest, Vite build) with CI coverage. The feature set is still skeletal and lacks production hardening, telemetry scale testing, and real RF/ML integration.

- **Remaining work to reach the project goal:**
  - **Firmware baseline:** Target the real MCU + RF front-end with validated drivers, ADC capture, GNSS monitoring, mesh stack integration, packet codec (CBOR/protobuf), AES-256-GCM security, OTA flow, watchdog coverage, and HIL tests around drivers/mesh/OTA/faults.
  - **Mesh protocols and transport:** Implement routing state exchange, link-quality-based parent selection, retries/fragmentation, and time synchronization across nodes. Run soak/chaos tests to characterize churn, mobility, node loss, and recovery behavior.
  - **Backend services:** Harden ingest, node registry, alert engine, and WebSocket feeds with schemas/migrations, auth (mTLS/token), load/latency tests, retention/backup, and observability hooks (logs/metrics/traces) against Postgres/Timescale.
  - **AI pipeline:** Extend the existing synthetic IQ→FFT→int8 TFLite pipeline (generation, feature extraction, training, quantization, contract + latency benchmark) to real RF datasets and on-device integration. Validate model performance on representative RF environments, finalize the TFLM-ready header/contract, and benchmark latency/memory/power on target MCUs. Pipeline now accepts real IQ/feature npz inputs (`--iq-path/--features-path`) and records dataset version + feature stats in the contract for on-device normalization parity; next step is to train/evaluate on real captures and run TFLM benchmarks on target MCUs.
  - **Dashboard:** Build real-time spectrum/FFT visualizations, node health + topology views, alert triage flows, and GPS interference timelines, wired to live backend data alongside mocks.
  - **Operations:** Extend CI to lint/static analysis; add IaC for backend + observability stack, deployment runbooks, field deployment checklist with SLOs and alerting policies.

## Security Note
This system is commercial, passive, and unclassified. It must not include any classified data, classified processing, or offensive/weaponized capabilities.
