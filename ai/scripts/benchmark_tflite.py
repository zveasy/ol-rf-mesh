"""
Benchmark a TFLite int8 model produced by the pipeline.

Reads a contract (JSON) for input shape/scale/zero-point and runs N inferences
with random inputs or a provided feature .npy/.npz to measure latency.
Also reports the embedded tensor arena hint if present in the generated header.
"""

import argparse
import json
import os
import time
from pathlib import Path

import numpy as np
import tensorflow as tf


def load_features(path: Path, input_shape):
    if not path:
        return None
    if path.suffix == ".npz":
        data = np.load(path)
        # Prefer key "features" if present
        if "features" in data:
            arr = data["features"]
        else:
            # Fallback: first array
            arr = data[list(data.keys())[0]]
    else:
        arr = np.load(path)
    arr = arr.astype(np.float32)
    # Flatten to match expected shape
    arr = arr.reshape((-1,) + tuple(input_shape[1:]))
    return arr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True, type=Path, help="Path to int8 TFLite model")
    ap.add_argument("--contract", required=True, type=Path, help="Path to model_contract.json")
    ap.add_argument("--features", type=Path, help="Optional .npy/.npz with features to benchmark")
    ap.add_argument("--runs", type=int, default=50, help="Number of timed inferences")
    args = ap.parse_args()

    with args.contract.open() as f:
        contract = json.load(f)
    input_shape = contract.get("input_shape", [1, 128])
    scale = contract.get("input_scale", 1.0)
    zero_point = contract.get("input_zero_point", 0)
    arena_hint = contract.get("tensor_arena_size", None)

    interpreter = tf.lite.Interpreter(model_path=str(args.model))
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]

    # Prepare inputs
    feats = load_features(args.features, input_shape)
    if feats is None:
        # Random features in expected range
        feats = np.random.normal(loc=0.0, scale=1.0, size=tuple([args.runs] + input_shape[1:])).astype(np.float32)
    else:
        # Repeat if not enough samples
        if feats.shape[0] < args.runs:
            reps = int(np.ceil(args.runs / feats.shape[0]))
            feats = np.tile(feats, (reps, 1))[: args.runs]

    # Quantize to int8
    feats_q = np.clip(np.round(feats / scale) + zero_point, -128, 127).astype(np.int8)

    times = []
    for i in range(args.runs):
        interpreter.set_tensor(input_details["index"], feats_q[i : i + 1])
        t0 = time.perf_counter()
        interpreter.invoke()
        t1 = time.perf_counter()
        times.append((t1 - t0) * 1000.0)
        _ = interpreter.get_tensor(output_details["index"])

    print(f"Model: {args.model} | Contract: {args.contract}")
    print(f"Input shape: {input_shape} scale={scale} zp={zero_point} arena_hint={arena_hint}")
    print(f"Latency ms: p50={np.percentile(times,50):.3f} p90={np.percentile(times,90):.3f} max={np.max(times):.3f}")
    print(f"Output dtype={output_details['dtype']} shape={output_details['shape']}")


if __name__ == "__main__":
    main()
