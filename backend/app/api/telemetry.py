from datetime import datetime
from typing import List

from fastapi import APIRouter

from ..schemas import GpsQualityIn, NodeStatus, RfEventIn, TelemetryIn, AlertIn
from .. import state, db
from .ws import manager
from .alerts import store_and_broadcast_alert

router = APIRouter()


@router.post("/")
async def ingest_telemetry(data: TelemetryIn):
    """Legacy single-sample telemetry endpoint.

    Useful for quick tests before nodes send richer RF/GPS payloads.
    """

    state.RF_EVENTS.append(
        RfEventIn(
            node_id=data.node_id,
            timestamp=datetime.utcfromtimestamp(data.timestamp),
            band_id="legacy",
            center_freq_hz=0.0,
            bin_width_hz=0.0,
            anomaly_score=data.anomaly_score,
            features=[data.rf_power_dbm],
        )
    )

    state.NODE_STATUS[data.node_id] = NodeStatus(
        node_id=data.node_id,
        last_seen=datetime.utcfromtimestamp(data.timestamp),
        battery_v=data.battery_v,
        rf_power_dbm=data.rf_power_dbm,
        temp_c=data.temp_c,
        anomaly_score=data.anomaly_score,
        gps_quality=state.NODE_STATUS.get(data.node_id, None)
        and state.NODE_STATUS[data.node_id].gps_quality,
    )

    await manager.broadcast({"type": "telemetry", "node_id": data.node_id, "anomaly_score": data.anomaly_score})
    db.insert_rf_event(
        RfEventIn(
            node_id=data.node_id,
            timestamp=datetime.utcfromtimestamp(data.timestamp),
            band_id="legacy",
            center_freq_hz=0.0,
            bin_width_hz=0.0,
            anomaly_score=data.anomaly_score,
            features=[data.rf_power_dbm],
        )
    )
    if data.anomaly_score >= 0.8:
        await store_and_broadcast_alert(
            AlertIn(
                node_ids=[data.node_id],
                type="rf_anomaly",
                severity="med",
                message=f"Anomaly score {data.anomaly_score:.2f}",
                created_at=datetime.utcfromtimestamp(data.timestamp),
            )
        )
    return {"status": "ok"}


@router.post("/rf-events")
async def ingest_rf_events(events: List[RfEventIn]):
    """Batch RF event ingestion endpoint.

    Firmware should prefer this path for FFT/anomaly outputs.
    """

    for e in events:
        state.RF_EVENTS.append(e)
        db.insert_rf_event(e)

        prev = state.NODE_STATUS.get(e.node_id)
        state.NODE_STATUS[e.node_id] = NodeStatus(
            node_id=e.node_id,
            last_seen=e.timestamp,
            battery_v=prev.battery_v if prev else 0.0,
            rf_power_dbm=e.features[0] if e.features else (prev.rf_power_dbm if prev else 0.0),
            temp_c=prev.temp_c if prev else 0.0,
            anomaly_score=e.anomaly_score,
            gps_quality=prev.gps_quality if prev else None,
        )

        await manager.broadcast(
            {"type": "rf_event", "node_id": e.node_id, "timestamp": e.timestamp.isoformat(), "anomaly_score": e.anomaly_score}
        )
        if e.anomaly_score >= 0.8:
            await store_and_broadcast_alert(
                AlertIn(
                    node_ids=[e.node_id],
                    type="rf_anomaly",
                    severity="high" if e.anomaly_score >= 0.95 else "med",
                    message=f"High anomaly score {e.anomaly_score:.2f}",
                    created_at=e.timestamp,
                )
            )

    return {"accepted": len(events)}


@router.post("/gps")
async def ingest_gps_quality(samples: List[GpsQualityIn]):
    """Batch GPS quality ingestion endpoint."""

    for s in samples:
        state.GPS_LOGS.append(s)
        db.insert_gps_log(s)

        prev = state.NODE_STATUS.get(s.node_id)
        if prev:
            state.NODE_STATUS[s.node_id] = prev.copy(
                update={
                    "last_seen": s.timestamp,
                    "gps_quality": {
                        "num_sats": s.num_sats,
                        "snr_avg": s.snr_avg,
                        "hdop": s.hdop,
                        "valid_fix": s.valid_fix,
                        "jamming_indicator": s.jamming_indicator,
                        "spoof_indicator": s.spoof_indicator,
                    },
                },
            )

        await manager.broadcast(
            {
                "type": "gps_quality",
                "node_id": s.node_id,
                "timestamp": s.timestamp.isoformat(),
                "num_sats": s.num_sats,
                "hdop": s.hdop,
                "valid_fix": s.valid_fix,
            }
        )
        if s.jamming_indicator > 0.5 or s.spoof_indicator > 0.5:
            await store_and_broadcast_alert(
                AlertIn(
                    node_ids=[s.node_id],
                    type="gps_jam" if s.jamming_indicator > s.spoof_indicator else "gps_spoof",
                    severity="high",
                    message="GNSS interference detected",
                    created_at=s.timestamp,
                )
            )

    return {"accepted": len(samples)}
