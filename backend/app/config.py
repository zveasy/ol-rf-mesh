import os

# Default keeps local development simple (SQLite file). Override for Postgres/Timescale.
DEFAULT_DATABASE_URL = "sqlite:///backend.db"


def get_database_url() -> str:
    return os.environ.get("DATABASE_URL", DEFAULT_DATABASE_URL)
