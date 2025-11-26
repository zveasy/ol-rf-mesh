import numpy as np
import pathlib
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report
import joblib

RAW = pathlib.Path("data/raw")
MODEL_OUT = pathlib.Path("models")
MODEL_OUT.mkdir(exist_ok=True)


def load_dataset():
    X, y = [], []
    for f in RAW.glob("normal_*.npy"):
        sig = np.load(f)
        X.append(np.abs(np.fft.rfft(sig)))
        y.append(0)
    for f in RAW.glob("anomaly_*.npy"):
        sig = np.load(f)
        X.append(np.abs(np.fft.rfft(sig)))
        y.append(1)
    return np.stack(X), np.array(y)


if __name__ == "__main__":
    X, y = load_dataset()
    clf = RandomForestClassifier(n_estimators=100)
    clf.fit(X, y)
    print(classification_report(y, clf.predict(X)))
    joblib.dump(clf, MODEL_OUT / "rf_baseline.joblib")
