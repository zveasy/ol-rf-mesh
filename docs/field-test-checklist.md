# Field Test Checklist v0 (lab)

- **Hardware sanity**: Inspect antenna/connector tightness, battery voltage, and enclosure seals. Verify tamper switch/IMU flags clear.
- **Firmware**: Build host smoke (`cmake --build build && ctest`) and flash target image; confirm watchdog + OTA rollback paths are enabled. Record firmware version + config.
- **RF/GNSS validation**: Run a short RF scan near known signals; capture FFT snapshot. Verify GNSS lock (sats, HDOP) and jamming/spoof indicators stay low indoors.
- **Mesh/network**: Bring up 2–3 nodes; confirm route table populates and TTL/hop counts behave. Validate gateway uplink buffering when link drops.
- **Backend/dashboard**: Start backend (`uvicorn app.main:app --reload`), ensure `/health`/`/metrics` 200, and load dashboard pointing to backend. Verify mock data renders even if nodes are silent.
- **Telemetry injection**: POST a sample to `/telemetry/rf-events` and check it appears in dashboard + WebSocket stream. Verify alert fan-out when anomaly score >= 0.8.
- **Logging/collection**: Capture logs from nodes + backend during test. Note battery/temp trends, RF noise floor, and any packet loss.
