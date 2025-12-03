from datetime import datetime, UTC

from fastapi.testclient import TestClient

from app.main import app


client = TestClient(app)


def test_ingest_rf_events_batch():
    now = datetime.now(UTC)
    payload = [
        {
            "node_id": "node-telemetry-1",
            "timestamp": now.isoformat(),
            "band_id": "test-band",
            "center_freq_hz": 915.0e6,
            "bin_width_hz": 1.0e3,
            "anomaly_score": 0.5,
            "features": [
                -50.0,
            ],
        }
    ]

    resp = client.post("/telemetry/rf-events", json=payload)
    assert resp.status_code == 200
    body = resp.json()
    assert body["accepted"] == len(payload)
