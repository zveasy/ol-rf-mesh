import { useEffect, useMemo, useState } from "react";
import {
  Alert,
  getAlerts,
  getMockSpectra,
  getMockTopology,
  getNodes,
  getRfEvents,
  isMockMode,
  NodeStatus,
  openTelemetryStream,
  RfEvent,
  SpectrumSnapshot,
  getSpectrumSnapshots,
  getSpectrumHistory,
  listBands,
} from "./api/client";

type TopologyNode = { id: string; peers: string[]; health: "good" | "fair" | "warn" | "down" };

const PAGE_SIZE = 10;

function App() {
  const [nodes, setNodes] = useState<NodeStatus[]>([]);
  const [alerts, setAlerts] = useState<Alert[]>([]);
  const [recentEvents, setRecentEvents] = useState<RfEvent[]>([]);
  const [historicalEvents, setHistoricalEvents] = useState<RfEvent[]>([]);
  const [spectra, setSpectra] = useState<SpectrumSnapshot[]>([]);
  const [topology, setTopology] = useState<TopologyNode[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [useMock, setUseMock] = useState(isMockMode());
  const [historyPage, setHistoryPage] = useState(0);
  const [severityFilter, setSeverityFilter] = useState<Alert["severity"] | "all">("all");
  const [bandFilter, setBandFilter] = useState<string | "all">("all");
  const [sinceHours, setSinceHours] = useState<number>(6);
  const [bandOptions, setBandOptions] = useState<string[]>(["all", "915mhz", "2.4ghz", "5ghz", "legacy"]);
  const [timeCursor, setTimeCursor] = useState(0);

  const fetchData = async () => {
    setLoading(true);
    setError(null);
    try {
      const sinceIso = new Date(Date.now() - sinceHours * 3600 * 1000).toISOString();
      const [nodeData, alertData, rfData, spectrumData] = await Promise.all([
        getNodes({ mock: useMock }),
        getAlerts({ mock: useMock }),
        getRfEvents({ limit: PAGE_SIZE, offset: historyPage * PAGE_SIZE, mock: useMock }).catch(() => []),
        bandFilter === "all"
          ? getSpectrumSnapshots()
          : getSpectrumHistory({ band_id: bandFilter, since: sinceIso, limit: 30 }).catch(() => getSpectrumSnapshots()),
      ]);
      setNodes(nodeData);
      setAlerts(alertData);
      setHistoricalEvents(rfData);
      setSpectra(spectrumData.length ? spectrumData : getMockSpectra());
      setTopology(getMockTopology());
    } catch (e: any) {
      setError(e.message ?? "Unknown error");
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchData();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [useMock, historyPage, bandFilter, sinceHours]);

  useEffect(() => {
    (async () => {
      const bands = await listBands();
      setBandOptions(["all", ...bands]);
    })();
  }, [useMock]);

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
          return next.slice(0, 30);
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
          ].slice(0, 30)
        );
      }
    }, { mock: useMock });
    return () => unsub();
  }, [useMock]);

  const avgBattery = useMemo(() => (nodes.length ? nodes.reduce((acc, n) => acc + n.battery_v, 0) / nodes.length : 0), [nodes]);
  const activeCount = nodes.length;
  const highAnomaly = nodes.filter((n) => n.anomaly_score > 0.5).length;
  const filteredAlerts = useMemo(
    () => alerts.filter((a) => (severityFilter === "all" ? true : a.severity === severityFilter)),
    [alerts, severityFilter]
  );
  const historyTotalPages = Math.max(1, Math.ceil(historicalEvents.length / PAGE_SIZE));
  const pagedHistory = useMemo(
    () => historicalEvents.slice(historyPage * PAGE_SIZE, historyPage * PAGE_SIZE + PAGE_SIZE),
    [historicalEvents, historyPage]
  );

  const backendLabel = useMock ? "Mock data" : import.meta.env.VITE_BACKEND_URL || "http://localhost:8000";

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
        <div style={{ display: "flex", gap: "0.75rem", alignItems: "center" }}>
          <div style={{ fontSize: "0.9rem", color: "#9fb3c8" }}>Source: {backendLabel}</div>
          <button
            onClick={() => {
              setUseMock((prev) => !prev);
              setHistoryPage(0);
            }}
            style={{
              background: useMock ? "#38bdf8" : "#1f2937",
              color: useMock ? "#0b132b" : "#e8f1ff",
              border: "1px solid rgba(255,255,255,0.2)",
              borderRadius: "10px",
              padding: "0.4rem 0.75rem",
              cursor: "pointer",
            }}
          >
            {useMock ? "Switch to real backend" : "Use mock data"}
          </button>
        </div>
      </div>

      {error && (
        <p style={{ color: "salmon" }}>
          Error: {error === "unauthorized" ? "Unauthorized – set VITE_API_TOKEN for backend access" : error}
        </p>
      )}

      {loading ? (
        <SkeletonRows />
      ) : (
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

          <div style={{ marginTop: "1rem", display: "grid", gridTemplateColumns: "1.4fr 1fr", gap: "1rem" }}>
            <Panel title="Anomaly trend (live)">
              <AnomalySparkline events={recentEvents} />
            </Panel>
            <Panel title="Topology (mock)">
              <TopologyView topology={topology} />
            </Panel>
          </div>

          <div style={{ marginTop: "1rem", display: "grid", gridTemplateColumns: "1.4fr 1fr", gap: "1rem" }}>
            <Panel title="RF spectra">
              <SpectrumControls
                bandFilter={bandFilter}
                sinceHours={sinceHours}
                onBandChange={setBandFilter}
                onSinceChange={setSinceHours}
                bandOptions={bandOptions}
              />
              <Waterfall spectra={spectra} timeCursor={timeCursor} onCursorChange={setTimeCursor} />
              <SpectrumTimeline spectra={spectra} />
            </Panel>
            <Panel title="Alert triage">
              <AlertTriage alerts={filteredAlerts} onFilter={setSeverityFilter} filter={severityFilter} />
            </Panel>
          </div>

          <div style={{ marginTop: "1rem" }}>
            <Panel title="RF event history">
              <HistoricalList events={pagedHistory} />
              <Paginator page={historyPage} totalPages={historyTotalPages} onChange={setHistoryPage} />
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
      <table style={{ width: "100%", borderCollapse: "collapse", minWidth: "620px" }}>
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

function AlertTriage({
  alerts,
  onFilter,
  filter,
}: {
  alerts: Alert[];
  onFilter: (sev: Alert["severity"] | "all") => void;
  filter: Alert["severity"] | "all";
}) {
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "0.6rem" }}>
      <div style={{ display: "flex", gap: "0.5rem", alignItems: "center" }}>
        <label style={{ color: "#9fb3c8", fontSize: "0.9rem" }}>Severity</label>
        <select
          value={filter}
          onChange={(e) => onFilter(e.target.value as Alert["severity"] | "all")}
          style={{ background: "#0b132b", color: "#e8f1ff", border: "1px solid rgba(255,255,255,0.2)", borderRadius: "8px", padding: "0.3rem" }}
        >
          <option value="all">All</option>
          <option value="low">Low</option>
          <option value="med">Med</option>
          <option value="high">High</option>
          <option value="critical">Critical</option>
        </select>
      </div>
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
        {alerts.length === 0 && <div style={{ color: "#9fb3c8" }}>No alerts.</div>}
      </div>
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

function AnomalySparkline({ events }: { events: RfEvent[] }) {
  if (!events.length) {
    return <div style={{ color: "#9fb3c8" }}>No data yet.</div>;
  }
  const last = events.slice(0, 30).reverse(); // oldest to newest
  const values = last.map((e) => e.anomaly_score);
  const max = Math.max(...values, 1);
  const min = Math.min(...values, 0);
  const range = max - min || 1;
  const width = 300;
  const height = 80;
  const step = width / Math.max(values.length - 1, 1);
  const points = values
    .map((v, i) => {
      const x = i * step;
      const y = height - ((v - min) / range) * height;
      return `${x},${y}`;
    })
    .join(" ");

  return (
    <div style={{ display: "flex", alignItems: "center", gap: "1rem" }}>
      <svg width={width} height={height} style={{ background: "rgba(255,255,255,0.03)", borderRadius: "8px" }}>
        <polyline points={points} fill="none" stroke="#38bdf8" strokeWidth="2" />
        <line x1={0} y1={height / 2} x2={width} y2={height / 2} stroke="rgba(255,255,255,0.05)" strokeDasharray="4 4" />
      </svg>
      <div style={{ color: "#cbd5e1", fontSize: "0.9rem" }}>
        <div>Latest: {values[values.length - 1].toFixed(3)}</div>
        <div>Min/Max: {min.toFixed(3)} / {max.toFixed(3)}</div>
      </div>
    </div>
  );
}

function HistoricalList({ events }: { events: RfEvent[] }) {
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "0.4rem" }}>
      {events.map((e, idx) => (
        <div
          key={`${e.node_id}-${idx}-hist`}
          style={{
            display: "grid",
            gridTemplateColumns: "1fr 1fr auto",
            background: "rgba(255,255,255,0.02)",
            padding: "0.5rem 0.6rem",
            borderRadius: "10px",
            border: "1px solid rgba(255,255,255,0.05)",
          }}
        >
          <div style={{ fontWeight: 600 }}>{e.node_id}</div>
          <div style={{ color: "#9fb3c8" }}>{new Date(e.timestamp).toLocaleString()}</div>
          <div style={{ textAlign: "right", color: e.anomaly_score > 0.5 ? "#f97316" : "#38bdf8" }}>{e.anomaly_score.toFixed(3)}</div>
        </div>
      ))}
      {events.length === 0 && <div style={{ color: "#9fb3c8" }}>No historical RF events.</div>}
    </div>
  );
}

