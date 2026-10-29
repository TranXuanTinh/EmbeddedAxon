#ifndef AEGIS_HAL_I_STORAGE_HAL_H_
#define AEGIS_HAL_I_STORAGE_HAL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aegis/common/result.h"

namespace aegis::hal {

class IStorageHal {
 public:
  virtual ~IStorageHal() = default;

  // Writes data to a temporary file, fsyncs it, atomically renames to rel_path,
  // and fsyncs the parent directory.
  virtual Status write_file_atomic(std::string_view rel_path,
                                   const uint8_t* data, std::size_t len) = 0;

  Status write_file_atomic(std::string_view rel_path,
                           std::string_view content) {
    return write_file_atomic(
        rel_path, reinterpret_cast<const uint8_t*>(content.data()),
        content.size());
  }

  Status write_file_atomic(std::string_view rel_path,
                           const std::vector<uint8_t>& data) {
    return write_file_atomic(rel_path, data.data(), data.size());
  }

  // Appends a single line (followed by '\n') and fsyncs the file.
  virtual Status append_line(std::string_view rel_path,
                             std::string_view line) = 0;

  virtual Result<std::vector<uint8_t>> read_file(std::string_view rel_path) = 0;

  Result<std::string> read_text(std::string_view rel_path) {
    auto res = read_file(rel_path);
    if (!res.ok()) {
      return Result<std::string>(res.status());
    }
    const auto& bytes = res.value();
    return Result<std::string>::Ok(
        std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
  }

  virtual bool exists(std::string_view rel_path) const = 0;

  virtual Result<std::vector<std::string>> list_dirs(
      std::string_view rel_dir) const = 0;

  virtual Result<std::vector<std::string>> list_files(
      std::string_view rel_dir) const {
    (void)rel_dir;
    return Result<std::vector<std::string>>::Ok({});
  }

  virtual Status remove_file(std::string_view rel_path) {
    (void)rel_path;
    return Status::Ok();
  }

  virtual Status mkdir_p(std::string_view rel_dir) = 0;

  virtual std::string root_dir() const = 0;
};

}  // namespace aegis::hal

#endif  // AEGIS_HAL_I_STORAGE_HAL_H_
