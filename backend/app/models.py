from datetime import datetime
from typing import Optional

from sqlalchemy import Column, Float, Integer, String, Boolean, JSON, DateTime
from sqlalchemy.orm import declarative_base

Base = declarative_base()


class NodeModel(Base):
    __tablename__ = "nodes"

    node_id = Column(String, primary_key=True)
    name = Column(String, nullable=True)
    role = Column(String, nullable=False)
    hw_serial = Column(String, nullable=False)
    lat = Column(Float, nullable=True)
    lon = Column(Float, nullable=True)
    alt = Column(Float, nullable=True)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow)


class RfEventModel(Base):
    __tablename__ = "rf_events"

    id = Column(Integer, primary_key=True, autoincrement=True)
    node_id = Column(String, index=True, nullable=False)
    timestamp = Column(DateTime, index=True, nullable=False)
    band_id = Column(String, nullable=False)
    center_freq_hz = Column(Float, nullable=False)
    bin_width_hz = Column(Float, nullable=False)
    anomaly_score = Column(Float, nullable=False)
    features_json = Column(JSON, nullable=False)


class GpsLogModel(Base):
    __tablename__ = "gps_logs"

    id = Column(Integer, primary_key=True, autoincrement=True)
    node_id = Column(String, index=True, nullable=False)
    timestamp = Column(DateTime, index=True, nullable=False)
    num_sats = Column(Integer, nullable=False)
    snr_avg = Column(Float, nullable=False)
    hdop = Column(Float, nullable=False)
    valid_fix = Column(Boolean, nullable=False)
    jamming_indicator = Column(Float, nullable=False)
    spoof_indicator = Column(Float, nullable=False)


class AlertModel(Base):
    __tablename__ = "alerts"

    id = Column(Integer, primary_key=True, autoincrement=True)
    node_ids_json = Column(JSON, nullable=False)
    type = Column(String, nullable=False)
    severity = Column(String, nullable=False)
    message = Column(String, nullable=False)
    created_at = Column(DateTime, default=datetime.utcnow)


class NodeRollupModel(Base):
    __tablename__ = "node_rollups"

    node_id = Column(String, primary_key=True)
    last_updated = Column(DateTime, default=datetime.utcnow)
    rf_events_24h = Column(Integer, default=0)
    avg_anomaly_24h = Column(Float, default=0.0)
    gps_jam_events_24h = Column(Integer, default=0)
