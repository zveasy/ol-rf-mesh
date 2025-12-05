#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${IDF_PATH:-}" ]]; then
  echo "IDF_PATH is not set; run 'source ~/esp-idf/export.sh' first." >&2
  exit 1
fi

# Override via env: ESPPORT=/dev/cu.usbserial-XXXX IDF_TARGET=esp32s3 ESPBAUD=921600
ESPPORT="${ESPPORT:-/dev/cu.usbserial-0001}"
IDF_TARGET="${IDF_TARGET:-esp32}"
ESPBAUD="${ESPBAUD:-921600}"

pushd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" >/dev/null

idf.py -B build \
  -DIDF_TARGET="${IDF_TARGET}" \
  -DENABLE_HW_WDT=ON \
  -DWDT_INCLUDE_HINT="${IDF_PATH}/components/esp_hw_support/include" \
  -DOL_FREERTOS=ON \
  -p "${ESPPORT}" \
  -b "${ESPBAUD}" \
  flash monitor

popd >/dev/null
