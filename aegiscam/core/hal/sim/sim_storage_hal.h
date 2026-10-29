#ifndef AEGIS_HAL_SIM_SIM_STORAGE_HAL_H_
#define AEGIS_HAL_SIM_SIM_STORAGE_HAL_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "aegis/hal/i_storage_hal.h"

namespace aegis::hal::sim {

class SimStorageHal final : public IStorageHal {
 public:
  explicit SimStorageHal(std::filesystem::path root_dir);

  Status write_file_atomic(std::string_view rel_path, const uint8_t* data,
                           std::size_t len) override;

  Status append_line(std::string_view rel_path,
                     std::string_view line) override;

  Result<std::vector<uint8_t>> read_file(std::string_view rel_path) override;

  bool exists(std::string_view rel_path) const override;

  Result<std::vector<std::string>> list_dirs(
      std::string_view rel_dir) const override;

  Result<std::vector<std::string>> list_files(
      std::string_view rel_dir) const override;

  Status remove_file(std::string_view rel_path) override;

  Status mkdir_p(std::string_view rel_dir) override;

  std::string root_dir() const override { return root_dir_.string(); }

  // Fault injection controls for deterministic testing
  void set_fail_next_writes(uint32_t count) noexcept;
  void set_disk_full(bool full) noexcept;
  bool is_disk_full() const noexcept;

 private:
  Status check_fault_locked();
  std::filesystem::path resolve(std::string_view rel_path) const;

  std::filesystem::path root_dir_;
  mutable std::mutex mutex_;
  uint32_t fail_next_writes_{0};
  bool disk_full_{false};
  uint64_t tmp_counter_{0};
};

}  // namespace aegis::hal::sim

#endif  // AEGIS_HAL_SIM_SIM_STORAGE_HAL_H_
