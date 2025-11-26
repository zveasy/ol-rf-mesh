from typing import Dict, List

from .schemas import Alert, GpsQualityIn, Node, NodeStatus, RfEventIn

# In-memory stores for the MVP backend.
NODES: Dict[str, Node] = {}
NODE_STATUS: Dict[str, NodeStatus] = {}
RF_EVENTS: List[RfEventIn] = []
GPS_LOGS: List[GpsQualityIn] = []
ALERTS: List[Alert] = []
