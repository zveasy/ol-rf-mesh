from typing import Dict, List

from .schemas import Alert, GpsQualityIn, Node, NodeStatus, RfEventIn, SpectrumSnapshot

# In-memory stores for the MVP backend.
NODES: Dict[str, Node] = {}
NODE_STATUS: Dict[str, NodeStatus] = {}
RF_EVENTS: List[RfEventIn] = []
GPS_LOGS: List[GpsQualityIn] = []
ALERTS: List[Alert] = []
SPECTRUM_BY_BAND: Dict[str, SpectrumSnapshot] = {}
