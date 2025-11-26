import { useEffect, useMemo, useState } from "react";
import { Alert, getAlerts, getNodes, NodeStatus, openTelemetryStream, RfEvent } from "./api/client";

function App() {
  const [nodes, setNodes] = useState<NodeStatus[]>([]);
  const [alerts, setAlerts] = useState<Alert[]>([]);
  const [recentEvents, setRecentEvents] = useState<RfEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    (async () => {
      try {
        const [nodeData, alertData] = await Promise.all([getNodes(), getAlerts()]);
        setNodes(nodeData);
        setAlerts(alertData);
      } catch (e: any) {
        setError(e.message ?? "Unknown error");
      } finally {
        setLoading(false);
      }
    })();
  }, []);

  useEffect(() => {
    const unsub = openTelemetryStream((msg) => {
      if (!msg?.type) return;
      if (msg.type === "rf_event" || msg.type === "telemetry") {
        setRecentEvents((prev) => {
          const next = [
            {
              node_id: msg.node_id,
              timestamp: msg.timestamp || new Date().toISOString(),
              anomaly_score: msg.anomaly_score ?? 0,
            },
            ...prev,
          ];
          return next.slice(0, 20);
        });
      }
      if (msg.type === "alert") {
        setAlerts((prev) =>
          [
            {
              id: msg.id ?? Date.now(),
              node_ids: msg.node_ids ?? [],
              type: msg.type,
              severity: msg.severity ?? "low",
              message: msg.message ?? "",
              created_at: msg.created_at || new Date().toISOString(),
            },
            ...prev,
          ].slice(0, 20)
        );
      }
    });
    return () => unsub();
  }, []);

  const avgBattery = useMemo(() => (nodes.length ? nodes.reduce((acc, n) => acc + n.battery_v, 0) / nodes.length : 0), [nodes]);
  const activeCount = nodes.length;
  const highAnomaly = nodes.filter((n) => n.anomaly_score > 0.5).length;

  return (
    <div
      style={{
        padding: "1.5rem",
        fontFamily: "'Space Grotesk', 'Inter', system-ui, sans-serif",
        background: "linear-gradient(135deg, #0b132b 0%, #1c2541 60%, #3a506b 100%)",
        color: "#e8f1ff",
        minHeight: "100vh",
      }}
    >
      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: "1rem" }}>
        <div>
          <div style={{ letterSpacing: "0.08em", fontSize: "0.85rem", color: "#9fb3c8" }}>O&L RF Mesh</div>
          <h1 style={{ margin: 0, fontSize: "2rem" }}>Dashboard</h1>
        </div>
        <div style={{ fontSize: "0.9rem", color: "#9fb3c8" }}>Backend: {import.meta.env.VITE_BACKEND_URL || "http://localhost:8000"}</div>
      </div>

      {loading && <p>Loading nodes…</p>}
      {error && <p style={{ color: "salmon" }}>Error: {error}</p>}

      {!loading && !error && (
        <>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fit, minmax(180px, 1fr))", gap: "0.75rem", marginBottom: "1rem" }}>
            <StatCard label="Active nodes" value={activeCount} accent="#4ade80" />
            <StatCard label="High anomaly" value={highAnomaly} accent="#f97316" />
            <StatCard label="Avg battery (V)" value={avgBattery.toFixed(2)} accent="#38bdf8" />
            <StatCard label="Alerts" value={alerts.length} accent="#f43f5e" />
          </div>

          <div style={{ display: "grid", gridTemplateColumns: "2fr 1fr", gap: "1rem" }}>
            <Panel title="Fleet status">
              <NodeTable nodes={nodes} />
            </Panel>
            <Panel title="Recent RF events">
              <EventList events={recentEvents} />
            </Panel>
          </div>

          <div style={{ marginTop: "1rem" }}>
            <Panel title="Alerts">
              <AlertList alerts={alerts} />
            </Panel>
          </div>
        </>
      )}
    </div>
  );
}

export default App;

function StatCard({ label, value, accent }: { label: string; value: number | string; accent: string }) {
  return (
    <div
      style={{
        background: "rgba(255,255,255,0.06)",
        border: `1px solid ${accent}33`,
        borderRadius: "12px",
        padding: "0.9rem",
        boxShadow: "0 6px 20px rgba(0,0,0,0.25)",
      }}
    >
      <div style={{ fontSize: "0.85rem", color: "#cbd5e1" }}>{label}</div>
      <div style={{ fontSize: "1.6rem", fontWeight: 700, color: accent }}>{value}</div>
    </div>
  );
}

function Panel({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <div
      style={{
        background: "rgba(10,16,29,0.6)",
        border: "1px solid rgba(255,255,255,0.08)",
        borderRadius: "14px",
        padding: "1rem",
        boxShadow: "0 10px 30px rgba(0,0,0,0.35)",
      }}
    >
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "0.5rem" }}>
        <h3 style={{ margin: 0 }}>{title}</h3>
      </div>
      <div style={{ borderTop: "1px solid rgba(255,255,255,0.06)", paddingTop: "0.5rem" }}>{children}</div>
    </div>
  );
}

