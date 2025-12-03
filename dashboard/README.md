React dashboard for O&L RF Mesh Node fleet and RF activity.

## Quick start

```bash
cd dashboard
npm install
npm run dev -- --host

# Production build
npm run build
```

Set `VITE_BACKEND_URL` (default: `http://localhost:8000`). Toggle `VITE_USE_MOCK=true` to force mock data (nodes/events/topology/FFT spectra) without hitting the backend. The UI also has a “Use mock data” switch.

## MVP views

- Fleet status table (nodes, battery, RF power, temp, anomaly, GPS fix)
- Live RF event strip (WebSocket `/ws/rf-stream`)
- Alerts + triage filters, topology snapshot, and mocked FFT waterfall tiles
- Stat tiles (active nodes, anomalies, avg battery, alert count)
