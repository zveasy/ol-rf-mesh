from datetime import datetime, timedelta, UTC
from typing import List, Tuple

from sqlalchemy import create_engine, select, func
from sqlalchemy.orm import Session, sessionmaker
from sqlalchemy.pool import StaticPool

from .models import Base, NodeModel, RfEventModel, GpsLogModel, AlertModel, NodeRollupModel, SpectrumModel
from .schemas import Alert, AlertIn, GpsQualityIn, Node, RfEventIn, SpectrumSnapshot, SpectrumPoint
from .config import get_database_url

DATABASE_URL = get_database_url()

engine_kwargs = {"future": True}

# SQLite needs thread override for TestClient + background tasks; in-memory gets StaticPool.
if DATABASE_URL.startswith("sqlite"):
    engine_kwargs["connect_args"] = {"check_same_thread": False}
    if DATABASE_URL.endswith(":memory:"):
        engine_kwargs["poolclass"] = StaticPool

engine = create_engine(DATABASE_URL, **engine_kwargs)
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False, future=True)


def init_db() -> None:
    Base.metadata.create_all(engine)


def insert_node(node: Node) -> None:
    with SessionLocal() as session:
        session.merge(
            NodeModel(
                node_id=node.node_id,
                name=node.name,
                role=node.role,
                hw_serial=node.hw_serial,
                lat=node.lat,
                lon=node.lon,
                alt=node.alt,
                created_at=node.created_at,
                updated_at=node.updated_at,
            )
        )
        session.commit()


def insert_rf_event(event: RfEventIn) -> None:
    with SessionLocal() as session:
        session.add(
            RfEventModel(
                node_id=event.node_id,
                timestamp=event.timestamp,
                band_id=event.band_id,
                center_freq_hz=event.center_freq_hz,
                bin_width_hz=event.bin_width_hz,
                anomaly_score=event.anomaly_score,
                features_json=event.features,
            )
        )
        session.commit()


def insert_gps_log(sample: GpsQualityIn) -> None:
    with SessionLocal() as session:
        session.add(
            GpsLogModel(
                node_id=sample.node_id,
                timestamp=sample.timestamp,
                num_sats=sample.num_sats,
                snr_avg=sample.snr_avg,
                hdop=sample.hdop,
                valid_fix=sample.valid_fix,
                jamming_indicator=sample.jamming_indicator,
                spoof_indicator=sample.spoof_indicator,
            )
        )
        session.commit()


def insert_alert(alert: AlertIn) -> Alert:
    created_at = alert.created_at or datetime.now(UTC)
    with SessionLocal() as session:
        model = AlertModel(
            node_ids_json=alert.node_ids,
            type=alert.type,
            severity=alert.severity,
            message=alert.message,
            created_at=created_at,
        )
        session.add(model)
        session.commit()
        session.refresh(model)
        return Alert(
            id=model.id,
            created_at=model.created_at,
            node_ids=model.node_ids_json,
            type=model.type,
            severity=model.severity,
            message=model.message,
        )


def recent_alerts(limit: int = 100) -> List[Alert]:
    with SessionLocal() as session:
        rows = session.execute(
            select(AlertModel).order_by(AlertModel.id.desc()).limit(limit)
        ).scalars().all()

    return [
        Alert(
            id=row.id,
            node_ids=row.node_ids_json,
            type=row.type,
            severity=row.severity,
            message=row.message,
            created_at=row.created_at,
        )
        for row in rows
    ]


def list_rf_events(limit: int = 100, offset: int = 0) -> Tuple[List[RfEventIn], int]:
    with SessionLocal() as session:
        total = session.scalar(select(func.count()).select_from(RfEventModel)) or 0
        rows = session.execute(
            select(RfEventModel)
            .order_by(RfEventModel.timestamp.desc())
            .limit(limit)
            .offset(offset)
        ).scalars().all()

    events: List[RfEventIn] = []
    for row in rows:
        events.append(
            RfEventIn(
                node_id=row.node_id,
                timestamp=row.timestamp,
                band_id=row.band_id,
                center_freq_hz=row.center_freq_hz,
                bin_width_hz=row.bin_width_hz,
                anomaly_score=row.anomaly_score,
                features=row.features_json,
            )
        )
    return events, total


