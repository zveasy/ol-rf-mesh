"""
Feature extraction for IQ data.

Input:
- data/iq/iq_samples.npz (see generate_synthetic_iq.py) or synthetic fallback

Output:
- data/processed/features.npz with X (log-mag FFT) and y labels + band_ids
"""

import argparse
import pathlib
from typing import Tuple

import numpy as np

from generate_synthetic_iq import generate_dataset

PROCESSED = pathlib.Path("data/processed")
PROCESSED.mkdir(parents=True, exist_ok=True)
IQ_PATH = pathlib.Path("data/iq/iq_samples.npz")


def load_iq(fallback_fft: int) -> Tuple[np.ndarray, np.ndarray, np.ndarray, int]:
    if IQ_PATH.exists():
        data = np.load(IQ_PATH)
        fft_size = int(data.get("fft_size", fallback_fft))
        iq = data["iq_real"] + 1j * data["iq_imag"]
        labels = data["labels"]
        band_ids = data["band_ids"]
        return iq, labels, band_ids, fft_size

    # fallback: generate quickly
    bands = [915e6, 2.4e9]
    iq, labels, band_ids = generate_dataset(30, fallback_fft, bands)
    return iq, labels, np.array(band_ids), fallback_fft


def iq_to_features(iq: np.ndarray, target_bins: int) -> np.ndarray:
    fft = np.fft.fft(iq, axis=1)
    mag = np.abs(fft[:, : target_bins]) + 1e-6
    logmag = np.log1p(mag)
    # Normalize per-sample
    logmag = logmag / (np.max(logmag, axis=1, keepdims=True) + 1e-6)
    return logmag.astype(np.float32)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fft-size", type=int, default=256)
    parser.add_argument("--bins", type=int, default=128)
    parser.add_argument("--output", type=str, default=str(PROCESSED / "features.npz"))
    args = parser.parse_args()

    iq, labels, band_ids, fft_size = load_iq(args.fft_size)
    features = iq_to_features(iq, args.bins)

    out_path = pathlib.Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(out_path, X=features, y=labels, band_ids=band_ids, fft_size=fft_size, bins=args.bins)
    print(f"Wrote features to {out_path} (X shape={features.shape}, labels={len(labels)})")


if __name__ == "__main__":
    main()
