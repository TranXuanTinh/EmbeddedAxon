#include "hal/linux/linux_watchdog_hal.h"

#ifdef __linux__
#include <fcntl.h>
#include <linux/watchdog.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace aegis::hal::linux_hal {

LinuxWatchdogHal::LinuxWatchdogHal(std::string dev_path)
    : dev_path_(std::move(dev_path)) {}

LinuxWatchdogHal::~LinuxWatchdogHal() { (void)stop(); }

Status LinuxWatchdogHal::start(uint32_t timeout_ms) {
#ifdef __linux__
  if (fd_ >= 0) {
    return Status::Ok();
  }
  fd_ = ::open(dev_path_.c_str(), O_WRONLY | O_CLOEXEC);
  if (fd_ < 0) {
    return Status::Error(ErrorCode::kIoError,
                         std::string("open watchdog failed: ") +
                             std::strerror(errno));
  }
  int timeout_sec = static_cast<int>((timeout_ms + 999U) / 1000U);
  if (timeout_sec < 1) {
    timeout_sec = 1;
  }
  if (::ioctl(fd_, WDIOC_SETTIMEOUT, &timeout_sec) < 0) {
    // Non-fatal on some softdog drivers, continue
  }
  return Status::Ok();
#else
  (void)timeout_ms;
  return Status::Error(ErrorCode::kNotSupported,
                       "LinuxWatchdogHal requires Linux");
#endif
}

Status LinuxWatchdogHal::kick() {
#ifdef __linux__
  if (fd_ < 0) {
    return Status::Error(ErrorCode::kInvalidState, "Watchdog not open");
  }
  int dummy = 0;
  if (::ioctl(fd_, WDIOC_KEEPALIVE, &dummy) < 0) {
    return Status::Error(ErrorCode::kIoError,
                         std::string("WDIOC_KEEPALIVE failed: ") +
                             std::strerror(errno));
  }
  return Status::Ok();
#else
  return Status::Error(ErrorCode::kNotSupported,
                       "LinuxWatchdogHal requires Linux");
#endif
}

Status LinuxWatchdogHal::stop() {
#ifdef __linux__
  if (fd_ >= 0) {
    // Magic close character 'V' disarms the hardware watchdog gracefully
    const char magic = 'V';
    (void)::write(fd_, &magic, 1);
    ::close(fd_);
    fd_ = -1;
  }
  return Status::Ok();
#else
  return Status::Ok();
#endif
}

}  // namespace aegis::hal::linux_hal
