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
