#include "aegis/app/device_controller.h"

#include <algorithm>
#include <sstream>

namespace aegis::app {

DeviceController::DeviceController(
    DeviceConfig config, common::IClock& clock, common::FakeClock* fake_clock,
    hal::ICameraHal& camera_hal, hal::IStorageHal& storage_hal,
    hal::IWatchdogHal& watchdog_hal, hal::IKeyStoreHal& key_store_hal,
    hal::IBootControlHal& boot_control_hal)
    : config_(std::move(config)),
      clock_(clock),
      fake_clock_(fake_clock),
      camera_hal_(camera_hal),
      storage_hal_(storage_hal),
      watchdog_hal_(watchdog_hal),
      pre_event_buffer_(config_.pre_event_ms, 8192),
      evidence_store_(storage_hal, key_store_hal, config_.device_id,
                      config_.pre_event_ms),
      audit_log_(storage_hal, clock),
      state_machine_(config_.segment_ms, config_.post_record_ms),
      trigger_arbiter_(config_.debounce_ms),
      ota_manager_(boot_control_hal, storage_hal),
      watchdog_supervisor_(watchdog_hal, clock) {}

void DeviceController::set_event_callback(EventCallback cb) {
  std::lock_guard<std::mutex> lock(mutex_);
  event_cb_ = std::move(cb);
}

void DeviceController::emit_event(const std::string& evt_line) {
  if (event_cb_) {
    event_cb_(evt_line);
  }
}

Status DeviceController::start() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (started_) {
    return Status::Ok();
  }

  auto audit_st = audit_log_.init();
  if (!audit_st.ok()) {
    return audit_st;
  }

  auto rec_st = evidence_store_.recover();
  if (!rec_st.ok()) {
    return rec_st;
  }

  auto ota_st = ota_manager_.init();
  if (!ota_st.ok()) {
    return ota_st;
  }

  auto wd_st = watchdog_hal_.start(10000);
  if (!wd_st.ok()) {
    return wd_st;
  }

  watchdog_supervisor_.register_thread("capture", 5000);
  watchdog_supervisor_.register_thread("controller", 5000);
  watchdog_supervisor_.register_thread("writer", 5000);

  const auto tr = state_machine_.dispatch(recording::Event::PowerOn());
  auto act_st = execute_actions(tr.actions);
  if (!act_st.ok()) {
    return act_st;
  }

  auto log_st = audit_log_.append("POWER_ON", "device=" + config_.device_id +
                                                  ",slot=" +
                                                  ota_manager_.active_slot());
  if (!log_st.ok()) {
    return log_st;
  }

  ingest_available_frames();
  started_ = true;
  return Status::Ok();
}

void DeviceController::ingest_available_frames() {
  while (true) {
    auto frame_res = camera_hal_.read_frame();
    if (!frame_res.ok()) {
      break;
    }
    if (!capture_ring_.try_push( std::move(frame_res.value()))) {
      // Ring full: drain one and push
      (void)capture_ring_.try_pop();
    }
  }

  evidence::Frame frame;
  while (capture_ring_.try_pop(frame)) {
    const auto st = state_machine_.state();
    if (st == recording::State::kBuffering) {
      pre_event_buffer_.push(std::move(frame));
    } else if (st == recording::State::kRecording ||
               st == recording::State::kPostRecord) {
      current_segment_frames_.push_back(std::move(frame));
    }
  }
}

Status DeviceController::cut_segment_if_nonempty() {
  if (active_incident_id_.empty() || current_segment_frames_.empty()) {
    return Status::Ok();
  }
  auto seg_res = evidence_store_.append_segment(active_incident_id_,
                                                current_segment_frames_);
  if (!seg_res.ok()) {
    return seg_res.status();
  }
  current_segment_frames_.clear();
  ++active_segment_count_;

  const auto& seg = seg_res.value();
  const std::string rel_path =
      "incidents/" + active_incident_id_ + "/" + seg.file;
  emit_event(ipc::CommandProtocol::format_segment_sealed_event(
      active_incident_id_, seg.index, rel_path, seg.sha256));
  return Status::Ok();
}

