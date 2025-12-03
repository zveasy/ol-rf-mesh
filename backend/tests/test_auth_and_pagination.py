from fastapi.testclient import TestClient

import app.auth as auth
from app.main import app


def test_alerts_require_token(monkeypatch):
    monkeypatch.setattr(auth, "API_TOKEN", "secret-token")
    client = TestClient(app)

    resp = client.get("/alerts/")
    assert resp.status_code == 401

    ok = client.get("/alerts/", headers={"Authorization": "Bearer secret-token"})
    assert ok.status_code == 200
    assert isinstance(ok.json(), list)


def test_history_pagination_with_token(monkeypatch):
    monkeypatch.setattr(auth, "API_TOKEN", "secret-token")
    client = TestClient(app)

    resp = client.get("/history/rf-events?limit=5&offset=0", headers={"Authorization": "Bearer secret-token"})
    assert resp.status_code == 200
    data = resp.json()
    assert "items" in data and "total" in data and data["limit"] == 5 and data["offset"] == 0
