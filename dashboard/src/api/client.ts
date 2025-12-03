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
  band_id?: string;
  center_freq_hz?: number;
  bin_width_hz?: number;
  features?: number[];
};

export type SpectrumPoint = { freq_hz: number; power_dbm: number };
export type SpectrumSnapshot = { band_id: string; points: SpectrumPoint[]; captured_at: string };

const backend = import.meta.env.VITE_BACKEND_URL || "http://localhost:8000";
const preferMock = (import.meta.env.VITE_USE_MOCK ?? "").toLowerCase() === "true";
const apiToken = import.meta.env.VITE_API_TOKEN;

const localMock = {
  nodes: [
    {
      node_id: "demo-gateway",
      last_seen: new Date().toISOString(),
      battery_v: 4.02,
      rf_power_dbm: -44,
      temp_c: 36.4,
      anomaly_score: 0.1,
      gps_quality: { num_sats: 9, hdop: 1.1, valid_fix: true },
    },
  ] satisfies NodeStatus[],
  alerts: [
    {
      id: 99,
      node_ids: ["demo-gateway"],
      type: "rf_anomaly",
      severity: "low" as const,
      message: "Mock RF anomaly",
      created_at: new Date().toISOString(),
    },
  ] satisfies Alert[],
  rf_events: [
    {
      node_id: "demo-gateway",
      timestamp: new Date().toISOString(),
      anomaly_score: 0.12,
      band_id: "915mhz",
      center_freq_hz: 915000000,
      bin_width_hz: 12500,
      features: [-50, -49, -51],
    },
  ] satisfies RfEvent[],
  spectra: [
    {
      band_id: "915mhz",
      captured_at: new Date().toISOString(),
      points: Array.from({ length: 64 }).map((_, i) => ({
        freq_hz: 915e6 - 500e3 + i * 15_625,
        power_dbm: -70 + Math.sin(i / 4) * 10 + (i > 40 ? 8 : 0),
      })),
    },
    {
      band_id: "2.4ghz",
      captured_at: new Date().toISOString(),
      points: Array.from({ length: 64 }).map((_, i) => ({
        freq_hz: 2.4e9 - 10e6 + i * 312_500,
        power_dbm: -68 + Math.cos(i / 3) * 6,
      })),
    },
  ] satisfies SpectrumSnapshot[],
  topology: [
    { id: "demo-gateway", peers: ["demo-edge-1", "demo-edge-2"], health: "good" },
    { id: "demo-edge-1", peers: ["demo-gateway"], health: "fair" },
    { id: "demo-edge-2", peers: ["demo-gateway"], health: "warn" },
  ],
};

async function fetchJson<T>(path: string): Promise<T> {
  const headers: Record<string, string> = {};
  if (apiToken) {
    headers["Authorization"] = `Bearer ${apiToken}`;
  }
  const res = await fetch(`${backend}${path}`, { headers });
  if (!res.ok) {
    if (res.status === 401) {
      throw new Error("unauthorized");
    }
    throw new Error(`Request failed with status ${res.status}`);
  }
  return res.json() as Promise<T>;
}

async function fetchMockNodes(): Promise<NodeStatus[]> {
  try {
    return await fetchJson<NodeStatus[]>("/mock/nodes");
  } catch {
    return localMock.nodes;
  }
}

async function fetchMockEvents(): Promise<{ rf_events: RfEvent[]; alerts: Alert[] }> {
  try {
    return await fetchJson<{ rf_events: RfEvent[]; alerts: Alert[] }>("/mock/events");
  } catch {
    return { rf_events: localMock.rf_events, alerts: localMock.alerts };
  }
}

export async function getNodes(opts?: { mock?: boolean }): Promise<NodeStatus[]> {
  const mockMode = opts?.mock ?? preferMock;
  if (mockMode) return fetchMockNodes();
  try {
    const data = await fetchJson<NodeStatus[]>("/nodes/");
    if (data.length === 0) {
      return fetchMockNodes();
    }
    return data;
  } catch {
    return fetchMockNodes();
  }
}

