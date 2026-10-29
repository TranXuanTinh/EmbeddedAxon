#include "hal/linux/posix_storage_hal.h"

#ifdef __linux__
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#endif

#include <fstream>
#include <iterator>
#include <system_error>

namespace aegis::hal::linux_hal {

namespace {

#ifdef __linux__
Status write_all_fd(int fd, const uint8_t* buf, std::size_t len) {
  std::size_t written = 0;
  while (written < len) {
    const ssize_t n = ::write(fd, buf + written, len - written);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return Status::Error(ErrorCode::kIoError,
                           std::string("write failed: ") +
                               std::strerror(errno));
    }
    written += static_cast<std::size_t>(n);
  }
  return Status::Ok();
}

void fsync_parent_dir(const std::filesystem::path& file_path) noexcept {
  if (!file_path.has_parent_path()) {
    return;
  }
  const int dir_fd =
      ::open(file_path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (dir_fd >= 0) {
    (void)::fsync(dir_fd);
    ::close(dir_fd);
  }
}
#endif

}  // namespace

PosixStorageHal::PosixStorageHal(std::filesystem::path root_dir)
    : root_dir_(std::move(root_dir)) {
  std::error_code ec;
  std::filesystem::create_directories(root_dir_, ec);
}

std::filesystem::path PosixStorageHal::resolve(
    std::string_view rel_path) const {
  if (rel_path.empty()) {
    return root_dir_;
  }
  return root_dir_ / std::filesystem::path(rel_path);
}

Status PosixStorageHal::write_file_atomic(std::string_view rel_path,
                                          const uint8_t* data,
                                          std::size_t len) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto target = resolve(rel_path);
  std::error_code ec;
  if (target.has_parent_path()) {
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) {
      return Status::Error(ErrorCode::kIoError, ec.message());
    }
  }

#ifdef __linux__
  const std::string tmp_path =
      target.string() + ".tmp." + std::to_string(++tmp_seq_);
  const int fd =
      ::open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);
  if (fd < 0) {
    return Status::Error(ErrorCode::kIoError,
                         std::string("open tmp failed: ") +
                             std::strerror(errno));
  }
  auto st = write_all_fd(fd, data, len);
  if (!st.ok()) {
    ::close(fd);
    ::unlink(tmp_path.c_str());
    return st;
  }
  if (::fsync(fd) < 0) {
    const int err = errno;
    ::close(fd);
    ::unlink(tmp_path.c_str());
    return Status::Error(ErrorCode::kIoError,
                         std::string("fsync tmp failed: ") +
                             std::strerror(err));
  }
  ::close(fd);

  if (::rename(tmp_path.c_str(), target.c_str()) < 0) {
    const int err = errno;
    ::unlink(tmp_path.c_str());
    return Status::Error(ErrorCode::kIoError,
                         std::string("rename failed: ") + std::strerror(err));
  }
  fsync_parent_dir(target);
  return Status::Ok();
#else
  (void)data;
  (void)len;
  return Status::Error(ErrorCode::kNotSupported,
                       "PosixStorageHal requires Linux");
#endif
}

Status PosixStorageHal::append_line(std::string_view rel_path,
                                    std::string_view line) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto target = resolve(rel_path);
  std::error_code ec;
  if (target.has_parent_path()) {
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) {
      return Status::Error(ErrorCode::kIoError, ec.message());
    }
  }

#ifdef __linux__
  const int fd =
      ::open(target.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0640);
  if (fd < 0) {
    return Status::Error(ErrorCode::kIoError,
                         std::string("open append failed: ") +
                             std::strerror(errno));
  }
  std::string buf(line);
  buf.push_back('\n');
  auto st = write_all_fd(fd, reinterpret_cast<const uint8_t*>(buf.data()),
                         buf.size());
  if (!st.ok()) {
    ::close(fd);
    return st;
  }
  if (::fsync(fd) < 0) {
    const int err = errno;
    ::close(fd);
    return Status::Error(ErrorCode::kIoError,
                         std::string("fsync append failed: ") +
                             std::strerror(err));
  }
  ::close(fd);
  return Status::Ok();
#else
  (void)line;
  return Status::Error(ErrorCode::kNotSupported,
                       "PosixStorageHal requires Linux");
#endif
}

Result<std::vector<uint8_t>> PosixStorageHal::read_file(
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
  return Result<std::vector<uint8_t>>::Ok(std::move(data));
}

bool PosixStorageHal::exists(std::string_view rel_path) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::error_code ec;
  return std::filesystem::exists(resolve(rel_path), ec);
}

Result<std::vector<std::string>> PosixStorageHal::list_dirs(
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
  return Result<std::vector<std::string>>::Ok(std::move(dirs));
}

Status PosixStorageHal::mkdir_p(std::string_view rel_dir) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::error_code ec;
  std::filesystem::create_directories(resolve(rel_dir), ec);
  if (ec) {
    return Status::Error(ErrorCode::kIoError, ec.message());
  }
  return Status::Ok();
}

}  // namespace aegis::hal::linux_hal
