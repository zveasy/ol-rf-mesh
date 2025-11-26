React dashboard for O&L RF Mesh Node fleet and RF activity.

## Quick start

```bash
cd dashboard
npm install
npm run dev -- --host
```

## MVP views

- Fleet status table (nodes, battery, RF power, temp, anomaly, GPS fix)
- Live RF event strip (WebSocket `/ws/rf-stream`)
- Alerts list (`/alerts/` REST + WS pushes)
- Stat tiles (active nodes, anomalies, avg battery, alert count)