function NodeTable({ nodes }: { nodes: NodeStatus[] }) {
  return (
    <div style={{ overflowX: "auto" }}>
      <table style={{ width: "100%", borderCollapse: "collapse", minWidth: "600px" }}>
        <thead>
          <tr style={{ textAlign: "left", color: "#9fb3c8", fontSize: "0.9rem" }}>
            <th style={{ padding: "0.4rem" }}>Node</th>
            <th style={{ padding: "0.4rem" }}>Last seen</th>
            <th style={{ padding: "0.4rem" }}>Battery (V)</th>
            <th style={{ padding: "0.4rem" }}>RF Power (dBm)</th>
            <th style={{ padding: "0.4rem" }}>Temp (°C)</th>
            <th style={{ padding: "0.4rem" }}>Anomaly</th>
            <th style={{ padding: "0.4rem" }}>GPS</th>
          </tr>
        </thead>
        <tbody>
          {nodes.map((n) => (
            <tr key={n.node_id} style={{ borderTop: "1px solid rgba(255,255,255,0.05)" }}>
              <td style={{ padding: "0.45rem 0.4rem", fontWeight: 600 }}>{n.node_id}</td>
              <td style={{ padding: "0.45rem 0.4rem" }}>{new Date(n.last_seen).toLocaleString()}</td>
              <td style={{ padding: "0.45rem 0.4rem" }}>{n.battery_v.toFixed(2)}</td>
              <td style={{ padding: "0.45rem 0.4rem" }}>{n.rf_power_dbm.toFixed(1)}</td>
              <td style={{ padding: "0.45rem 0.4rem" }}>{n.temp_c.toFixed(1)}</td>
              <td style={{ padding: "0.45rem 0.4rem", color: n.anomaly_score > 0.5 ? "#f59e0b" : "#38bdf8" }}>{n.anomaly_score.toFixed(3)}</td>
              <td style={{ padding: "0.45rem 0.4rem" }}>
                {n.gps_quality?.valid_fix ? "Fix" : "No fix"} {n.gps_quality?.num_sats ? `(${n.gps_quality.num_sats} sats)` : ""}
              </td>
            </tr>
          ))}
          {nodes.length === 0 && (
            <tr>
              <td colSpan={7} style={{ textAlign: "center", padding: "0.75rem", color: "#9fb3c8" }}>
                No nodes yet.
              </td>
            </tr>
          )}
        </tbody>
      </table>
    </div>
  );
}

function EventList({ events }: { events: RfEvent[] }) {
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "0.4rem" }}>
      {events.map((e, idx) => (
        <div
          key={`${e.node_id}-${idx}`}
          style={{
            display: "grid",
            gridTemplateColumns: "1fr 1fr auto",
            background: "rgba(255,255,255,0.03)",
            padding: "0.5rem 0.6rem",
            borderRadius: "10px",
            border: "1px solid rgba(255,255,255,0.05)",
          }}
        >
          <div style={{ fontWeight: 600 }}>{e.node_id}</div>
          <div style={{ color: "#9fb3c8" }}>{new Date(e.timestamp).toLocaleTimeString()}</div>
          <div style={{ textAlign: "right", color: e.anomaly_score > 0.5 ? "#f97316" : "#38bdf8" }}>{e.anomaly_score.toFixed(3)}</div>
        </div>
      ))}
      {events.length === 0 && <div style={{ color: "#9fb3c8" }}>No RF events yet.</div>}
    </div>
  );
}

function AlertList({ alerts }: { alerts: Alert[] }) {
  return (
    <div style={{ display: "grid", gap: "0.5rem" }}>
      {alerts.map((a) => (
        <div
          key={a.id}
          style={{
            background: "rgba(255,255,255,0.04)",
            border: `1px solid ${severityColor(a.severity)}55`,
            borderRadius: "10px",
            padding: "0.6rem 0.7rem",
            display: "grid",
            gridTemplateColumns: "1fr auto",
            alignItems: "center",
          }}
        >
          <div>
            <div style={{ fontWeight: 700, color: severityColor(a.severity) }}>{a.message}</div>
            <div style={{ color: "#9fb3c8", fontSize: "0.9rem" }}>
              {a.type} · {a.node_ids.join(", ") || "all"} · {new Date(a.created_at).toLocaleString()}
            </div>
          </div>
          <div style={{ fontSize: "0.85rem", color: severityColor(a.severity), fontWeight: 700 }}>{a.severity}</div>
        </div>
      ))}
      {alerts.length === 0 && <div style={{ color: "#9fb3c8" }}>No alerts yet.</div>}
    </div>
  );
}

function severityColor(sev: Alert["severity"]) {
  switch (sev) {
    case "high":
    case "critical":
      return "#f43f5e";
    case "med":
      return "#f59e0b";
    default:
      return "#38bdf8";
  }
}
