Firmware for O&L RF Mesh Node (Pico/ESP32).

## Milestone 1: Task skeleton

This repo now includes a minimal FreeRTOS-inspired scaffold to exercise the basic data flow:

- **RFScanTask**: collects a stub RF sample window from the ADC front-end.
- **FFTTflmTask**: extracts simple features and computes a placeholder anomaly score.
- **GNSSMonitorTask**: returns fixed GNSS metadata (valid fix, sat count, HDOP).
- **SensorHealthTask**: returns stub battery/temp/tilt + tamper flag.
- **PacketBuilderTask**: packages telemetry into a mesh frame and logs the send.

Key structs live in `include/telemetry.hpp` (`RFSampleWindow`, `RFEvent`, `GpsStatus`, `HealthStatus`, `MeshFrame`).
`src/tasks.cpp` orchestrates the sequential task execution for native builds; on-device this will map to real FreeRTOS tasks + queues.

## Milestone 2: Packet format & mesh framing

- **Mesh frame schema**: `MeshFrameHeader` now carries version, msg type (`Telemetry`, `Routing`, `Control`, `Ota`), TTL, hop count, seq, src/dest IDs. `MeshSecurity` holds nonce + auth tag placeholders for future AES-GCM.
- **Telemetry payload**: RF event + GNSS + health, routed through `MeshTelemetryPayload`.
- **Routing payload**: `MeshRoutingPayload` with up to 8 `RouteEntry` neighbors; attached to outgoing frames for status beacons.
- **Encoding**: `encode_mesh_frame` in `src/mesh_encode.cpp` serializes to a fixed buffer (hand-rolled stub; replace with CBOR/Proto + AES-GCM on-device).
- **Mesh module**: `send_mesh_frame` now logs encoded length and routing count; routing table helpers `add_route_entry` and `current_routing_payload` added.

## Milestone 3: OTA & fault tolerance scaffold

- **OTA state**: `ota.hpp`/`ota.cpp` define `OtaState`, `OtaStatus`, `ota_apply_chunk`, and `ota_verify_and_mark` (stubbed to succeed) for future signed updates.
- **Fault monitor**: `fault.hpp`/`fault.cpp` track fault activation and counters (watchdog resets, OTA failures, tamper events) with `record_fault`/`record_tamper`.
- **Frame payload**: `MeshFrame` now carries `FaultStatus` and `OtaStatus` so gateways/backends can observe OTA progress and faults; encoder includes these fields.
- **Tasks**: `TaskStatus` expanded with `OtaUpdateTask` and `FaultMonitorTask`; the orchestrated cycle checks tamper flags and includes OTA/fault info in outgoing frames.

## Milestone 4: FFT + TFLM-ready inference and AES-GCM framing (scaffold)

- **FFT features**: `model_inference.cpp` now computes FFT magnitudes (naive DFT for portability) and derives peak/avg dBm to feed anomaly scoring. Swap with a hardware FFT/TFLM interpreter when available.
- **Crypto interface**: `crypto.hpp`/`crypto.cpp` introduce an AES-GCM API (stubbed: copies plaintext and hashes into tag). `mesh_encode` wraps frames into `[auth_tag||ciphertext]`.
- **Mesh key**: `NodeConfig` includes a 32-byte `mesh_key`; nonce/tag are carried in `MeshSecurity` and used during encryption.
- **Wire-up**: `mesh.cpp` now encrypts encoded frames before send and logs cipher length. Build stays stub-friendly while defining the interfaces for real crypto/TFLM.

## Build + smoke test (host)

```bash
cd firmware
cmake -S . -B build
cmake --build build
cd build && ctest -V
```

This config builds against the host toolchain (no MCU SDK needed) and runs two tiny tests:

- `test_model_inference`: validates FFT feature extraction and normalized anomaly score.
- `test_mesh_routing`: sanity-checks route table capacity and encoding stubs.

Host fuzz/codec test (enabled by default via `ENABLE_FUZZ_TESTS=ON`):

- `test_mesh_codec_fuzz`: rapidly encodes random mesh frames, encrypts them, and ensures outputs stay within bounds. Uses `MockRadio` to accumulate sends.
- `test_mesh_retry`: simulates packet drops via a mock air queue and asserts delivery with retry attempts.
