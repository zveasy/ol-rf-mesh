from datetime import datetime
from typing import Any, Dict, List, Optional

from pydantic import BaseModel
from pydantic import Field


class TelemetryIn(BaseModel):
    """Legacy simple telemetry payload kept for compatibility.

    Represents a single RF/health snapshot from a node.
    """

    node_id: str
    timestamp: int
    rf_power_dbm: float
    battery_v: float
    temp_c: float
    anomaly_score: float


class NodeStatus(BaseModel):
    """High-level status snapshot for a node used by the dashboard."""

    node_id: str
    last_seen: datetime
    battery_v: float
    rf_power_dbm: float
    temp_c: float
    anomaly_score: float
    gps_quality: Optional[Dict[str, Any]] = None


class NodeBase(BaseModel):
    node_id: str
    name: Optional[str] = None
    role: str  # "edge" or "gateway"


class NodeCreate(NodeBase):
    hw_serial: str
    lat: Optional[float] = None
    lon: Optional[float] = None
    alt: Optional[float] = None


class Node(NodeBase):
    hw_serial: str
    lat: Optional[float]
    lon: Optional[float]
    alt: Optional[float]
    created_at: datetime
    updated_at: datetime


class NodeConfig(BaseModel):
    node_id: str
    rf_bands: List[str]
    sample_window_size: int
    sample_rate_hz: int
    duty_cycle: float
    alert_thresholds: Dict[str, float]
    mesh_prefs: Dict[str, Any]
    model_version: str


class RfEventIn(BaseModel):
    node_id: str
    timestamp: datetime
    band_id: str
    center_freq_hz: float
    bin_width_hz: float
    anomaly_score: float
    features: List[float]


class SpectrumPoint(BaseModel):
    freq_hz: float
    power_dbm: float


class SpectrumSnapshot(BaseModel):
    band_id: str
    captured_at: datetime
    points: List[SpectrumPoint] = Field(default_factory=list)


class GpsQualityIn(BaseModel):
    node_id: str
    timestamp: datetime
    num_sats: int
    snr_avg: float
    hdop: float
    valid_fix: bool
    jamming_indicator: float
    spoof_indicator: float


class AlertIn(BaseModel):
    node_ids: List[str]
    type: str  # rf_anomaly, gps_jam, tamper, offline, etc.
    severity: str  # low/med/high/critical
    message: str
    created_at: Optional[datetime] = None


class Alert(AlertIn):
    id: int
    created_at: datetime


class Paginated(BaseModel):
    total: int
    limit: int
    offset: int


class PaginatedRfEvents(Paginated):
    items: List[RfEventIn]


class PaginatedGps(Paginated):
    items: List[GpsQualityIn]


class Rollup(BaseModel):
    node_id: str
    last_updated: datetime
    rf_events_24h: int
    avg_anomaly_24h: float
    gps_jam_events_24h: int