function Paginator({ page, totalPages, onChange }: { page: number; totalPages: number; onChange: (p: number) => void }) {
  return (
    <div style={{ marginTop: "0.5rem", display: "flex", justifyContent: "flex-end", gap: "0.5rem", alignItems: "center" }}>
      <button
        onClick={() => onChange(Math.max(0, page - 1))}
        disabled={page === 0}
        style={{ padding: "0.3rem 0.6rem", borderRadius: "8px", border: "1px solid rgba(255,255,255,0.2)", background: "transparent", color: "#e8f1ff" }}
      >
        Prev
      </button>
      <span style={{ color: "#9fb3c8", fontSize: "0.9rem" }}>
        Page {page + 1} / {totalPages}
      </span>
      <button
        onClick={() => onChange(Math.min(totalPages - 1, page + 1))}
        disabled={page >= totalPages - 1}
        style={{ padding: "0.3rem 0.6rem", borderRadius: "8px", border: "1px solid rgba(255,255,255,0.2)", background: "transparent", color: "#e8f1ff" }}
      >
        Next
      </button>
    </div>
  );
}

function TopologyView({ topology }: { topology: TopologyNode[] }) {
  if (!topology.length) return <div style={{ color: "#9fb3c8" }}>No topology data.</div>;
  return (
    <div style={{ display: "grid", gap: "0.5rem" }}>
      {topology.map((n) => (
        <div
          key={n.id}
          style={{
            display: "grid",
            gridTemplateColumns: "1fr auto",
            background: "rgba(255,255,255,0.03)",
            padding: "0.5rem 0.6rem",
            borderRadius: "10px",
            border: "1px solid rgba(255,255,255,0.05)",
          }}
        >
          <div>
            <div style={{ fontWeight: 700 }}>{n.id}</div>
            <div style={{ color: "#9fb3c8", fontSize: "0.9rem" }}>Peers: {n.peers.join(", ") || "none"}</div>
          </div>
          <div style={{ color: healthColor(n.health), fontWeight: 700 }}>{n.health}</div>
        </div>
      ))}
    </div>
  );
}

