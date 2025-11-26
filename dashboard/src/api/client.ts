export type NodeStatus = {
  node_id: string;
  last_seen: string;
  battery_v: number;
  rf_power_dbm: number;
  temp_c: number;
  anomaly_score: number;
  gps_quality?: {
    num_sats?: number;
    hdop?: number;
    valid_fix?: boolean;
  };
};

export type Alert = {
  id: number;
  node_ids: string[];
  type: string;
  severity: "low" | "med" | "high" | "critical";
  message: string;
  created_at: string;
};

export type RfEvent = {
  node_id: string;
  timestamp: string;
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

export async function getAlerts(): Promise<Alert[]> {
  const res = await fetch(`${backend}/alerts/`);
  if (!res.ok) {
    throw new Error(`Failed to fetch alerts: ${res.status}`);
  }
  return res.json();
}

export function openTelemetryStream(onMessage: (msg: any) => void): () => void {
  const ws = new WebSocket(`${backend.replace("http", "ws")}/ws/rf-stream`);
  ws.onmessage = (ev) => {
    try {
      const parsed = JSON.parse(ev.data);
      onMessage(parsed);
    } catch {
      // ignore malformed messages
    }
  };
  ws.onclose = () => {
    // no-op for now; reconnection handled by caller if desired
  };
  return () => ws.close();
}