Status DeviceController::seal_current_incident() {
  if (active_incident_id_.empty()) {
    return Status::Ok();
  }
  if (!current_segment_frames_.empty()) {
    auto cut_st = cut_segment_if_nonempty();
    if (!cut_st.ok()) {
      return cut_st;
    }
  }
  auto seal_st = evidence_store_.seal_incident(active_incident_id_);
  if (!seal_st.ok()) {
    return seal_st;
  }
  const std::string sealed_id = active_incident_id_;
  const uint32_t total_segs = active_segment_count_;
  trigger_arbiter_.clear_active();

  auto log_st = audit_log_.append(
      "INCIDENT_SEALED",
      "incident=" + sealed_id + ",segments=" + std::to_string(total_segs));
  if (!log_st.ok()) {
    return log_st;
  }

  emit_event(ipc::CommandProtocol::format_incident_sealed_event(sealed_id,
                                                                total_segs));
  return Status::Ok();
}

Status DeviceController::execute_actions(
    const std::vector<recording::Action>& actions) {
  for (const auto& action : actions) {
    switch (action.type) {
      case recording::ActionType::kStartPreEventBuffer:
        pre_event_buffer_.clear();
        break;

      case recording::ActionType::kBeginIncident: {
        current_segment_frames_.clear();
        active_segment_count_ = 0;
        auto inc_res =
            evidence_store_.begin_incident(clock_.now_ms(), action.detail);
        if (!inc_res.ok()) {
          return inc_res.status();
        }
        active_incident_id_ = inc_res.value();
        auto log_st = audit_log_.append(
            "INCIDENT_START",
            "incident=" + active_incident_id_ + ",src=" + action.detail);
        if (!log_st.ok()) {
          return log_st;
        }
        break;
      }

      case recording::ActionType::kFlushPreEventBuffer: {
        auto drained = pre_event_buffer_.drain();
        for (auto& f : drained) {
          current_segment_frames_.push_back(std::move(f));
        }
        break;
      }

      case recording::ActionType::kCutSegment: {
        auto st = cut_segment_if_nonempty();
        if (!st.ok()) {
          return st;
        }
        break;
      }

      case recording::ActionType::kEnterPostRecord: {
        auto st =
            audit_log_.append("POST_RECORD", "incident=" + active_incident_id_);
        if (!st.ok()) {
          return st;
        }
        break;
      }

      case recording::ActionType::kResumeRecording: {
        auto st = audit_log_.append(
            "RECORDING_RESUMED",
            "incident=" + active_incident_id_ + ",src=" + action.detail);
        if (!st.ok()) {
          return st;
        }
        break;
      }

      case recording::ActionType::kSealIncident: {
        auto st = seal_current_incident();
        if (!st.ok()) {
          return st;
        }
        break;
      }

      case recording::ActionType::kRaiseFault: {
        auto st = audit_log_.append("FAULT", action.detail);
        if (!st.ok()) {
          return st;
        }
        break;
      }

      case recording::ActionType::kClearFault: {
        auto st = audit_log_.append("FAULT_CLEARED", action.detail);
        if (!st.ok()) {
          return st;
        }
        break;
      }
    }
  }
  return Status::Ok();
}

std::string DeviceController::build_status_json() const {
  std::ostringstream oss;
  oss << "{\"state\":\"" << recording::state_to_string(state_machine_.state())
      << "\",\"incident\":\"" << active_incident_id_
      << "\",\"segments\":" << active_segment_count_ << ",\"net\":\""
      << net_state_ << "\",\"slot\":\"" << ota_manager_.active_slot() << "\"}";
  return oss.str();
}

Status DeviceController::shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (quit_requested_) {
    return Status::Ok();
  }
  quit_requested_ = true;

  // 1. Drain any remaining camera frames
  ingest_available_frames();

  // 2. If currently Recording or in PostRecord, flush final segment & seal
  const auto st = state_machine_.state();
  if (st == recording::State::kRecording ||
      st == recording::State::kPostRecord) {
    auto seal_st = seal_current_incident();
    if (!seal_st.ok()) {
      return seal_st;
    }
  }

  // 3. Audit graceful shutdown
  auto log_st = audit_log_.append("SHUTDOWN", "graceful");
  if (!log_st.ok()) {
    return log_st;
  }

  (void)watchdog_hal_.stop();
  return Status::Ok();
}

