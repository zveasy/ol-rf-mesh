import numpy as np
import pathlib

OUT = pathlib.Path("data/raw")
OUT.mkdir(parents=True, exist_ok=True)


def generate_signal(n: int = 1024, anomaly: bool = False) -> np.ndarray:
    t = np.linspace(0, 1, n)
    base = 0.1 * np.random.randn(n)
    if anomaly:
        base += np.sin(2 * np.pi * 50 * t)
    return base.astype(np.float32)


if __name__ == "__main__":
    for i in range(100):
        sig = generate_signal(anomaly=False)
        np.save(OUT / f"normal_{i}.npy", sig)
    for i in range(100):
        sig = generate_signal(anomaly=True)
        np.save(OUT / f"anomaly_{i}.npy", sig)
