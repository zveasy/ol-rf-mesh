AI / DSP experiments for O&L RF Mesh Node (FFT, anomaly detection, TinyML prep).

## Milestone 6: TinyML pipeline

- `scripts/train_tflm_autoencoder.py`: trains a small dense autoencoder on spectral magnitudes, exports to TFLite, and emits a C header for TFLM integration.
- Usage:
  ```bash
  cd ai
  python -m venv .venv && source .venv/bin/activate
  pip install -e .
  python scripts/train_tflm_autoencoder.py --epochs 5 --output models/tiny_autoencoder.tflite --header models/tflm_model.h
  ```
- Input: uses `data/raw/*.npy` if present (see `scripts/generate_synthetic_data.py`), otherwise generates synthetic signals.
- Output: `.tflite` file + `models/tflm_model.h` C array with `kTensorArenaSize` hint (4KB default).

## New offline pipeline (IQ → FFT → TFLite int8)

```bash
cd ai
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt  # tensorflow-cpu + numpy/sklearn

# 1) Generate IQ (use --samples for synthetic, or drop a real capture at data/iq/iq_samples.npz)
  python scripts/generate_synthetic_iq.py --samples 50 --fft-size 256

# 2) Extract log-magnitude FFT features (or point to your real IQ npz)
python scripts/extract_features.py --fft-size 256 --bins 128 --iq-path data/iq/iq_samples.npz

# 3) Train + quantize baseline classifier, emit TFLite/TFLM + contract
python scripts/train_tflite_baseline.py \
  --epochs 5 \
  --output models/rf_classifier_int8.tflite \
  --header models/rf_classifier_model.h \
  --contract models/model_contract.json \
  --features-path data/processed/features.npz \
  --dataset-version real-rf-v1

Notes:
- On Apple Silicon, `pip install -r requirements.txt` will pull `tensorflow-macos` (2.14.x); optionally add `pip install tensorflow-metal` for GPU acceleration.
```

Artifacts:
- `data/iq/iq_samples.npz`: synthetic IQ with labels/bands.
- `data/processed/features.npz`: log-magnitude FFT features + labels.
- `models/rf_classifier_int8.tflite`: int8 quantized classifier.
- `models/rf_classifier_model.h`: C array + tensor arena hint.
- `models/model_contract.json`: on-device API contract (input shape, quantization, threshold, feature stats + dataset version).
- `scripts/benchmark_tflite.py`: measures latency for a given `.tflite` + contract; accepts `--features` to benchmark on real captures.

Benchmark: `train_tflite_baseline.py` reports avg int8 inference latency on host CPU.

### Containerized TFLite build (CPU)

If local TensorFlow wheels are unavailable, build inside Docker:

```bash
cd ai
docker build -f Dockerfile.tflite -t ol-rf-mesh-tflite .
docker run --rm -v $(pwd)/models:/workspace/ai/models -v $(pwd)/data:/workspace/ai/data ol-rf-mesh-tflite
```

This runs: generate_synthetic_iq → extract_features → train_tflite_baseline (int8) and outputs `.tflite`, header, and contract under `models/`.

### Running on real captures
- Drop real IQ NPZ at `data/iq/real_capture.npz` (shape `[n, 2, fft_size]`, I/Q) or precomputed features at `data/processed/features.npz` with keys `X` (features) and `y` (labels).
- Train and export:
  ```
  cd ai && source .venv/bin/activate
  python scripts/train_tflite_baseline.py \
    --iq-path data/iq/real_capture.npz \
    --features-path data/processed/features.npz \
    --dataset-version real-rf-v1 \
    --output models/rf_classifier_int8.tflite \
    --header models/rf_classifier_model.h \
    --contract models/model_contract.json
  ```
- Benchmark latency on host CPU (int8):
  ```
  python scripts/benchmark_tflite.py \
    --model models/rf_classifier_int8.tflite \
    --contract models/model_contract.json \
    --features data/processed/features.npz
  ```
