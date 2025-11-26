from pydantic import BaseModel


class TelemetryIn(BaseModel):
    node_id: str
    timestamp: int
    rf_power_dbm: float
    battery_v: float
    temp_c: float
    anomaly_score: float


class NodeStatus(BaseModel):
    node_id: str
    last_seen: int
    battery_v: float
    rf_power_dbm: float
    temp_c: float
    anomaly_score: float
