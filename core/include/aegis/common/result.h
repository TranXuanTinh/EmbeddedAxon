#ifndef AEGIS_COMMON_RESULT_H_
#define AEGIS_COMMON_RESULT_H_

#include <string>
#include <utility>
#include <variant>

namespace aegis::common {

enum class ErrorCode {
  kOk = 0,
  kParseError,
  kInvalidArgument,
  kInvalidState,
  kIoError,
  kIntegrityError,
  kNotSupported,
  kTimeout,
  kFull,
  kEmpty,
};

inline const char* error_code_to_string(ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::kOk:
      return "OK";
    case ErrorCode::kParseError:
      return "PARSE_ERROR";
    case ErrorCode::kInvalidArgument:
      return "INVALID_ARGUMENT";
    case ErrorCode::kInvalidState:
      return "INVALID_STATE";
    case ErrorCode::kIoError:
      return "IO_ERROR";
    case ErrorCode::kIntegrityError:
      return "INTEGRITY_ERROR";
    case ErrorCode::kNotSupported:
      return "NOT_SUPPORTED";
    case ErrorCode::kTimeout:
      return "TIMEOUT";
    case ErrorCode::kFull:
      return "FULL";
    case ErrorCode::kEmpty:
      return "EMPTY";
  }
  return "UNKNOWN_ERROR";
}

class Status {
 public:
  Status() noexcept : code_(ErrorCode::kOk) {}
  Status(ErrorCode code, std::string message)
      : code_(code), message_(std::move(message)) {}

  static Status Ok() noexcept { return Status(); }
  static Status Error(ErrorCode code, std::string message) {
    return Status(code, std::move(message));
  }

  bool ok() const noexcept { return code_ == ErrorCode::kOk; }
  explicit operator bool() const noexcept { return ok(); }

  ErrorCode code() const noexcept { return code_; }
  const std::string& message() const noexcept { return message_; }

 private:
  ErrorCode code_;
  std::string message_;
};

template <typename T>
class Result {
 public:
  Result(T value) : storage_(std::in_place_index<0>, std::move(value)) {}
  Result(Status error) : storage_(std::in_place_index<1>, std::move(error)) {}

  static Result<T> Ok(T value) { return Result<T>(std::move(value)); }
  static Result<T> Error(ErrorCode code, std::string message) {
    return Result<T>(Status::Error(code, std::move(message)));
  }

  bool ok() const noexcept { return storage_.index() == 0; }
  explicit operator bool() const noexcept { return ok(); }

  const T& value() const& { return std::get<0>(storage_); }
  T& value() & { return std::get<0>(storage_); }
  T&& value() && { return std::get<0>(std::move(storage_)); }

  const Status& status() const noexcept {
    static const Status kOkStatus = Status::Ok();
    if (ok()) {
      return kOkStatus;
    }
    return std::get<1>(storage_);
  }

  ErrorCode code() const noexcept {
    return ok() ? ErrorCode::kOk : std::get<1>(storage_).code();
  }

  const std::string& message() const noexcept { return status().message(); }

 private:
  std::variant<T, Status> storage_;
};

}  // namespace aegis::common

namespace aegis {
using common::ErrorCode;
using common::Result;
using common::Status;
using common::error_code_to_string;
}  // namespace aegis

#endif  // AEGIS_COMMON_RESULT_H_
