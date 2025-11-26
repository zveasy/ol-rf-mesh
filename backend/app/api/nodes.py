from datetime import datetime
from typing import Dict, List

from fastapi import APIRouter, HTTPException

from ..schemas import Node, NodeCreate, NodeStatus
from .. import state, db

router = APIRouter()


@router.post("/", response_model=Node)
async def register_node(node: NodeCreate) -> Node:
    """Register a node with basic metadata.

    For now this uses an in-memory registry; later this can be backed by a database.
    """

    if node.node_id in state.NODES:
        raise HTTPException(status_code=409, detail="Node already registered")

    created = Node(
        **node.dict(),
        created_at=datetime.utcnow(),
        updated_at=datetime.utcnow(),
    )
    state.NODES[node.node_id] = created
    db.insert_node(created)

    # Initialize a status record so the dashboard has something to show.
    state.NODE_STATUS.setdefault(
        node.node_id,
        NodeStatus(
            node_id=node.node_id,
            last_seen=datetime.utcnow(),
            battery_v=0.0,
            rf_power_dbm=0.0,
            temp_c=0.0,
            anomaly_score=0.0,
            gps_quality=None,
        ),
    )

    return created


@router.get("/", response_model=List[NodeStatus])
async def list_nodes() -> List[NodeStatus]:
    """Return the latest status snapshot for all known nodes."""

    return list(state.NODE_STATUS.values())


@router.get("/{node_id}", response_model=Node)
async def get_node(node_id: str) -> Node:
    node = state.NODES.get(node_id)
    if not node:
        raise HTTPException(status_code=404, detail="Node not found")
    return node


@router.get("/{node_id}/status", response_model=NodeStatus)
async def get_node_status(node_id: str) -> NodeStatus:
    status = state.NODE_STATUS.get(node_id)
    if not status:
        raise HTTPException(status_code=404, detail="Node status not found")
    return status
