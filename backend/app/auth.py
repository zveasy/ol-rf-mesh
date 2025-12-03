import os
from fastapi import Header, HTTPException, status

API_TOKEN = os.getenv("API_TOKEN")


async def require_token(authorization: str = Header(default="")):
    """Simple bearer token guard. Set API_TOKEN env var to enable."""
    if not API_TOKEN:
        return
    if not authorization.lower().startswith("bearer "):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="missing bearer token")
    token = authorization.split(" ", 1)[1]
    if token != API_TOKEN:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="invalid token")
