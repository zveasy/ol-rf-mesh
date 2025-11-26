from typing import List

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

router = APIRouter()


class ConnectionManager:
    def __init__(self) -> None:
        self.active: List[WebSocket] = []

    async def connect(self, websocket: WebSocket) -> None:
        await websocket.accept()
        self.active.append(websocket)

    def disconnect(self, websocket: WebSocket) -> None:
        if websocket in self.active:
            self.active.remove(websocket)

    async def broadcast(self, message: dict) -> None:
        for connection in list(self.active):
            try:
                await connection.send_json(message)
            except WebSocketDisconnect:
                self.disconnect(connection)


manager = ConnectionManager()


@router.websocket("/ws/rf-stream")
async def rf_stream(websocket: WebSocket) -> None:
    """Simple RF event stream for dashboards.

    For now, this keeps the connection open and relies on other code paths
    to call manager.broadcast(...) whenever new RF events are ingested.
    """

    await manager.connect(websocket)
    try:
        while True:
            # Optionally receive simple pings/filters from the client.
            await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)
