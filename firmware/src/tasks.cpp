#include "tasks.hpp"
#include "adc.hpp"
#include "fault.hpp"
#include "mesh.hpp"
#include "model_inference.hpp"
#include "ota.hpp"
#include "sensors.hpp"
#include "watchdog.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#ifdef OL_FREERTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

namespace {
using TaskFn = void(*)(const NodeConfig&, uint32_t, TaskHeartbeat&);

struct TransportItem {
    MeshFrame frame{};
    uint8_t attempts = 0;
    uint32_t next_attempt_ms = 0;
    bool in_use = false;
};

struct TransportQueue {
    static constexpr std::size_t depth = 4;
    static constexpr uint32_t retry_backoff_ms = 250;
    static constexpr uint8_t max_retries = 3;
    std::array<TransportItem, depth> slots{};
    std::size_t head = 0;
    std::size_t tail = 0;
    std::size_t size = 0;

    bool full() const { return size >= depth; }
    bool empty() const { return size == 0; }

    bool push(const MeshFrame& frame) {
        if (full()) {
            return false;
        }
        slots[tail].frame = frame;
        slots[tail].attempts = 0;
        slots[tail].next_attempt_ms = 0;
        slots[tail].in_use = true;
        tail = (tail + 1) % depth;
        size++;
        return true;
    }

    TransportItem& front() {
        return slots[head];
    }

    void pop() {
        if (empty()) {
            return;
        }
        slots[head].in_use = false;
        head = (head + 1) % depth;
        size--;
    }
};

struct TaskQueues {
    RFSampleWindow last_rf_window{};
    RFEvent last_rf_event{};
    GpsStatus last_gps{};
    HealthStatus last_health{};
};

TaskQueues g_queues{};
TransportQueue g_transport_queue{};
uint32_t g_seq_no = 0;
NodeConfig g_runtime_cfg{};

TaskStatus g_status{
    {"TransportTask", 0},
    {"RFScanTask", 0},
    {"FFTTflmTask", 0},
    {"GNSSMonitorTask", 0},
    {"SensorHealthTask", 0},
    {"PacketBuilderTask", 0},
    {"OtaUpdateTask", 0},
    {"FaultMonitorTask", 0},
    {},
};

const std::array<TaskConfig, kTaskCount> kPlan{{
    {"FaultMonitorTask", 6, 768, 250, true, 750},
    {"RFScanTask", 5, 2048, 500, true, 1000},
    {"FFTTflmTask", 5, 3584, 500, true, 1000},
    {"PacketBuilderTask", 4, 2048, 1000, true, 2000},
    {"TransportTask", 4, 2048, 250, true, 750},
    {"GNSSMonitorTask", 3, 1536, 2000, false, 0},
    {"SensorHealthTask", 3, 1536, 1000, true, 2000},
    {"OtaUpdateTask", 2, 2048, 5000, true, 8000},
}};

void touch(TaskHeartbeat& hb, uint32_t now_ms) {
    hb.last_beat_ms = now_ms;
}

bool enqueue_transport(const MeshFrame& frame) {
    if (!g_transport_queue.push(frame)) {
        record_fault("Transport queue full");
        return false;
    }
    return true;
}

void service_transport_queue(uint32_t now_ms) {
    if (g_transport_queue.empty()) {
        return;
    }
    TransportItem& item = g_transport_queue.front();
    if (now_ms < item.next_attempt_ms) {
        return;
    }

    const bool ok = send_mesh_frame(item.frame);
    if (ok) {
        g_transport_queue.pop();
        return;
    }

    item.attempts++;
    if (item.attempts > TransportQueue::max_retries) {
        record_fault("Transport retries exceeded");
        note_retry_drop();
        g_transport_queue.pop();
        return;
    }
    item.next_attempt_ms = now_ms + TransportQueue::retry_backoff_ms;
}

void rf_scan_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    g_queues.last_rf_window = collect_rf_window(now_ms);
    g_queues.last_rf_window.center_freq_hz = cfg.rf_center_freq_hz;
    touch(hb, now_ms);
}

void fft_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    const RfFeatures features = extract_rf_features(g_queues.last_rf_window);
    const float score = run_model_inference(features);

    g_queues.last_rf_event.timestamp_ms = now_ms;
    g_queues.last_rf_event.center_freq_hz = cfg.rf_center_freq_hz;
    g_queues.last_rf_event.features = features;
    g_queues.last_rf_event.anomaly_score = score;
    g_queues.last_rf_event.model_version = 1;

    touch(hb, now_ms);
}

void gnss_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    (void)cfg;
    g_queues.last_gps = read_gps_status(now_ms);
    touch(hb, now_ms);
}

void health_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    (void)cfg;
    g_queues.last_health = read_health_status(now_ms);
    touch(hb, now_ms);
}

