from typing import List

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from .. import auth

router = APIRouter()


class ConnectionManager:
    def __init__(self) -> None:
        self.active: List[WebSocket] = []

    def connect(self, websocket: WebSocket) -> None:
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

    await websocket.accept()
    token_required = bool(auth.API_TOKEN)
    if token_required:
        token = None
        auth_header = websocket.headers.get("authorization", "")
        if auth_header.lower().startswith("bearer "):
            token = auth_header.split(" ", 1)[1]
        elif "token" in websocket.query_params:
            token = websocket.query_params.get("token")

        if token != auth.API_TOKEN:
            await websocket.close(code=1008)
            return

    manager.connect(websocket)
    try:
        while True:
            # Optionally receive simple pings/filters from the client.
            await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)
