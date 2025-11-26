FROM python:3.13-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

WORKDIR /app

COPY pyproject.toml ./
RUN pip install --no-cache-dir numpy scikit-learn joblib

COPY scripts ./scripts
COPY data ./data

VOLUME ["/app/data", "/app/models"]

CMD ["sh", "-c", "python scripts/generate_synthetic_data.py && python scripts/train_baseline_model.py"]
