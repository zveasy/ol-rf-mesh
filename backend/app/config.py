import os

# Default targets the compose db service; falls back to localhost for direct runs.
DEFAULT_DATABASE_URL = "postgresql://mesh:mesh@db:5432/mesh"


def get_database_url() -> str:
    return os.environ.get("DATABASE_URL", DEFAULT_DATABASE_URL)
