#!/usr/bin/env bash
set -euo pipefail

# Build and run the Dockerized TFLite pipeline (IQ -> features -> quantized model)
# Outputs land in ./models and ./data on the host.

IMAGE_NAME="${IMAGE_NAME:-ol-rf-mesh-tflite}"
PLATFORM="${PLATFORM:-linux/amd64}"

cd "$(dirname "$0")"

echo "[+] Building ${IMAGE_NAME} for ${PLATFORM}..."
docker build --platform "${PLATFORM}" -f Dockerfile.tflite -t "${IMAGE_NAME}" .

echo "[+] Running pipeline in container..."
docker run --rm \
  --platform "${PLATFORM}" \
  -v "$(pwd)/models:/workspace/ai/models" \
  -v "$(pwd)/data:/workspace/ai/data" \
  "${IMAGE_NAME}"

echo "[+] Done. Check ai/models/ for updated .tflite, header, and contract."
