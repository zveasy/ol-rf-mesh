# Mesh Packet Schema (CBOR, schema v1)

Encoding: CBOR, deterministic (sorted numeric keys), little-endian floats, AES-GCM envelope with `nonce || auth_tag || ciphertext`. Ciphertext is the CBOR body below.

Top-level map keys:
- `1` header map: `1 ver`, `2 msg_type`, `3 ttl`, `4 hop_count`, `5 seq_no`, `6 src_id (tstr)`, `7 dest_id (tstr)`.
- `2` security map: `1 encrypted (bool)`, `2 nonce (bstr, 12B)`, `3 auth_tag (bstr, 16B)`.
- `3` counters map: `1 tx_counter`, `2 replay_window`.
- `4` RF map: `1 ts_ms`, `2 center_hz`, `3 avg_dbm (float32)`, `4 peak_dbm (float32)`, `5 anomaly (float32)`, `6 model_version`.
- `5` GPS map: `1 ts_ms`, `2 lat_deg (f32)`, `3 lon_deg (f32)`, `4 alt_m (f32)`, `5 sats`, `6 hdop (f32)`, `7 valid_fix`, `8 jam`, `9 spoof`, `10 cn0_avg (f32)`.
- `6` health map: `1 ts_ms`, `2 batt_v (f32)`, `3 temp_c (f32)`, `4 imu_tilt_deg (f32)`, `5 tamper`.
- `7` routing map: `1 epoch_ms`, `2 version`, `3 entries (array of maps: 1 neighbor_id, 2 rssi_dbm, 3 link_quality, 4 cost)`, `4 entry_count`.
- `8` fault map: `1 fault_active`, `2 wdt_resets`, `3 ota_failures`, `4 tamper_events`.
- `9` ota map: `1 state`, `2 current_offset`, `3 total_size`, `4 signature_valid`.

Golden vector:
- `firmware/tests/test_mesh_golden.cpp` locks a deterministic frame to hex: `000102030405060708090A0B6DA4D65112F7DF92000000000000000055E2D0A48D5413303DB48015BEEE11C036A41D3957D4052B1B9A5CD6B4468F004A3095E2640BBB8026216CB9731F14FACBD4526453F6BECB04BC81E443BCBED25E1C7E8BF1C1FB13BB9AB8D2A1C9E5CC7DC71014F8024A043D00567D18AB88349DB9EF5F40888955289F5A1AB00AB6968E635D091FDEBCEE55EBBAA4F4823C5BB015B070C5948B916391374C9FDFDC414DC18F508E00862759D320E9CFA69C9F672D5A9B8C16D4052AC7EBF1131134EFE460BAC04381BE60A378722F17D33E7E5DBBAFC1159DF80B9F3C018432F0A1DCB824D952F3976558B6F6828652470614EED07B3E`.

Notes:
- Frame size cap remains `kMaxMeshFrameLen` (256 B before AES-GCM overhead). Transport drops if estimated fragments exceed 3×200B.
- Replay guard: per-node monotonic seq_no; counters map includes `tx_counter` + `replay_window` for future sliding-window validation.
