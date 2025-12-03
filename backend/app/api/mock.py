from datetime import datetime, timedelta, UTC
from typing import List

from fastapi import APIRouter

from ..schemas import Alert, NodeStatus, RfEventIn

router = APIRouter()


def _mock_nodes() -> List[NodeStatus]:
    now = datetime.now(UTC)
    return [
        NodeStatus(
            node_id="demo-gateway",
            last_seen=now - timedelta(seconds=15),
            battery_v=4.0,
            rf_power_dbm=-42.0,
            temp_c=37.0,
            anomaly_score=0.08,
            gps_quality={"num_sats": 10, "hdop": 0.9, "valid_fix": True},
        ),
        NodeStatus(
            node_id="demo-edge-1",
            last_seen=now - timedelta(minutes=2),
            battery_v=3.72,
            rf_power_dbm=-55.0,
            temp_c=33.5,
            anomaly_score=0.22,
            gps_quality={"num_sats": 7, "hdop": 1.4, "valid_fix": True},
        ),
        NodeStatus(
            node_id="demo-edge-2",
            last_seen=now - timedelta(minutes=5),
            battery_v=3.61,
            rf_power_dbm=-63.5,
            temp_c=36.2,
            anomaly_score=0.74,
            gps_quality={"num_sats": 5, "hdop": 2.1, "valid_fix": False, "jamming_indicator": 0.18},
        ),
    ]


def _mock_events() -> List[RfEventIn]:
    now = datetime.now(UTC)
    return [
        RfEventIn(
            node_id="demo-edge-2",
            timestamp=now - timedelta(seconds=30),
            band_id="915mhz",
            center_freq_hz=915000000.0,
            bin_width_hz=12500.0,
            anomaly_score=0.82,
            features=[-48.0, -49.5, -52.1],
        ),
        RfEventIn(
            node_id="demo-edge-1",
            timestamp=now - timedelta(minutes=3),
            band_id="2.4ghz",
            center_freq_hz=2420000000.0,
            bin_width_hz=20000.0,
            anomaly_score=0.21,
            features=[-60.0, -58.5, -57.0],
        ),
    ]


def _mock_alerts() -> List[Alert]:
    now = datetime.now(UTC)
    return [
        Alert(
            id=1,
            node_ids=["demo-edge-2"],
            type="rf_anomaly",
            severity="med",
            message="High anomaly score near 915 MHz",
            created_at=now - timedelta(minutes=1),
        ),
        Alert(
            id=2,
            node_ids=["demo-edge-2"],
            type="gps_jam",
            severity="low",
            message="Weak GNSS lock quality",
            created_at=now - timedelta(minutes=6),
        ),
    ]


@router.get("/nodes", response_model=List[NodeStatus])
async def mock_nodes() -> List[NodeStatus]:
    """Static mock data to keep the dashboard usable before real nodes connect."""

    return _mock_nodes()


@router.get("/events")
async def mock_events():
    """RF events + alerts mock payload for smoke tests."""

    return {"rf_events": _mock_events(), "alerts": _mock_alerts()}
