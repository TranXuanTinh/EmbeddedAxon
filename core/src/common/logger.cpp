#include "aegis/common/logger.h"

#include <cstdio>

namespace aegis::common {

namespace {

const char* level_to_str(LogLevel level) noexcept {
  switch (level) {
    case LogLevel::kDebug:
      return "DEBUG";
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kWarn:
      return "WARN";
    case LogLevel::kError:
      return "ERROR";
    case LogLevel::kSilent:
      return "SILENT";
  }
  return "UNKNOWN";
}

}  // namespace

Logger& Logger::instance() noexcept {
  static Logger logger;
  return logger;
}

void Logger::set_level(LogLevel level) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  min_level_ = level;
}

LogLevel Logger::level() const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  return min_level_;
}

void Logger::log(LogLevel level, std::string_view component,
                 std::string_view message) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  if (level < min_level_ || level == LogLevel::kSilent) {
    return;
  }
  std::fprintf(stderr, "[%s][%.*s] %.*s\n", level_to_str(level),
               static_cast<int>(component.size()), component.data(),
               static_cast<int>(message.size()), message.data());
  std::fflush(stderr);
}

void log_debug(std::string_view component, std::string_view message) noexcept {
  Logger::instance().log(LogLevel::kDebug, component, message);
}

void log_info(std::string_view component, std::string_view message) noexcept {
  Logger::instance().log(LogLevel::kInfo, component, message);
}

void log_warn(std::string_view component, std::string_view message) noexcept {
  Logger::instance().log(LogLevel::kWarn, component, message);
}

void log_error(std::string_view component, std::string_view message) noexcept {
  Logger::instance().log(LogLevel::kError, component, message);
}

}  // namespace aegis::common
