"""
Generate synthetic IQ samples for multiple bands with optional anomalies.

Outputs:
- data/iq/iq_samples.npz with arrays: iq_real, iq_imag, labels, band_ids
"""

import argparse
import pathlib
from typing import List, Tuple

import numpy as np

OUT_DIR = pathlib.Path("data/iq")
OUT_DIR.mkdir(parents=True, exist_ok=True)


def synth_iq_sample(n: int, band_hz: float, anomaly: bool = False) -> np.ndarray:
    t = np.arange(n) / n
    # Base noise floor
    iq = (0.05 * np.random.randn(n) + 0.05j * np.random.randn(n)).astype(np.complex64)

    # Add a carrier tone in-band
    tone_freq = np.random.uniform(-0.2, 0.2)  # normalized
    tone = np.exp(2j * np.pi * tone_freq * t)
    iq += 0.2 * tone

    if anomaly:
        # Add a bursty interferer/chirp
        chirp = np.exp(2j * np.pi * (tone_freq + 0.3 * t) * t)
        window = np.hanning(n)
        iq += 0.5 * chirp * window

    # Slight band-dependent tilt
    iq *= 1.0 + 0.05 * np.sin(2 * np.pi * t * (band_hz % 5))
    return iq


def generate_dataset(samples: int, n_fft: int, bands: List[float]) -> Tuple[np.ndarray, np.ndarray, List[str]]:
    iq_samples = []
    labels = []
    band_ids = []
    for band in bands:
        band_name = f"{int(band/1e6)}mhz"
        for _ in range(samples):
            iq = synth_iq_sample(n_fft, band, anomaly=False)
            iq_samples.append(iq)
            labels.append(0)
            band_ids.append(band_name)
        for _ in range(samples // 2):
            iq = synth_iq_sample(n_fft, band, anomaly=True)
            iq_samples.append(iq)
            labels.append(1)
            band_ids.append(band_name)
    return np.stack(iq_samples), np.array(labels, dtype=np.int8), band_ids


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", type=int, default=50, help="Normal samples per band; anomalies are half this count")
    parser.add_argument("--fft-size", type=int, default=256)
    parser.add_argument("--bands", type=float, nargs="+", default=[915e6, 2.4e9, 5.8e9])
    parser.add_argument("--output", type=str, default=str(OUT_DIR / "iq_samples.npz"))
    args = parser.parse_args()

    iq, labels, band_ids = generate_dataset(args.samples, args.fft_size, args.bands)
    out_path = pathlib.Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        out_path,
        iq_real=iq.real.astype(np.float32),
        iq_imag=iq.imag.astype(np.float32),
        labels=labels,
        band_ids=np.array(band_ids),
        fft_size=args.fft_size,
    )
    print(f"Wrote synthetic IQ dataset to {out_path} (samples={len(labels)}, fft_size={args.fft_size})")


if __name__ == "__main__":
    main()