function healthColor(status: TopologyNode["health"]) {
  switch (status) {
    case "good":
      return "#4ade80";
    case "fair":
      return "#38bdf8";
    case "warn":
      return "#f59e0b";
    default:
      return "#f43f5e";
  }
}

function Waterfall({ spectra, timeCursor, onCursorChange }: { spectra: SpectrumSnapshot[]; timeCursor: number; onCursorChange: (v: number) => void }) {
  if (!spectra.length) return <div style={{ color: "#9fb3c8" }}>No spectrum data (mock).</div>;
  const snap = spectra[Math.min(timeCursor, spectra.length - 1)];
  const label = `${snap.band_id} · ${new Date(snap.captured_at).toLocaleTimeString()}`;
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "0.75rem" }}>
      <div style={{ display: "flex", justifyContent: "space-between", color: "#cbd5e1" }}>
        <div>{label}</div>
        <div style={{ color: "#9fb3c8", fontSize: "0.85rem" }}>
          Snapshot {timeCursor + 1}/{spectra.length}
        </div>
      </div>
      <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fit, minmax(6px, 1fr))", gap: "1px", height: "140px" }}>
        {snap.points.map((p, idx) => (
          <div
            key={idx}
            title={`${(p.freq / 1e6).toFixed(2)} MHz, ${p.power.toFixed(1)} dBm`}
            style={{
              background: powerColor(p.power),
              width: "100%",
              borderRadius: "1px",
            }}
          />
        ))}
      </div>
      {spectra.length > 1 && (
        <input
          type="range"
          min={0}
          max={Math.max(0, spectra.length - 1)}
          value={timeCursor}
          onChange={(e) => onCursorChange(Number(e.target.value))}
          style={{ width: "100%" }}
        />
      )}
    </div>
  );
}

