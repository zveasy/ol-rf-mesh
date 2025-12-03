import time
import httpx
import os
import pytest

BASE = os.getenv("BACKEND_URL", "http://127.0.0.1:8000")
API_TOKEN = os.getenv("API_TOKEN")
RUN_LOAD = os.getenv("RUN_LOAD_TEST") == "1"


def _headers():
    if API_TOKEN:
        return {"Authorization": f"Bearer {API_TOKEN}"}
    return {}


def test_rf_events_load():
    """Light load test: 50 batches of RF events, expect 200 and sub-200ms latency."""

    if not RUN_LOAD:
        pytest.skip("RUN_LOAD_TEST not set; skipping load test", allow_module_level=True)

    try:
        httpx.get(BASE + "/health", timeout=2)
    except Exception:
        pytest.skip(f"Backend not reachable at {BASE}, skipping load test", allow_module_level=True)

    events = [
        {
            "node_id": "node-load",
            "timestamp": "2024-01-01T00:00:00Z",
            "band_id": "test-band",
            "center_freq_hz": 915000000.0,
            "bin_width_hz": 12500.0,
            "anomaly_score": 0.2,
            "features": [-60.0, -59.5, -61.2],
        }
    ]

    with httpx.Client(base_url=BASE, headers=_headers(), timeout=5) as client:
        latencies = []
        for _ in range(50):
            t0 = time.perf_counter()
            resp = client.post("/telemetry/rf-events", json=events)
            dt = (time.perf_counter() - t0) * 1000
            latencies.append(dt)
            assert resp.status_code == 200
            assert resp.json().get("accepted") == len(events)

        avg = sum(latencies) / len(latencies)
        assert avg < 200
