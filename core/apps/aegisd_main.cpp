#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "aegis/app/device_controller.h"
#include "aegis/common/clock.h"
#include "hal/sim/sim_boot_control_hal.h"
#include "hal/sim/sim_camera_hal.h"
#include "hal/sim/sim_key_store_hal.h"
#include "hal/sim/sim_storage_hal.h"
#include "hal/sim/sim_watchdog_hal.h"

namespace {

std::atomic<bool> g_signal_received{false};

void signal_handler(int /*signum*/) {
  g_signal_received.store(true, std::memory_order_release);
}

bool parse_u64_arg(const char* str, uint64_t& out) {
  if (str == nullptr || *str == '\0') {
    return false;
  }
  char* end = nullptr;
  const unsigned long long val = std::strtoull(str, &end, 10);
  if (end == str || *end != '\0') {
    return false;
  }
  out = static_cast<uint64_t>(val);
  return true;
}

void print_usage(const char* prog) {
  std::cerr << "Usage: " << prog
            << " --sim --store <dir> [--device-id <id>] [--pre-event-ms <ms>]"
               " [--segment-ms <ms>] [--post-record-ms <ms>] [--fps <fps>]\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  aegis::app::DeviceConfig config;
  std::string store_dir;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--sim") {
      config.sim_mode = true;
    } else if (arg == "--store" && i + 1 < argc) {
      store_dir = argv[++i];
    } else if (arg == "--device-id" && i + 1 < argc) {
      config.device_id = argv[++i];
    } else if (arg == "--pre-event-ms" && i + 1 < argc) {
      if (!parse_u64_arg(argv[++i], config.pre_event_ms)) {
        print_usage(argv[0]);
        return 1;
      }
    } else if (arg == "--segment-ms" && i + 1 < argc) {
      if (!parse_u64_arg(argv[++i], config.segment_ms) ||
          config.segment_ms == 0) {
        print_usage(argv[0]);
        return 1;
      }
    } else if (arg == "--post-record-ms" && i + 1 < argc) {
      if (!parse_u64_arg(argv[++i], config.post_record_ms)) {
        print_usage(argv[0]);
        return 1;
      }
    } else if (arg == "--fps" && i + 1 < argc) {
      uint64_t fps_val = 0;
      if (!parse_u64_arg(argv[++i], fps_val) || fps_val == 0 ||
          fps_val > 240) {
        print_usage(argv[0]);
        return 1;
      }
      config.fps = static_cast<uint32_t>(fps_val);
    } else if (arg == "--help" || arg == "-h") {
      print_usage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown or incomplete argument: " << arg << "\n";
      print_usage(argv[0]);
      return 1;
    }
  }

  if (store_dir.empty()) {
    std::cerr << "Error: --store <dir> is required\n";
    print_usage(argv[0]);
    return 1;
  }

  aegis::common::FakeClock fake_clock(1700000000000ULL);
  aegis::hal::sim::SimCameraHal camera_hal(fake_clock, config.fps, 30);
  aegis::hal::sim::SimStorageHal storage_hal(store_dir);
  aegis::hal::sim::SimWatchdogHal watchdog_hal;
  aegis::hal::sim::SimKeyStoreHal key_store_hal;
  aegis::hal::sim::SimBootControlHal boot_control_hal(storage_hal);

  aegis::app::DeviceController controller(
      config, fake_clock, &fake_clock, camera_hal, storage_hal, watchdog_hal,
      key_store_hal, boot_control_hal);

  controller.set_event_callback([](const std::string& evt_line) {
    std::cout << evt_line << "\n";
    std::cout.flush();
  });

  auto start_st = controller.start();
  if (!start_st.ok()) {
    std::cerr << "Failed to start aegisd: " << start_st.message() << "\n";
    return 1;
  }

  std::string line;
  while (!g_signal_received.load(std::memory_order_acquire) &&
         !controller.quit_requested() && std::getline(std::cin, line)) {
    const std::string resp = controller.handle_line(line);
    std::cout << resp << "\n";
    std::cout.flush();
    if (controller.quit_requested()) {
      break;
    }
  }

  if (!controller.quit_requested()) {
    (void)controller.shutdown();
  }

  return 0;
}
