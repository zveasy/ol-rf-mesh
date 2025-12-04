from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

import app.auth as auth
from app.main import app


def test_websocket_rejects_without_token(monkeypatch):
    monkeypatch.setattr(auth, "API_TOKEN", "secret-token")
    client = TestClient(app)

    try:
        with client.websocket_connect("/ws/rf-stream") as websocket:
            websocket.receive_text()
    except WebSocketDisconnect as exc:
        assert exc.code == 1008
    else:
        raise AssertionError("WebSocket should have rejected missing token")


def test_websocket_accepts_with_token(monkeypatch):
    monkeypatch.setattr(auth, "API_TOKEN", "secret-token")
    client = TestClient(app)

    with client.websocket_connect("/ws/rf-stream?token=secret-token") as websocket:
        websocket.send_text("ping")
        # Connection stays open; server echoes nothing so just close.
        websocket.close()