void packet_builder_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    MeshFrame frame{};
    frame.header.version = 1;
    frame.header.msg_type = MeshMsgType::Telemetry;
    frame.header.ttl = 4;
    frame.header.hop_count = 0;
    frame.header.seq_no = ++g_seq_no;

    std::snprintf(frame.header.src_node_id, sizeof(frame.header.src_node_id), "%s", cfg.node_id.c_str());
    frame.header.dest_node_id[0] = '\0'; // broadcast in stub

    frame.security.encrypted = true;
    for (std::size_t i = 0; i < frame.security.nonce.size(); ++i) {
        frame.security.nonce[i] = static_cast<uint8_t>((g_seq_no >> (i % 4)) & 0xFF);
    }
    frame.security.auth_tag.fill(0); // placeholder until AES-GCM integrated

    frame.counters.tx_counter = g_seq_no;
    frame.counters.replay_window = 0;

    frame.telemetry.rf_event = g_queues.last_rf_event;
    frame.telemetry.gps = g_queues.last_gps;
    frame.telemetry.health = g_queues.last_health;

    frame.routing = current_routing_payload();
    frame.routing.epoch_ms = now_ms;
    frame.fault = fault_status();
    frame.ota = ota_status();

    enqueue_transport(frame);
    touch(hb, now_ms);
}

void transport_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    (void)cfg;
    service_transport_queue(now_ms);
    touch(hb, now_ms);
}

void ota_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    (void)cfg;
    touch(hb, now_ms);
}

void fault_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    (void)cfg;
    if (g_queues.last_health.tamper_flag) {
        record_tamper();
    }
    touch(hb, now_ms);
}

struct TaskSlot {
    TaskConfig cfg;
    TaskHeartbeat* hb;
    TaskFn fn;
    uint32_t next_release_ms;
};

void sort_slots_by_priority(std::array<TaskSlot, kTaskCount>& slots) {
    std::sort(slots.begin(), slots.end(), [](const TaskSlot& a, const TaskSlot& b) {
        if (a.cfg.priority == b.cfg.priority) {
            return a.cfg.period_ms < b.cfg.period_ms;
        }
        return a.cfg.priority > b.cfg.priority;
    });
}

void enforce_watchdog(const TaskSlot& slot, uint32_t now_ms) {
    if (!slot.cfg.watchdog_protected) {
        return;
    }
    const uint32_t budget = slot.cfg.watchdog_budget_ms ? slot.cfg.watchdog_budget_ms
                                                       : (slot.cfg.period_ms * 2);
    if (now_ms > slot.hb->last_beat_ms && (now_ms - slot.hb->last_beat_ms) > budget) {
        record_watchdog_reset();
    }
}

std::array<TaskSlot, kTaskCount>& task_slots() {
    static std::array<TaskSlot, kTaskCount> slots = {{
        {kPlan[0], &g_status.fault_monitor, fault_task, 0},
        {kPlan[1], &g_status.rf_scan, rf_scan_task, 0},
        {kPlan[2], &g_status.fft, fft_task, 0},
        {kPlan[3], &g_status.packet_builder, packet_builder_task, 0},
        {kPlan[4], &g_status.transport, transport_task, 0},
        {kPlan[5], &g_status.gnss, gnss_task, 0},
        {kPlan[6], &g_status.health, health_task, 0},
        {kPlan[7], &g_status.ota, ota_task, 0},
    }};
    static bool slots_sorted = false;
    if (!slots_sorted) {
        sort_slots_by_priority(slots);
        slots_sorted = true;
    }
    return slots;
}

#ifdef OL_FREERTOS
void rtos_task_entry(void* arg) {
    auto* slot_ptr = static_cast<TaskSlot*>(arg);
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        const uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        slot_ptr->fn(g_runtime_cfg, now_ms, *slot_ptr->hb);
        if (slot_ptr->cfg.watchdog_protected) {
            watchdog_feed(slot_ptr->cfg.name);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(slot_ptr->cfg.period_ms));
    }
}
#endif
} // namespace

const std::array<TaskConfig, kTaskCount>& task_plan() {
    return kPlan;
}

TaskStatus run_firmware_cycle(const NodeConfig& cfg, uint32_t now_ms) {
    auto& slots = task_slots();

    for (auto& slot : slots) {
        if (now_ms >= slot.next_release_ms) {
            slot.fn(cfg, now_ms, *slot.hb);
            slot.next_release_ms = now_ms + slot.cfg.period_ms;
        }
        enforce_watchdog(slot, now_ms);
    }

    g_status.faults = fault_status();
    return g_status;
}

void start_freertos_tasks(const NodeConfig& cfg) {
#ifdef OL_FREERTOS
    g_runtime_cfg = cfg;
    uint32_t max_watchdog_ms = 0;
    for (const auto& slot_cfg : kPlan) {
        if (slot_cfg.watchdog_protected) {
            max_watchdog_ms = std::max(max_watchdog_ms, slot_cfg.watchdog_budget_ms);
        }
    }
    watchdog_init(max_watchdog_ms);
    auto& slots = task_slots();

    for (auto& slot : slots) {
        xTaskCreate(
            rtos_task_entry,
            slot.cfg.name,
            slot.cfg.stack_words,
            &slot,
            slot.cfg.priority,
            nullptr
        );
        if (slot.cfg.watchdog_protected) {
            watchdog_register_task(slot.cfg.name, slot.cfg.watchdog_budget_ms);
        }
    }
#else
    (void)cfg;
#endif
}
