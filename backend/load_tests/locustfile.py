from datetime import datetime, timedelta, UTC
import random
from locust import HttpUser, task, between


def _rf_event(node_id: str) -> dict:
    now = datetime.now(UTC)
    return {
        "node_id": node_id,
        "timestamp": now.isoformat(),
        "band_id": random.choice(["915mhz", "2.4ghz", "5ghz"]),
        "center_freq_hz": random.choice([915_000_000.0, 2_420_000_000.0, 5_800_000_000.0]),
        "bin_width_hz": random.choice([12_500.0, 20_000.0]),
        "anomaly_score": random.random(),
        "features": [-50.0 + random.random() * 10.0 for _ in range(4)],
    }


class MeshUser(HttpUser):
    wait_time = between(0.1, 1.5)

    @task(3)
    def ingest_batch(self):
        payload = [_rf_event(f"node-{i}") for i in range(3)]
        self.client.post("/telemetry/rf-events", json=payload)

    @task(1)
    def history_page(self):
        self.client.get("/history/rf-events?limit=10&offset=0")

    @task(1)
    def health(self):
        self.client.get("/health")

    def on_start(self):
        # Warm up metrics and mock paths
        self.client.get("/mock/nodes")
