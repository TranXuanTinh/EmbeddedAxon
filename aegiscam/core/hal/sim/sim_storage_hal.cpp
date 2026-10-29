#include "hal/sim/sim_storage_hal.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace aegis::hal::sim {

SimStorageHal::SimStorageHal(std::filesystem::path root_dir)
    : root_dir_(std::move(root_dir)) {
  std::error_code ec;
  std::filesystem::create_directories(root_dir_, ec);
}

std::filesystem::path SimStorageHal::resolve(std::string_view rel_path) const {
  if (rel_path.empty()) {
    return root_dir_;
  }
  return root_dir_ / std::filesystem::path(rel_path);
}

Status SimStorageHal::check_fault_locked() {
  if (disk_full_) {
    return Status::Error(ErrorCode::kIoError, "Simulated disk full (ENOSPC)");
  }
  if (fail_next_writes_ > 0) {
    --fail_next_writes_;
    return Status::Error(ErrorCode::kIoError, "Simulated storage write fault");
  }
  return Status::Ok();
}

void SimStorageHal::set_fail_next_writes(uint32_t count) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  fail_next_writes_ = count;
}

void SimStorageHal::set_disk_full(bool full) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  disk_full_ = full;
}

bool SimStorageHal::is_disk_full() const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  return disk_full_;
}

Status SimStorageHal::write_file_atomic(std::string_view rel_path,
                                        const uint8_t* data, std::size_t len) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto fault = check_fault_locked();
  if (!fault.ok()) {
    return fault;
  }

  const auto target = resolve(rel_path);
  std::error_code ec;
  if (target.has_parent_path()) {
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) {
      return Status::Error(ErrorCode::kIoError,
                           "mkdir failed: " + ec.message());
    }
  }

  const auto tmp_path = std::filesystem::path(
      target.string() + ".tmp." + std::to_string(++tmp_counter_));
  {
    std::ofstream ofs(tmp_path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      return Status::Error(ErrorCode::kIoError,
                           "Failed to open tmp file: " + tmp_path.string());
    }
    if (len > 0 && data != nullptr) {
      ofs.write(reinterpret_cast<const char*>(data),
                static_cast<std::streamsize>(len));
    }
    ofs.flush();
    if (!ofs) {
      ofs.close();
      std::filesystem::remove(tmp_path, ec);
      return Status::Error(ErrorCode::kIoError,
                           "Failed writing tmp file: " + tmp_path.string());
    }
  }

  std::filesystem::rename(tmp_path, target, ec);
  if (ec) {
    // On Windows std::filesystem::rename may fail if destination exists;
    // remove destination and retry rename.
    std::filesystem::remove(target, ec);
    ec.clear();
    std::filesystem::rename(tmp_path, target, ec);
    if (ec) {
      std::filesystem::remove(tmp_path, ec);
      return Status::Error(ErrorCode::kIoError,
                           "Atomic rename failed: " + ec.message());
    }
  }
  return Status::Ok();
}

Status SimStorageHal::append_line(std::string_view rel_path,
                                  std::string_view line) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto fault = check_fault_locked();
  if (!fault.ok()) {
    return fault;
  }

  const auto target = resolve(rel_path);
  std::error_code ec;
  if (target.has_parent_path()) {
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) {
      return Status::Error(ErrorCode::kIoError,
                           "mkdir failed: " + ec.message());
    }
  }

  std::ofstream ofs(target, std::ios::binary | std::ios::app);
  if (!ofs) {
    return Status::Error(ErrorCode::kIoError,
                         "Failed to open file for append: " + target.string());
  }
  ofs.write(line.data(), static_cast<std::streamsize>(line.size()));
  ofs.put('\n');
  ofs.flush();
  if (!ofs) {
    return Status::Error(ErrorCode::kIoError,
                         "Failed to flush appended line: " + target.string());
  }
  return Status::Ok();
}

Result<std::vector<uint8_t>> SimStorageHal::read_file(
    std::string_view rel_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto target = resolve(rel_path);
  std::ifstream ifs(target, std::ios::binary);
  if (!ifs) {
    return Result<std::vector<uint8_t>>::Error(
        ErrorCode::kIoError, "File not found: " + target.string());
  }
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
  if (ifs.bad()) {
    return Result<std::vector<uint8_t>>::Error(
        ErrorCode::kIoError, "Failed reading file: " + target.string());
  }
  return Result<std::vector<uint8_t>>::Ok(std::move(data));
}

bool SimStorageHal::exists(std::string_view rel_path) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::error_code ec;
  return std::filesystem::exists(resolve(rel_path), ec);
}

Result<std::vector<std::string>> SimStorageHal::list_dirs(
    std::string_view rel_dir) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto target = resolve(rel_dir);
  std::error_code ec;
  if (!std::filesystem::exists(target, ec)) {
    return Result<std::vector<std::string>>::Ok({});
  }
  std::vector<std::string> dirs;
  for (const auto& entry : std::filesystem::directory_iterator(target, ec)) {
    if (entry.is_directory()) {
      dirs.push_back(entry.path().filename().string());
    }
  }
  if (ec) {
    return Result<std::vector<std::string>>::Error(
        ErrorCode::kIoError, "Directory iteration failed: " + ec.message());
  }
  return Result<std::vector<std::string>>::Ok(std::move(dirs));
}

Result<std::vector<std::string>> SimStorageHal::list_files(
    std::string_view rel_dir) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto target = resolve(rel_dir);
  std::error_code ec;
  if (!std::filesystem::exists(target, ec)) {
    return Result<std::vector<std::string>>::Ok({});
  }
  std::vector<std::string> files;
  for (const auto& entry : std::filesystem::directory_iterator(target, ec)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path().filename().string());
    }
  }
  if (ec) {
    return Result<std::vector<std::string>>::Error(
        ErrorCode::kIoError, "Directory iteration failed: " + ec.message());
  }
  return Result<std::vector<std::string>>::Ok(std::move(files));
}

Status SimStorageHal::remove_file(std::string_view rel_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::error_code ec;
  std::filesystem::remove(resolve(rel_path), ec);
  if (ec) {
    return Status::Error(ErrorCode::kIoError, "remove failed: " + ec.message());
  }
  return Status::Ok();
}

Status SimStorageHal::mkdir_p(std::string_view rel_dir) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto fault = check_fault_locked();
  if (!fault.ok()) {
    return fault;
  }
  std::error_code ec;
  std::filesystem::create_directories(resolve(rel_dir), ec);
  if (ec) {
    return Status::Error(ErrorCode::kIoError, "mkdir_p failed: " + ec.message());
  }
  return Status::Ok();
}

}  // namespace aegis::hal::sim
