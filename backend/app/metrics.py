from prometheus_client import Counter, Gauge, Histogram

# Request-level metrics
REQUESTS_TOTAL = Counter(
    "ol_rf_mesh_requests_total",
    "Total HTTP requests",
    labelnames=("method", "path", "status", "auth"),
)

REQUEST_LATENCY_MS = Histogram(
    "ol_rf_mesh_request_latency_ms",
    "Request latency in milliseconds",
    buckets=(5, 10, 25, 50, 100, 250, 500, 1000, 2000, float("inf")),
    labelnames=("method", "path"),
)

# Backend state gauges
NODES_KNOWN = Gauge("ol_rf_mesh_nodes_known", "Number of node status records cached")
RF_EVENTS_CACHED = Gauge("ol_rf_mesh_rf_events_cached", "RF events stored in memory")
GPS_LOGS_CACHED = Gauge("ol_rf_mesh_gps_logs_cached", "GPS logs stored in memory")
ALERTS_CACHED = Gauge("ol_rf_mesh_alerts_cached", "Alerts stored in memory")
REQUEST_ERRORS_TOTAL = Counter(
    "ol_rf_mesh_request_errors_total",
    "Total HTTP requests resulting in error",
    labelnames=("method", "path", "status"),
)
MOCK_QUEUE_DEPTH = Gauge("ol_rf_mesh_mock_queue_depth", "Mock queues length (RF events + alerts)")