def list_gps_logs(limit: int = 100, offset: int = 0) -> Tuple[List[GpsQualityIn], int]:
    with SessionLocal() as session:
        total = session.scalar(select(func.count()).select_from(GpsLogModel)) or 0
        rows = session.execute(
            select(GpsLogModel)
            .order_by(GpsLogModel.timestamp.desc())
            .limit(limit)
            .offset(offset)
        ).scalars().all()

    samples: List[GpsQualityIn] = []
    for row in rows:
        samples.append(
            GpsQualityIn(
                node_id=row.node_id,
                timestamp=row.timestamp,
                num_sats=row.num_sats,
                snr_avg=row.snr_avg,
                hdop=row.hdop,
                valid_fix=row.valid_fix,
                jamming_indicator=row.jamming_indicator,
                spoof_indicator=row.spoof_indicator,
            )
        )
    return samples, total


def prune_rf_events(older_than: datetime) -> int:
    with SessionLocal() as session:
        result = session.query(RfEventModel).filter(RfEventModel.timestamp < older_than).delete()
        session.commit()
        return result


def prune_gps_logs(older_than: datetime) -> int:
    with SessionLocal() as session:
        result = session.query(GpsLogModel).filter(GpsLogModel.timestamp < older_than).delete()
        session.commit()
        return result


def compute_rollups(now: datetime) -> None:
    """Compute simple 24h rollups per node and store in node_rollups."""
    day_ago = now - timedelta(hours=24)
    with SessionLocal() as session:
        rows = session.execute(
            select(
                RfEventModel.node_id,
                func.count(RfEventModel.id),
                func.avg(RfEventModel.anomaly_score),
            )
            .where(RfEventModel.timestamp >= day_ago)
            .group_by(RfEventModel.node_id)
        ).all()

        jam_rows = session.execute(
            select(GpsLogModel.node_id, func.count(GpsLogModel.id))
            .where(
                GpsLogModel.timestamp >= day_ago,
                (GpsLogModel.jamming_indicator > 0.5) | (GpsLogModel.spoof_indicator > 0.5),
            )
            .group_by(GpsLogModel.node_id)
        ).all()
        jam_map = {row[0]: row[1] for row in jam_rows}

        for node_id, count, avg_anom in rows:
            rollup = NodeRollupModel(
                node_id=node_id,
                last_updated=now,
                rf_events_24h=count,
                avg_anomaly_24h=avg_anom or 0.0,
                gps_jam_events_24h=jam_map.get(node_id, 0),
            )
            session.merge(rollup)
        session.commit()


def list_rollups() -> List[NodeRollupModel]:
    with SessionLocal() as session:
        return session.query(NodeRollupModel).all()


def insert_spectrum(snapshot: SpectrumSnapshot) -> None:
    with SessionLocal() as session:
        session.add(
            SpectrumModel(
                band_id=snapshot.band_id,
                captured_at=snapshot.captured_at,
                points_json=[{"freq_hz": p.freq_hz, "power_dbm": p.power_dbm} for p in snapshot.points],
            )
        )
        session.commit()


def list_latest_spectrum() -> List[SpectrumSnapshot]:
    """Return latest snapshot per band (best-effort)."""
    with SessionLocal() as session:
        rows = session.execute(
            select(SpectrumModel).order_by(SpectrumModel.captured_at.desc()).limit(50)
        ).scalars().all()
    seen = set()
    snapshots: List[SpectrumSnapshot] = []
    for row in rows:
        if row.band_id in seen:
            continue
        seen.add(row.band_id)
        snapshots.append(
            SpectrumSnapshot(
                band_id=row.band_id,
                captured_at=row.captured_at,
                points=[SpectrumPoint(**p) for p in row.points_json],
            )
        )
    return snapshots


def list_spectrum_history(band_id: str | None = None, since: datetime | None = None, limit: int = 100, offset: int = 0) -> List[SpectrumSnapshot]:
    query = select(SpectrumModel)
    if band_id:
        query = query.where(SpectrumModel.band_id == band_id)
    if since:
        query = query.where(SpectrumModel.captured_at >= since)
    query = query.order_by(SpectrumModel.captured_at.desc()).limit(limit).offset(offset)

    with SessionLocal() as session:
        rows = session.execute(query).scalars().all()

    return [
        SpectrumSnapshot(
            band_id=row.band_id,
            captured_at=row.captured_at,
            points=[SpectrumPoint(**p) for p in row.points_json],
        )
        for row in rows
    ]