export async function getAlerts(opts?: { mock?: boolean }): Promise<Alert[]> {
  const mockMode = opts?.mock ?? preferMock;
  if (mockMode) {
    const mock = await fetchMockEvents();
    return mock.alerts;
  }
  try {
    const data = await fetchJson<Alert[]>("/alerts/");
    if (data.length === 0) {
      const mock = await fetchMockEvents();
      return mock.alerts;
    }
    return data;
  } catch {
    const mock = await fetchMockEvents();
    return mock.alerts;
  }
}

export async function getRfEvents({ limit = 20, offset = 0, mock }: { limit?: number; offset?: number; mock?: boolean }): Promise<RfEvent[]> {
  const mockMode = mock ?? preferMock;
  if (mockMode) {
    const mockData = await fetchMockEvents();
    return mockData.rf_events.slice(offset, offset + limit);
  }
  try {
    const data = await fetchJson<{ items: RfEvent[] }>("/history/rf-events?limit=" + limit + "&offset=" + offset);
    if (!data.items || data.items.length === 0) {
      const mockData = await fetchMockEvents();
      return mockData.rf_events.slice(offset, offset + limit);
    }
    return data.items;
  } catch {
    const mockData = await fetchMockEvents();
    return mockData.rf_events.slice(offset, offset + limit);
  }
}

export function openTelemetryStream(onMessage: (msg: any) => void, opts?: { mock?: boolean }): () => void {
  const mockMode = opts?.mock ?? preferMock;
  if (mockMode) {
    // Simulate occasional events
    const timer = setInterval(() => {
      const mockEvent = localMock.rf_events[0];
      onMessage({
        type: "rf_event",
        node_id: mockEvent.node_id,
        timestamp: new Date().toISOString(),
        anomaly_score: mockEvent.anomaly_score + Math.random() * 0.1,
      });
    }, 1500);
    return () => clearInterval(timer);
  }
  try {
    const ws = new WebSocket(`${backend.replace("http", "ws")}/ws/rf-stream`);
    ws.onmessage = (ev) => {
      try {
        const parsed = JSON.parse(ev.data);
        onMessage(parsed);
      } catch {
        // ignore malformed messages
      }
    };
    ws.onerror = () => {
      ws.close();
    };
    ws.onclose = () => {
      // no-op for now; reconnection handled by caller if desired
    };
    return () => ws.close();
  } catch {
    return () => {};
  }
}

export function getMockSpectra(): SpectrumSnapshot[] {
  return localMock.spectra;
}

export function getMockTopology(): { id: string; peers: string[]; health: "good" | "fair" | "warn" | "down" }[] {
  return localMock.topology;
}

export function isMockMode(): boolean {
  return preferMock;
}

export async function getSpectrumSnapshots(): Promise<SpectrumSnapshot[]> {
  if (preferMock) {
    return getMockSpectra();
  }
  try {
    return await fetchJson<SpectrumSnapshot[]>("/spectrum/latest");
  } catch {
    return getMockSpectra();
  }
}

export async function getSpectrumHistory({
  band_id,
  since,
  limit = 50,
  offset = 0,
}: {
  band_id?: string;
  since?: string;
  limit?: number;
  offset?: number;
}): Promise<SpectrumSnapshot[]> {
  if (preferMock) {
    return getMockSpectra();
  }
  const params = new URLSearchParams();
  if (band_id) params.append("band_id", band_id);
  if (since) params.append("since", since);
  params.append("limit", String(limit));
  params.append("offset", String(offset));
  try {
    return await fetchJson<SpectrumSnapshot[]>("/spectrum/history?" + params.toString());
  } catch {
    return getMockSpectra();
  }
}

export async function listBands(): Promise<string[]> {
  if (preferMock) {
    const bands = Array.from(new Set(getMockSpectra().map((s) => s.band_id)));
    return bands.length ? bands : ["legacy"];
  }
  try {
    const latest = await getSpectrumSnapshots();
    const bands = Array.from(new Set(latest.map((s) => s.band_id)));
    return bands.length ? bands : ["legacy"];
  } catch {
    return ["legacy"];
  }
}