function SpectrumTimeline({ spectra }: { spectra: SpectrumSnapshot[] }) {
  if (!spectra.length) return null;
  const points = spectra
    .map((s) => ({
      t: new Date(s.captured_at).getTime(),
      peak: Math.max(...s.points.map((p) => p.power_dbm)),
    }))
    .sort((a, b) => a.t - b.t);
  if (points.length < 2) return null;
  const minT = points[0].t;
  const maxT = points[points.length - 1].t;
  const minP = Math.min(...points.map((p) => p.peak));
  const maxP = Math.max(...points.map((p) => p.peak));
  const rangeP = maxP - minP || 1;
  const width = 320;
  const height = 80;
  const coords = points
    .map((p) => {
      const x = ((p.t - minT) / (maxT - minT || 1)) * width;
      const y = height - ((p.peak - minP) / rangeP) * height;
      return `${x},${y}`;
    })
    .join(" ");
  return (
    <div style={{ marginTop: "0.5rem" }}>
      <div style={{ color: "#9fb3c8", fontSize: "0.85rem", marginBottom: "0.25rem" }}>Peak power over time</div>
      <svg width={width} height={height} style={{ background: "rgba(255,255,255,0.02)", borderRadius: "8px" }}>
        <polyline points={coords} fill="none" stroke="#38bdf8" strokeWidth="2" />
      </svg>
    </div>
  );
}

function powerColor(power: number) {
  if (power > -50) return "#f43f5e";
  if (power > -60) return "#f59e0b";
  if (power > -70) return "#38bdf8";
  return "#0ea5e9";
}

function SpectrumControls({
  bandFilter,
  sinceHours,
  onBandChange,
  onSinceChange,
  bandOptions,
}: {
  bandFilter: string | "all";
  sinceHours: number;
  onBandChange: (b: string | "all") => void;
  onSinceChange: (h: number) => void;
  bandOptions: string[];
}) {
  return (
    <div style={{ display: "flex", gap: "0.75rem", marginBottom: "0.75rem", alignItems: "center", flexWrap: "wrap" }}>
      <label style={{ color: "#9fb3c8", fontSize: "0.9rem" }}>Band</label>
      <select
        value={bandFilter}
        onChange={(e) => onBandChange(e.target.value as any)}
        style={{ background: "#0b132b", color: "#e8f1ff", border: "1px solid rgba(255,255,255,0.2)", borderRadius: "8px", padding: "0.3rem" }}
      >
        {bandOptions.map((b) => (
          <option key={b} value={b}>
            {b}
          </option>
        ))}
      </select>
      <label style={{ color: "#9fb3c8", fontSize: "0.9rem" }}>Since (hours)</label>
      <input
        type="number"
        min={1}
        max={72}
        value={sinceHours}
        onChange={(e) => onSinceChange(Math.max(1, Math.min(72, Number(e.target.value) || 1)))}
        style={{
          width: "70px",
          background: "#0b132b",
          color: "#e8f1ff",
          border: "1px solid rgba(255,255,255,0.2)",
          borderRadius: "8px",
          padding: "0.3rem",
        }}
      />
    </div>
  );
}

function SkeletonRows() {
  return (
    <div style={{ display: "grid", gap: "0.8rem" }}>
      {[...Array(3)].map((_, i) => (
        <div key={i} style={{ height: "90px", background: "rgba(255,255,255,0.04)", borderRadius: "12px", position: "relative", overflow: "hidden" }}>
          <div
            style={{
              position: "absolute",
              inset: 0,
              background: "linear-gradient(90deg, transparent, rgba(255,255,255,0.08), transparent)",
              animation: "shimmer 1.4s infinite",
            }}
          />
        </div>
      ))}
      <style>
        {`@keyframes shimmer {0%{transform: translateX(-100%);}100%{transform: translateX(100%);}}`}
      </style>
    </div>
  );
}
