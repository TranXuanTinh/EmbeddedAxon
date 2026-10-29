#ifndef AEGIS_APP_DEVICE_CONTROLLER_H_
#define AEGIS_APP_DEVICE_CONTROLLER_H_

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "aegis/common/clock.h"
#include "aegis/common/result.h"
#include "aegis/evidence/audit_log.h"
#include "aegis/evidence/evidence_store.h"
#include "aegis/evidence/frame.h"
#include "aegis/evidence/pre_event_buffer.h"
#include "aegis/hal/i_boot_control_hal.h"
#include "aegis/hal/i_camera_hal.h"
#include "aegis/hal/i_key_store_hal.h"
#include "aegis/hal/i_storage_hal.h"
#include "aegis/hal/i_watchdog_hal.h"
#include "aegis/health/watchdog_supervisor.h"
#include "aegis/ipc/command_protocol.h"
#include "aegis/ota/ab_slot_manager.h"
#include "aegis/recording/recording_state_machine.h"
#include "aegis/trigger/trigger_arbiter.h"
#include "aegis/util/spsc_ring_buffer.h"

namespace aegis::app {

struct DeviceConfig {
  std::string device_id{"AEG-0001"};
  uint64_t pre_event_ms{30000};
  uint64_t segment_ms{10000};
  uint64_t post_record_ms{5000};
  uint32_t fps{30};
  uint64_t debounce_ms{50};
  bool sim_mode{true};
};

class DeviceController {
 public:
  using EventCallback = std::function<void(const std::string& evt_line)>;

  DeviceController(DeviceConfig config, common::IClock& clock,
                   common::FakeClock* fake_clock, hal::ICameraHal& camera_hal,
                   hal::IStorageHal& storage_hal,
                   hal::IWatchdogHal& watchdog_hal,
                   hal::IKeyStoreHal& key_store_hal,
                   hal::IBootControlHal& boot_control_hal);

  // Initializes audit log, OTA slot state, watchdog, and transitions state
  // machine from Idle -> Buffering.
  Status start();

  // Graceful shutdown: stops ingress, drains & cuts remaining frames, seals
  // active incident, writes audit log record.
  Status shutdown();

  void set_event_callback(EventCallback cb);

  ipc::Response handle(const ipc::Command& cmd);
  std::string handle_line(std::string_view line);

  bool quit_requested() const noexcept { return quit_requested_; }
  recording::State state() const noexcept { return state_machine_.state(); }
  const std::string& active_incident_id() const noexcept {
    return active_incident_id_;
  }
  uint32_t active_segment_count() const noexcept {
    return active_segment_count_;
  }

 private:
  Status execute_actions(const std::vector<recording::Action>& actions);
  Status cut_segment_if_nonempty();
  Status seal_current_incident();
  void ingest_available_frames();
  std::string build_status_json() const;
  void emit_event(const std::string& evt_line);

  DeviceConfig config_;
  common::IClock& clock_;
  common::FakeClock* fake_clock_;
  hal::ICameraHal& camera_hal_;
  hal::IStorageHal& storage_hal_;
  hal::IWatchdogHal& watchdog_hal_;

  evidence::PreEventBuffer pre_event_buffer_;
  evidence::EvidenceStore evidence_store_;
  evidence::AuditLog audit_log_;
  recording::RecordingStateMachine state_machine_;
  trigger::TriggerArbiter trigger_arbiter_;
  ota::AbSlotManager ota_manager_;
  health::WatchdogSupervisor watchdog_supervisor_;
  util::SpscRingBuffer<evidence::Frame, 2048> capture_ring_;

  mutable std::mutex mutex_;
  EventCallback event_cb_;
  std::string active_incident_id_;
  uint32_t active_segment_count_{0};
  std::vector<evidence::Frame> current_segment_frames_;
  std::string net_state_{"down"};
  bool quit_requested_{false};
  bool started_{false};
};

}  // namespace aegis::app

#endif  // AEGIS_APP_DEVICE_CONTROLLER_H_
