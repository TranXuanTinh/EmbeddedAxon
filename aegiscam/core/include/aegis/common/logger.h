#ifndef AEGIS_COMMON_LOGGER_H_
#define AEGIS_COMMON_LOGGER_H_

#include <mutex>
#include <string_view>

namespace aegis::common {

enum class LogLevel {
  kDebug = 0,
  kInfo = 1,
  kWarn = 2,
  kError = 3,
  kSilent = 4,
};

class Logger {
 public:
  static Logger& instance() noexcept;

  void set_level(LogLevel level) noexcept;
  LogLevel level() const noexcept;

  void log(LogLevel level, std::string_view component,
           std::string_view message) noexcept;

 private:
  Logger() = default;

  mutable std::mutex mutex_;
  LogLevel min_level_{LogLevel::kWarn};
};

void log_debug(std::string_view component, std::string_view message) noexcept;
void log_info(std::string_view component, std::string_view message) noexcept;
void log_warn(std::string_view component, std::string_view message) noexcept;
void log_error(std::string_view component, std::string_view message) noexcept;

}  // namespace aegis::common

#endif  // AEGIS_COMMON_LOGGER_H_