ipc::Response DeviceController::handle(const ipc::Command& cmd) {
  std::unique_lock<std::mutex> lock(mutex_);

  watchdog_supervisor_.heartbeat("controller");

  switch (cmd.type) {
    case ipc::CommandType::kTrigger: {
      if (state_machine_.state() == recording::State::kFault) {
        return ipc::Response::Error(ErrorCode::kInvalidState,
                                    "Device is in Fault state");
      }
      auto src_res = trigger::parse_trigger_source(cmd.arg1);
      if (!src_res.ok()) {
        return ipc::Response::Error(src_res.code(), src_res.message());
      }
      ingest_available_frames();

      const auto decision =
          trigger_arbiter_.evaluate_trigger(src_res.value(), clock_.now_ms());
      if (decision == trigger::ArbiterDecision::kDebounced) {
        return ipc::Response::OkJson(build_status_json());
      }

      const auto tr =
          state_machine_.dispatch(recording::Event::Trigger(cmd.arg1));
      auto act_st = execute_actions(tr.actions);
      if (!act_st.ok()) {
        // Storage write failure transitions to Fault
        if (act_st.code() == ErrorCode::kIoError) {
          const auto fault_tr =
              state_machine_.dispatch(recording::Event::StorageFull());
          (void)execute_actions(fault_tr.actions);
        }
        return ipc::Response::Error(act_st.code(), act_st.message());
      }
      return ipc::Response::OkJson(build_status_json());
    }

    case ipc::CommandType::kStop: {
      const auto cur_state = state_machine_.state();
      if (cur_state != recording::State::kRecording &&
          cur_state != recording::State::kPostRecord) {
        return ipc::Response::Error(ErrorCode::kInvalidState,
                                    "Not currently recording");
      }
      auto src_res = trigger::parse_trigger_source(cmd.arg1);
      if (!src_res.ok()) {
        return ipc::Response::Error(src_res.code(), src_res.message());
      }
      const auto decision =
          trigger_arbiter_.evaluate_stop(src_res.value(), clock_.now_ms());
      if (decision == trigger::ArbiterDecision::kRejectedLowerPriority) {
        return ipc::Response::Error(
            ErrorCode::kInvalidState,
            "STOP rejected: lower priority than active trigger source");
      }
      if (cur_state == recording::State::kRecording) {
        const auto tr =
            state_machine_.dispatch(recording::Event::Stop(cmd.arg1));
        auto act_st = execute_actions(tr.actions);
        if (!act_st.ok()) {
          return ipc::Response::Error(act_st.code(), act_st.message());
        }
      }
      return ipc::Response::OkJson(build_status_json());
    }

    case ipc::CommandType::kTick: {
      if (!config_.sim_mode || fake_clock_ == nullptr) {
        return ipc::Response::Error(ErrorCode::kNotSupported,
                                    "TICK is only supported in --sim mode");
      }
      uint64_t remaining_ms = cmd.numeric_arg;
      // Step in intervals <= 1000 ms (or segment_ms) so frames align with
      // segment cuts accurately.
      const uint64_t step_limit =
          std::min<uint64_t>(1000ULL, std::max<uint64_t>(1ULL, config_.segment_ms));
      while (remaining_ms > 0) {
        const uint64_t step = std::min<uint64_t>(remaining_ms, step_limit);
        fake_clock_->advance(step);
        remaining_ms -= step;

        ingest_available_frames();
        watchdog_supervisor_.heartbeat("capture");
        watchdog_supervisor_.heartbeat("writer");
        watchdog_supervisor_.heartbeat("controller");
        (void)watchdog_supervisor_.check_and_kick();

        const auto tr = state_machine_.dispatch(recording::Event::Tick(step));
        auto act_st = execute_actions(tr.actions);
        if (!act_st.ok()) {
          if (act_st.code() == ErrorCode::kIoError) {
            const auto fault_tr =
                state_machine_.dispatch(recording::Event::StorageFull());
            (void)execute_actions(fault_tr.actions);
          }
          return ipc::Response::Error(act_st.code(), act_st.message());
        }
      }
      return ipc::Response::OkJson(build_status_json());
    }

    case ipc::CommandType::kStatus: {
      return ipc::Response::OkJson(build_status_json());
    }

    case ipc::CommandType::kVerify: {
      auto ev_st = evidence_store_.verify_all();
      if (!ev_st.ok()) {
        return ipc::Response::Error(ev_st.code(), ev_st.message());
      }
      auto aud_st = audit_log_.verify();
      if (!aud_st.ok()) {
        return ipc::Response::Error(aud_st.code(), aud_st.message());
      }
      return ipc::Response::OkJson("{\"verified\":true,\"status\":\"OK\"}");
    }

    case ipc::CommandType::kNet: {
      net_state_ = cmd.arg1;
      (void)audit_log_.append("NET_STATE", "net=" + net_state_);
      return ipc::Response::OkJson(build_status_json());
    }

    case ipc::CommandType::kUploadAck: {
      auto st = evidence_store_.mark_segment_uploaded(
          cmd.arg1, static_cast<uint32_t>(cmd.numeric_arg));
      if (!st.ok()) {
        return ipc::Response::Error(st.code(), st.message());
      }
      (void)audit_log_.append(
          "UPLOAD_ACK",
          "incident=" + cmd.arg1 + ",seg=" + std::to_string(cmd.numeric_arg));
      return ipc::Response::OkJson(build_status_json());
    }

    case ipc::CommandType::kOtaStage: {
      auto st = ota_manager_.stage(cmd.arg1, cmd.arg2);
      if (!st.ok()) {
        return ipc::Response::Error(st.code(), st.message());
      }
      (void)audit_log_.append("OTA_STAGE",
                              "pending=" + ota_manager_.pending_slot() +
                                  ",sha256=" + cmd.arg2);
      std::ostringstream oss;
      oss << "{\"slot\":\"" << ota_manager_.active_slot()
          << "\",\"pending_slot\":\"" << ota_manager_.pending_slot()
          << "\",\"boot_attempts_left\":" << ota_manager_.boot_attempts_left()
          << "}";
      return ipc::Response::OkJson(oss.str());
    }

    case ipc::CommandType::kOtaReboot: {
      if (!config_.sim_mode) {
        return ipc::Response::Error(
            ErrorCode::kNotSupported,
            "OTA_REBOOT is only supported in --sim mode");
      }
      auto st = ota_manager_.on_boot();
      if (!st.ok()) {
        return ipc::Response::Error(st.code(), st.message());
      }
      (void)audit_log_.append(
          "OTA_REBOOT",
          "slot=" + ota_manager_.active_slot() + ",attempts_left=" +
              std::to_string(ota_manager_.boot_attempts_left()));
      std::ostringstream oss;
      oss << "{\"slot\":\"" << ota_manager_.active_slot()
          << "\",\"pending_slot\":\"" << ota_manager_.pending_slot()
          << "\",\"boot_attempts_left\":" << ota_manager_.boot_attempts_left()
          << ",\"marked_good\":"
          << (ota_manager_.is_marked_good() ? "true" : "false") << "}";
      return ipc::Response::OkJson(oss.str());
    }

    case ipc::CommandType::kOtaMarkGood: {
      auto st = ota_manager_.mark_good();
      if (!st.ok()) {
        return ipc::Response::Error(st.code(), st.message());
      }
      (void)audit_log_.append("OTA_MARK_GOOD",
                              "slot=" + ota_manager_.active_slot());
      std::ostringstream oss;
      oss << "{\"slot\":\"" << ota_manager_.active_slot()
          << "\",\"marked_good\":true}";
      return ipc::Response::OkJson(oss.str());
    }

    case ipc::CommandType::kQuit: {
      lock.unlock();
      auto st = shutdown();
      lock.lock();
      if (!st.ok()) {
        return ipc::Response::Error(st.code(), st.message());
      }
      return ipc::Response::OkJson(build_status_json());
    }
  }

  return ipc::Response::Error(ErrorCode::kInvalidArgument, "Unknown command");
}

std::string DeviceController::handle_line(std::string_view line) {
  auto parsed = ipc::CommandProtocol::parse_line(line);
  if (!parsed.ok()) {
    return ipc::CommandProtocol::format_response(
        ipc::Response::Error(parsed.code(), parsed.message()));
  }
  const auto resp = handle(parsed.value());
  return ipc::CommandProtocol::format_response(resp);
}

}  // namespace aegis::app
