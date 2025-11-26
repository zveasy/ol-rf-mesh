import struct
import requests
from typing import Dict
from datetime import datetime

# Struct layouts
HEADER_FMT = "<BBBBI16s16s"  # version, msg_type, ttl, hop_count, seq, src_id, dest_id
TELEMETRY_FMT = "<Iffff"    # timestamp_ms, rf_power_dbm, battery_v, temp_c, anomaly_score
RF_EVENT_FMT = "<IIfffff8s"  # timestamp_ms, center_freq_hz, bin_width_hz, anomaly_score, feat0-3, band_id(8 bytes)
GPS_FMT = "<IBfffBff"        # timestamp_ms, num_sats, snr_avg, hdop, valid_fix, jamming, spoof

HEADER_SIZE = struct.calcsize(HEADER_FMT)
TELEMETRY_SIZE = struct.calcsize(TELEMETRY_FMT)
RF_EVENT_SIZE = struct.calcsize(RF_EVENT_FMT)
GPS_SIZE = struct.calcsize(GPS_FMT)
FRAME_SIZE = HEADER_SIZE + TELEMETRY_SIZE


def decode_legacy_frame(buf: bytes) -> Dict:
    if len(buf) < FRAME_SIZE:
        raise ValueError("buffer too small")
    header = struct.unpack_from(HEADER_FMT, buf, 0)
    telemetry = struct.unpack_from(TELEMETRY_FMT, buf, HEADER_SIZE)

    version, msg_type, ttl, hop, seq_no, src_id_raw, dest_id_raw = header
    src_id = src_id_raw.split(b"\x00", 1)[0].decode(errors="ignore")
    dest_id = dest_id_raw.split(b"\x00", 1)[0].decode(errors="ignore")

    timestamp_ms, rf_power_dbm, battery_v, temp_c, anomaly_score = telemetry

    return {
        "header": {
            "version": version,
            "msg_type": msg_type,
            "ttl": ttl,
            "hop_count": hop,
            "seq_no": seq_no,
            "src_id": src_id,
            "dest_id": dest_id,
        },
        "telemetry": {
            "node_id": src_id,
            "timestamp": timestamp_ms / 1000.0,
            "rf_power_dbm": rf_power_dbm,
            "battery_v": battery_v,
            "temp_c": temp_c,
            "anomaly_score": anomaly_score,
        },
    }


def post_telemetry(backend: str, payload: Dict) -> None:
    resp = requests.post(f"{backend}/telemetry/", json=payload)
    resp.raise_for_status()


def decode_rf_event(buf: bytes) -> Dict:
    if len(buf) < HEADER_SIZE + RF_EVENT_SIZE:
        raise ValueError("buffer too small")
    version, msg_type, ttl, hop, seq_no, src_id_raw, dest_id_raw = struct.unpack_from(HEADER_FMT, buf, 0)
    payload = struct.unpack_from(RF_EVENT_FMT, buf, HEADER_SIZE)
    ts_ms, center_freq_hz, bin_width_hz, anomaly_score, f0, f1, f2, f3, band_raw = payload
    src_id = src_id_raw.split(b"\x00", 1)[0].decode(errors="ignore")
    band_id = band_raw.split(b"\x00", 1)[0].decode(errors="ignore")
    return {
        "header": {"version": version, "msg_type": msg_type, "ttl": ttl, "hop_count": hop, "seq_no": seq_no, "src_id": src_id},
        "rf_event": {
            "node_id": src_id,
            "timestamp": datetime.utcfromtimestamp(ts_ms / 1000.0).isoformat(),
            "band_id": band_id or "unknown",
            "center_freq_hz": float(center_freq_hz),
            "bin_width_hz": float(bin_width_hz),
            "anomaly_score": float(anomaly_score),
            "features": [f0, f1, f2, f3],
        },
    }


def post_rf_event(backend: str, payload: Dict) -> None:
    resp = requests.post(f"{backend}/telemetry/rf-events", json=[payload])
    resp.raise_for_status()


def decode_gps(buf: bytes) -> Dict:
    if len(buf) < HEADER_SIZE + GPS_SIZE:
        raise ValueError("buffer too small")
    version, msg_type, ttl, hop, seq_no, src_id_raw, _ = struct.unpack_from(HEADER_FMT, buf, 0)
    payload = struct.unpack_from(GPS_FMT, buf, HEADER_SIZE)
    ts_ms, num_sats, snr_avg, hdop, valid_fix, jam, spoof = payload
    src_id = src_id_raw.split(b"\x00", 1)[0].decode(errors="ignore")
    return {
        "header": {"version": version, "msg_type": msg_type, "ttl": ttl, "hop_count": hop, "seq_no": seq_no, "src_id": src_id},
        "gps": {
            "node_id": src_id,
            "timestamp": datetime.utcfromtimestamp(ts_ms / 1000.0).isoformat(),
            "num_sats": int(num_sats),
            "snr_avg": float(snr_avg),
            "hdop": float(hdop),
            "valid_fix": bool(valid_fix),
            "jamming_indicator": float(jam),
            "spoof_indicator": float(spoof),
        },
    }


def post_gps(backend: str, payload: Dict) -> None:
    resp = requests.post(f"{backend}/telemetry/gps", json=[payload])
    resp.raise_for_status()


if __name__ == "__main__":
    import argparse
    from pathlib import Path

    parser = argparse.ArgumentParser()
    parser.add_argument("frame_file", type=Path, help="binary frame file")
    parser.add_argument("--backend", default="http://localhost:8000", help="FastAPI backend URL")
    args = parser.parse_args()

    buf = args.frame_file.read_bytes()
    msg_type = buf[1] if len(buf) > 1 else 0

    if msg_type == 1:
        decoded = decode_legacy_frame(buf)
        post_telemetry(args.backend, decoded["telemetry"])
        print("Posted telemetry:", decoded["telemetry"])
    elif msg_type == 2:
        decoded = decode_rf_event(buf)
        post_rf_event(args.backend, decoded["rf_event"])
        print("Posted rf_event:", decoded["rf_event"])
    elif msg_type == 3:
        decoded = decode_gps(buf)
        post_gps(args.backend, decoded["gps"])
        print("Posted gps:", decoded["gps"])
    else:
        raise SystemExit(f"Unsupported msg_type {msg_type}")
