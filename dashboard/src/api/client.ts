export type NodeStatus = {
  node_id: string;
  last_seen: number;
  battery_v: number;
  rf_power_dbm: number;
  temp_c: number;
  anomaly_score: number;
};

const backend = import.meta.env.VITE_BACKEND_URL || "http://localhost:8000";

export async function getNodes(): Promise<NodeStatus[]> {
  const res = await fetch(`${backend}/nodes/`);
  if (!res.ok) {
    throw new Error(`Failed to fetch nodes: ${res.status}`);
  }
  return res.json();
}
