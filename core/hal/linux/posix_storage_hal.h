#ifndef AEGIS_HAL_LINUX_POSIX_STORAGE_HAL_H_
#define AEGIS_HAL_LINUX_POSIX_STORAGE_HAL_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "aegis/hal/i_storage_hal.h"

namespace aegis::hal::linux_hal {

// Power-loss safe POSIX storage HAL using open/write/fsync/rename + parent
// directory fsync.
class PosixStorageHal final : public IStorageHal {
 public:
  explicit PosixStorageHal(std::filesystem::path root_dir);

  Status write_file_atomic(std::string_view rel_path, const uint8_t* data,
                           std::size_t len) override;

  Status append_line(std::string_view rel_path,
                     std::string_view line) override;

  Result<std::vector<uint8_t>> read_file(std::string_view rel_path) override;

  bool exists(std::string_view rel_path) const override;

  Result<std::vector<std::string>> list_dirs(
      std::string_view rel_dir) const override;

  Status mkdir_p(std::string_view rel_dir) override;

  std::string root_dir() const override { return root_dir_.string(); }

 private:
  std::filesystem::path resolve(std::string_view rel_path) const;

  std::filesystem::path root_dir_;
  mutable std::mutex mutex_;
  uint64_t tmp_seq_{0};
};

}  // namespace aegis::hal::linux_hal

#endif  // AEGIS_HAL_LINUX_POSIX_STORAGE_HAL_H_
