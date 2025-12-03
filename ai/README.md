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

# 1) Generate synthetic IQ (multi-band, anomalies)
python scripts/generate_synthetic_iq.py --samples 50 --fft-size 256

# 2) Extract log-magnitude FFT features
python scripts/extract_features.py --fft-size 256 --bins 128

# 3) Train + quantize baseline classifier, emit TFLite/TFLM + contract
python scripts/train_tflite_baseline.py --epochs 5 --output models/rf_classifier_int8.tflite --header models/rf_classifier_model.h --contract models/model_contract.json
```

Artifacts:
- `data/iq/iq_samples.npz`: synthetic IQ with labels/bands.
- `data/processed/features.npz`: log-magnitude FFT features + labels.
- `models/rf_classifier_int8.tflite`: int8 quantized classifier.
- `models/rf_classifier_model.h`: C array + tensor arena hint.
- `models/model_contract.json`: on-device API contract (input shape, quantization, threshold).

Benchmark: `train_tflite_baseline.py` reports avg int8 inference latency on host CPU.

### Containerized TFLite build (CPU)

If local TensorFlow wheels are unavailable, build inside Docker:

```bash
cd ai
docker build -f Dockerfile.tflite -t ol-rf-mesh-tflite .
docker run --rm -v $(pwd)/models:/workspace/ai/models -v $(pwd)/data:/workspace/ai/data ol-rf-mesh-tflite
```

This runs: generate_synthetic_iq → extract_features → train_tflite_baseline (int8) and outputs `.tflite`, header, and contract under `models/`.
