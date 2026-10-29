#ifndef AEGIS_UTIL_BOUNDED_QUEUE_H_
#define AEGIS_UTIL_BOUNDED_QUEUE_H_

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace aegis::util {

enum class OverflowPolicy {
  kReject,
  kDropOldest,
};

// Thread-safe Multi-Producer Multi-Consumer (MPMC) bounded queue.
template <typename T>
class BoundedQueue {
 public:
  explicit BoundedQueue(std::size_t capacity,
                        OverflowPolicy policy = OverflowPolicy::kReject)
      : capacity_(capacity > 0 ? capacity : 1), policy_(policy) {}

  BoundedQueue(const BoundedQueue&) = delete;
  BoundedQueue& operator=(const BoundedQueue&) = delete;

  // Non-blocking push obeying OverflowPolicy.
  // Returns false if closed, or if full under kReject policy.
  bool try_push(T item) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_) {
        return false;
      }
      if (queue_.size() >= capacity_) {
        if (policy_ == OverflowPolicy::kReject) {
          ++rejected_count_;
          return false;
        }
        queue_.pop_front();
        ++dropped_count_;
      }
      queue_.push_back(std::move(item));
    }
    cv_not_empty_.notify_one();
    return true;
  }

  // Blocking push when policy is kReject; waits until space is available or
  // queue is closed. Under kDropOldest, behaves like try_push.
  bool push(T item) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (policy_ == OverflowPolicy::kReject) {
        cv_not_full_.wait(
            lock, [this]() { return closed_ || queue_.size() < capacity_; });
        if (closed_) {
          return false;
        }
      } else {
        if (closed_) {
          return false;
        }
        if (queue_.size() >= capacity_) {
          queue_.pop_front();
          ++dropped_count_;
        }
      }
      queue_.push_back(std::move(item));
    }
    cv_not_empty_.notify_one();
    return true;
  }

  // Blocking pop: waits until an element is available or queue is closed and
  // drained. Returns std::nullopt when closed and empty.
  std::optional<T> pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_not_empty_.wait(lock, [this]() { return closed_ || !queue_.empty(); });
    if (queue_.empty()) {
      return std::nullopt;
    }
    T item = std::move(queue_.front());
    queue_.pop_front();
    lock.unlock();
    cv_not_full_.notify_one();
    return item;
  }

  // Pop with timeout. Returns std::nullopt on timeout or closed+empty.
  template <typename Rep, typename Period>
  std::optional<T> pop_for(const std::chrono::duration<Rep, Period>& timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cv_not_empty_.wait_for(
            lock, timeout, [this]() { return closed_ || !queue_.empty(); })) {
      return std::nullopt;
    }
    if (queue_.empty()) {
      return std::nullopt;
    }
    T item = std::move(queue_.front());
    queue_.pop_front();
    lock.unlock();
    cv_not_full_.notify_one();
    return item;
  }

  // Non-blocking pop.
  std::optional<T> try_pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return std::nullopt;
    }
    T item = std::move(queue_.front());
    queue_.pop_front();
    lock.unlock();
    cv_not_full_.notify_one();
    return item;
  }

  void close() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      closed_ = true;
    }
    cv_not_empty_.notify_all();
    cv_not_full_.notify_all();
  }

  bool is_closed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  bool empty() const { return size() == 0; }

  std::size_t capacity() const noexcept { return capacity_; }

  std::size_t dropped_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_count_;
  }

  std::size_t rejected_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rejected_count_;
  }

 private:
  const std::size_t capacity_;
  const OverflowPolicy policy_;
  mutable std::mutex mutex_;
  std::condition_variable cv_not_empty_;
  std::condition_variable cv_not_full_;
  std::deque<T> queue_;
  bool closed_{false};
  std::size_t dropped_count_{0};
  std::size_t rejected_count_{0};
};

}  // namespace aegis::util

#endif  // AEGIS_UTIL_BOUNDED_QUEUE_H_
