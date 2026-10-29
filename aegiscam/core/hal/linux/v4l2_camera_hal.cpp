#include "hal/linux/v4l2_camera_hal.h"

#ifdef __linux__
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace aegis::hal::linux_hal {

V4l2CameraHal::V4l2CameraHal(std::string device_node,
                             const common::IClock& clock)
    : device_node_(std::move(device_node)), clock_(clock) {}

V4l2CameraHal::~V4l2CameraHal() { close_device(); }

Status V4l2CameraHal::open_device() {
#ifdef __linux__
  if (fd_ >= 0) {
    return Status::Ok();
  }
  fd_ = ::open(device_node_.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (fd_ < 0) {
    return Status::Error(ErrorCode::kIoError,
                         std::string("open v4l2 device failed: ") +
                             std::strerror(errno));
  }
  struct v4l2_capability cap {};
  if (::ioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
    const int err = errno;
    close_device();
    return Status::Error(ErrorCode::kIoError,
                         std::string("VIDIOC_QUERYCAP failed: ") +
                             std::strerror(err));
  }
  return Status::Ok();
#else
  return Status::Error(ErrorCode::kNotSupported,
                       "V4l2CameraHal requires Linux");
#endif
}

void V4l2CameraHal::close_device() noexcept {
#ifdef __linux__
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
#endif
}

Result<evidence::Frame> V4l2CameraHal::read_frame() {
#ifdef __linux__
  if (fd_ < 0) {
    return Result<evidence::Frame>::Error(ErrorCode::kInvalidState,
                                          "V4L2 device is not open");
  }
  struct v4l2_buffer buf {};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  if (::ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
    if (errno == EAGAIN) {
      return Result<evidence::Frame>::Error(ErrorCode::kEmpty, "EAGAIN");
    }
    return Result<evidence::Frame>::Error(
        ErrorCode::kIoError,
        std::string("VIDIOC_DQBUF failed: ") + std::strerror(errno));
  }
  evidence::Frame frame;
  frame.seq = seq_++;
  frame.ts_ms = clock_.now_ms();
  frame.keyframe = (buf.flags & V4L2_BUF_FLAG_KEYFRAME) != 0;
  (void)::ioctl(fd_, VIDIOC_QBUF, &buf);
  return Result<evidence::Frame>::Ok(std::move(frame));
#else
  (void)clock_;
  (void)seq_;
  return Result<evidence::Frame>::Error(ErrorCode::kNotSupported,
                                        "V4l2CameraHal requires Linux");
#endif
}

}  // namespace aegis::hal::linux_hal
