import { useEffect, useState } from "react";
import { getNodes, NodeStatus } from "./api/client";

function App() {
  const [nodes, setNodes] = useState<NodeStatus[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    (async () => {
      try {
        const data = await getNodes();
        setNodes(data);
      } catch (e: any) {
        setError(e.message ?? "Unknown error");
      } finally {
        setLoading(false);
      }
    })();
  }, []);

  return (
    <div style={{ padding: "1.5rem", fontFamily: "system-ui, sans-serif" }}>
      <h1>O&amp;L RF Mesh Dashboard</h1>

      {loading && <p>Loading nodes…</p>}
      {error && <p style={{ color: "red" }}>Error: {error}</p>}

      {!loading && !error && (
        <table
          style={{
            borderCollapse: "collapse",
            marginTop: "1rem",
            minWidth: "600px",
          }}
        >
          <thead>
            <tr>
              <th>Node ID</th>
              <th>Last Seen</th>
              <th>Battery (V)</th>
              <th>RF Power (dBm)</th>
              <th>Temp (°C)</th>
              <th>Anomaly</th>
            </tr>
          </thead>
          <tbody>
            {nodes.map((n) => (
              <tr key={n.node_id}>
                <td>{n.node_id}</td>
                <td>{n.last_seen}</td>
                <td>{n.battery_v.toFixed(2)}</td>
                <td>{n.rf_power_dbm.toFixed(1)}</td>
                <td>{n.temp_c.toFixed(1)}</td>
                <td>{n.anomaly_score.toFixed(3)}</td>
              </tr>
            ))}
            {nodes.length === 0 && (
              <tr>
                <td colSpan={6} style={{ textAlign: "center", padding: "0.5rem" }}>
                  No nodes yet.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      )}
    </div>
  );
}

export default App;
