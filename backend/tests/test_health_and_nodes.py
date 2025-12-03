from fastapi.testclient import TestClient
from datetime import datetime, UTC

from app.main import app


client = TestClient(app)


def test_health_ok():
    response = client.get("/health")
    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


def test_register_and_list_node():
    payload = {
        "node_id": "node-1",
        "name": "Test Node",
        "role": "edge",
        "hw_serial": "HW123",
        "lat": 1.0,
        "lon": 2.0,
        "alt": 3.0,
    }
    create_resp = client.post("/nodes/", json=payload)
    assert create_resp.status_code == 200
    created = create_resp.json()
    assert created["node_id"] == payload["node_id"]

    list_resp = client.get("/nodes/")
    assert list_resp.status_code == 200
    nodes = list_resp.json()
    assert any(n["node_id"] == payload["node_id"] for n in nodes)


def test_metrics_and_mock_data():
    metrics_resp = client.get("/metrics")
    assert metrics_resp.status_code == 200
    assert metrics_resp.headers.get("content-type", "").startswith("text/plain")
    text = metrics_resp.text
    assert "ol_rf_mesh_requests_total" in text
    assert "auth=\"none\"" in text or "auth=\"token\"" in text or "auth=\"mtls\"" in text
    assert "ol_rf_mesh_nodes_known" in text
    assert metrics_resp.headers.get("X-Request-ID")

    mock_nodes_resp = client.get("/mock/nodes")
    assert mock_nodes_resp.status_code == 200
    mock_nodes = mock_nodes_resp.json()
    assert len(mock_nodes) >= 1
    assert "node_id" in mock_nodes[0]

    mock_events_resp = client.get("/mock/events")
    assert mock_events_resp.status_code == 200
    payload = mock_events_resp.json()
    assert "rf_events" in payload and "alerts" in payload


def test_spectrum_from_ingest():
    now_iso = datetime.now(UTC).isoformat()
    payload = [
        {
            "node_id": "node-spect",
            "timestamp": now_iso,
            "band_id": "915mhz",
            "center_freq_hz": 915.0e6,
            "bin_width_hz": 1.0e3,
            "anomaly_score": 0.2,
            "features": [-60.0, -58.0, -57.0],
        }
    ]
    resp = client.post("/telemetry/rf-events", json=payload)
    assert resp.status_code == 200

    spec_resp = client.get("/spectrum/latest")
    assert spec_resp.status_code == 200
    spectra = spec_resp.json()
    assert len(spectra) >= 1
    band_ids = [s["band_id"] for s in spectra]
    assert "915mhz" in band_ids

    history_resp = client.get("/spectrum/history?band_id=915mhz&limit=5")
    assert history_resp.status_code == 200
    history = history_resp.json()
    assert history
    assert history[0]["band_id"] == "915mhz"
