"""
Milestone 6: Tiny autoencoder training + TFLite Micro export.

This script:
- Generates simple spectral feature windows (or uses synthetic signals)
- Trains a tiny dense autoencoder (Keras)
- Exports to TFLite
- Emits a C header with a const uint8_t array for firmware
"""

import argparse
import pathlib
import numpy as np
import tensorflow as tf

RAW = pathlib.Path("data/raw")
OUT = pathlib.Path("models")
OUT.mkdir(exist_ok=True)


def load_or_generate(num_samples: int = 200, n: int = 256):
    X = []
    if RAW.exists():
        for f in sorted(RAW.glob("*.npy")):
            X.append(np.load(f))
    if not X:
        t = np.linspace(0, 1, n)
        for i in range(num_samples):
            base = 0.1 * np.random.randn(n)
            if i % 5 == 0:
                base += np.sin(2 * np.pi * 50 * t)
            X.append(base.astype(np.float32))
    return np.stack(X, axis=0)


def to_features(waveforms: np.ndarray) -> np.ndarray:
    fft = np.fft.rfft(waveforms, axis=1)
    mag = np.abs(fft)
    mag = mag / (np.max(mag, axis=1, keepdims=True) + 1e-6)
    return mag.astype(np.float32)


def build_autoencoder(input_dim: int):
    inputs = tf.keras.Input(shape=(input_dim,))
    x = tf.keras.layers.Dense(64, activation="relu")(inputs)
    x = tf.keras.layers.Dense(24, activation="relu")(x)
    bottleneck = tf.keras.layers.Dense(8, activation="relu", name="bottleneck")(x)
    x = tf.keras.layers.Dense(24, activation="relu")(bottleneck)
    x = tf.keras.layers.Dense(64, activation="relu")(x)
    outputs = tf.keras.layers.Dense(input_dim, activation="linear")(x)
    model = tf.keras.Model(inputs, outputs)
    model.compile(optimizer="adam", loss="mse")
    return model


def representative_dataset(features: np.ndarray):
    for i in range(min(len(features), 100)):
        yield [np.expand_dims(features[i], 0).astype(np.float32)]


def export_tflite(model: tf.keras.Model, features: np.ndarray, out_path: pathlib.Path):
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = lambda: representative_dataset(features)
    tflite_model = converter.convert()
    out_path.write_bytes(tflite_model)
    print(f"Wrote TFLite model to {out_path} ({len(tflite_model)} bytes)")
    return tflite_model


def emit_c_array(tflite_bytes: bytes, header_path: pathlib.Path, tensor_arena_bytes: int = 4 * 1024):
    array_name = "g_tflite_model"
    with header_path.open("w") as f:
        f.write("#pragma once\n\n")
        f.write("#include <cstddef>\n#include <cstdint>\n\n")
        f.write(f"constexpr std::size_t kTfliteModelLen = {len(tflite_bytes)};\n")
        f.write(f"constexpr std::size_t kTensorArenaSize = {tensor_arena_bytes};\n")
        f.write(f"alignas(16) const unsigned char {array_name}[kTfliteModelLen] = {{\n")
        for i, b in enumerate(tflite_bytes):
            if i % 12 == 0:
                f.write("  ")
            f.write(f"0x{b:02x}, ")
            if i % 12 == 11:
                f.write("\n")
        f.write("\n};\n")
    print(f"Wrote C header to {header_path}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--output", type=str, default="models/tiny_autoencoder.tflite")
    parser.add_argument("--header", type=str, default="models/tflm_model.h")
    args = parser.parse_args()

    waveforms = load_or_generate()
    features = to_features(waveforms)

    model = build_autoencoder(features.shape[1])
    model.fit(features, features, epochs=args.epochs, batch_size=args.batch_size, verbose=0)
    loss = model.evaluate(features, features, verbose=0)
    print(f"Training complete. Recon loss: {loss:.6f}")

    tflite_bytes = export_tflite(model, features, pathlib.Path(args.output))
    emit_c_array(tflite_bytes, pathlib.Path(args.header))


if __name__ == "__main__":
    main()
