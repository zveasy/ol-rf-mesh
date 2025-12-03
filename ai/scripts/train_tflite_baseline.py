"""
Train a small classifier on log-magnitude FFT features and export to int8 TFLite/TFLM.

Steps:
- Loads features from data/processed/features.npz (or generates IQ + features).
- Trains a compact dense model.
- Quantizes to int8 with representative dataset.
- Emits .tflite, C header, and contract JSON with tensor/threshold info.
- Benchmarks latency on the host CPU.
"""

import argparse
import json
import pathlib
import time
from typing import Tuple

import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split

from extract_features import load_iq, iq_to_features

PROCESSED = pathlib.Path("data/processed/features.npz")
MODELS = pathlib.Path("models")
MODELS.mkdir(exist_ok=True)


def load_features(fft_size: int, bins: int) -> Tuple[np.ndarray, np.ndarray]:
    if PROCESSED.exists():
        data = np.load(PROCESSED)
        return data["X"], data["y"]

    iq, labels, _, _ = load_iq(fft_size)
    feats = iq_to_features(iq, bins)
    return feats, labels


def build_model(input_dim: int) -> tf.keras.Model:
    model = tf.keras.Sequential(
        [
            tf.keras.layers.Input(shape=(input_dim,)),
            tf.keras.layers.Dense(48, activation="relu"),
            tf.keras.layers.Dense(24, activation="relu"),
            tf.keras.layers.Dense(1, activation="sigmoid"),
        ]
    )
    model.compile(optimizer="adam", loss="binary_crossentropy", metrics=["accuracy"])
    return model


def quantize_int8(model: tf.keras.Model, features: np.ndarray, out_path: pathlib.Path) -> bytes:
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    def rep_ds():
        for i in range(min(200, len(features))):
            yield [np.expand_dims(features[i], 0).astype(np.float32)]

    converter.representative_dataset = rep_ds
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    tflite_model = converter.convert()
    out_path.write_bytes(tflite_model)
    print(f"Wrote quantized TFLite to {out_path} ({len(tflite_model)} bytes)")
    return tflite_model


def emit_header(tflite_bytes: bytes, header_path: pathlib.Path, tensor_arena_bytes: int = 8 * 1024):
    array_name = "g_tflite_rff_model"
    with header_path.open("w") as f:
        f.write("#pragma once\n\n#include <cstddef>\n#include <cstdint>\n\n")
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


def benchmark_tflite(tflite_bytes: bytes, runs: int = 100) -> Tuple[float, dict]:
    interpreter = tf.lite.Interpreter(model_content=tflite_bytes)
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]

    # random input within int8 range
    input_scale, input_zero = input_details["quantization"]
    sample = (np.random.randint(-128, 127, size=input_details["shape"], dtype=np.int8)).astype(np.int8)
    interpreter.set_tensor(input_details["index"], sample)

    # warmup
    for _ in range(5):
        interpreter.invoke()

    t0 = time.perf_counter()
    for _ in range(runs):
        interpreter.invoke()
    latency_ms = ((time.perf_counter() - t0) / runs) * 1000.0

    # collect quant info
    out_scale, out_zero = output_details["quantization"]
    meta = {
        "input_shape": input_details["shape"].tolist(),
        "input_scale": input_scale,
        "input_zero_point": input_zero,
        "output_scale": out_scale,
        "output_zero_point": out_zero,
    }
    return latency_ms, meta


def build_contract(path: pathlib.Path, meta: dict, threshold: float, fft_size: int, bins: int):
    contract = {
        "input_shape": meta["input_shape"],
        "input_dtype": "int8",
        "input_scale": meta["input_scale"],
        "input_zero_point": meta["input_zero_point"],
        "output_scale": meta["output_scale"],
        "output_zero_point": meta["output_zero_point"],
        "fft_size": fft_size,
        "feature_bins": bins,
        "feature": "log-magnitude FFT, normalized per window",
        "threshold": threshold,
        "decision": "output > threshold => anomaly",
    }
    path.write_text(json.dumps(contract, indent=2))
    print(f"Wrote model contract to {path}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fft-size", type=int, default=256)
    parser.add_argument("--bins", type=int, default=128)
    parser.add_argument("--epochs", type=int, default=10)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--output", type=str, default="models/rf_classifier_int8.tflite")
    parser.add_argument("--header", type=str, default="models/rf_classifier_model.h")
    parser.add_argument("--contract", type=str, default="models/model_contract.json")
    args = parser.parse_args()

    X, y = load_features(args.fft_size, args.bins)
    X_train, X_val, y_train, y_val = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)

    model = build_model(X.shape[1])
    model.fit(X_train, y_train, validation_data=(X_val, y_val), epochs=args.epochs, batch_size=args.batch_size, verbose=0)
    val_loss, val_acc = model.evaluate(X_val, y_val, verbose=0)
    print(f"Validation acc: {val_acc:.4f}")

    # Determine a simple threshold at 95th percentile of normal scores
    scores = model.predict(X, verbose=0).squeeze()
    threshold = float(np.percentile(scores[y == 0], 95)) if np.any(y == 0) else 0.5
    print(f"Suggested anomaly threshold: {threshold:.3f}")

    tflite_path = pathlib.Path(args.output)
    tflite_bytes = quantize_int8(model, X, tflite_path)
    header_path = pathlib.Path(args.header)
    emit_header(tflite_bytes, header_path)

    latency_ms, meta = benchmark_tflite(tflite_bytes)
    print(f"TFLite int8 latency: {latency_ms:.3f} ms (avg over 100 runs)")

    contract_path = pathlib.Path(args.contract)
    build_contract(contract_path, meta, threshold, args.fft_size, args.bins)


if __name__ == "__main__":
    main()
