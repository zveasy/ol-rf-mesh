from fastapi import FastAPI
from .api import telemetry, nodes
from fastapi.middleware.cors import CORSMiddleware
from .config import get_database_url

app = FastAPI(title="O&L RF Mesh Backend")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(telemetry.router, prefix="/telemetry", tags=["telemetry"])
app.include_router(nodes.router, prefix="/nodes", tags=["nodes"])

app.state.database_url = get_database_url()


@app.get("/health")
def health():
    return {"status": "ok"}
