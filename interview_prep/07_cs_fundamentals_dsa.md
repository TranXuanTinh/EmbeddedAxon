# 07. CS Fundamentals, OOD & Embedded Coding Problems

> **Interviewer đang đánh giá gì?** JD yêu cầu: *"Strong foundation in computer science fundamentals, including data structures, algorithms, run-time analysis, object-oriented design, and code quality verification."*
>
> Ở vòng Coding cho vị trí Staff/Lead Embedded Engineer, đề bài hiếm khi là quy hoạch động (DP) đánh đố kiểu thi đấu, mà tập trung vào **cấu trúc dữ liệu hệ thống có giới hạn bộ nhớ (bounded memory), xử lý luồng sự kiện theo thời gian (streams/timestamps), đồng bộ hoá, và thiết kế hướng đối tượng sạch (SOLID, State Machine, HAL)**.
> Đặc biệt: khi phân tích độ phức tạp (run-time analysis), đừng chỉ nói "trung bình $O(1)$" — hãy luôn nói thêm **worst-case latency**, **cache locality**, và **heap allocation overhead**.

Liên hệ dự án:
- AegisCam: [`spsc_ring_buffer.h`](../aegiscam/core/include/aegis/util/spsc_ring_buffer.h), [`bounded_queue.h`](../aegiscam/core/include/aegis/util/bounded_queue.h), [`pre_event_buffer.h`](../aegiscam/core/include/aegis/evidence/pre_event_buffer.h), [`trigger_arbiter.h`](../aegiscam/core/include/aegis/trigger/trigger_arbiter.h), [`recording_state_machine.h`](../aegiscam/core/include/aegis/recording/recording_state_machine.h), [`hash_chain.h`](../aegiscam/core/include/aegis/crypto/hash_chain.h), [`uploader.go` (Token Bucket)](../aegiscam/netagent/internal/uploader/uploader.go)

---

## 1. Run-Time & Space Analysis dưới góc nhìn Embedded

Tại sao $O(1)$ trung bình trên lý thuyết có thể **không đạt yêu cầu** trên thiết bị nhúng thời gian thực?

| Cấu trúc chuẩn | Độ phức tạp lý thuyết | Bẫy trên hệ thống nhúng / Real-Time | Thay thế chuẩn Embedded |
|---|---|---|---|
| `std::unordered_map` | $O(1)$ average lookup/insert | 1) Mỗi node cấp phát heap riêng (pointer chasing → cache miss).<br>2) Khi vượt load factor, **rehash** là $O(N)$ làm spike latency.<br>3) Hash collision xấu nhất là $O(N)$. | Fixed-capacity open-addressing table (linear/robin-hood probing) hoặc `std::array` phẳng nếu số key nhỏ |
| `std::vector::push_back` | Amortized $O(1)$ | Khi hết `capacity()`, cấp phát vùng mới gấp đôi + copy toàn bộ $O(N)$ phần tử | `reserve(max_n)` lúc khởi tạo hoặc dùng `SpscRingBuffer<T, Cap>` cố định |
| `std::list` / `std::map` | $O(1)$ splice / $O(\log N)$ | Mỗi phần tử tốn thêm 16–24 byte con trỏ + header allocator; nằm rải rác trong RAM → **L1/L2 cache miss** liên tục | Mảng liên tục (`std::vector` đã `reserve` + `std::lower_bound`, hoặc intrusive list trong pool tĩnh) |
| Đệ quy (Recursion) | $O(\text{depth})$ stack | Stack của thread RT/RTOS rất nhỏ (vài KB – vài chục KB) → nguy cơ **stack overflow** không kiểm soát được | Khử đệ quy sang vòng lặp (iterative) với hàng đợi/ngăn xếp có giới hạn kích thước |

---

## 2. Object-Oriented Design (OOD) & SOLID cho Embedded Systems

### 2.1 Năm nguyên tắc SOLID áp dụng thực tế trong AegisCam
1. **S — Single Responsibility Principle:**
   - `RecordingStateMachine` chỉ quyết định chuyển trạng thái.
   - `SegmentWriter` chỉ lo ghi file nguyên tử (`tmp` → `fsync` → `rename`).
   - `TriggerArbiter` chỉ lo chống rung (debounce) và phân xử độ ưu tiên nguồn kích hoạt.
2. **O — Open/Closed Principle:**
   - Thêm một nguồn trigger mới hoặc một loại storage backend mới không phải sửa logic của `DeviceController`.
3. **L — Liskov Substitution Principle:**
   - `SimStorageHal` và `PosixStorageHal` tuân thủ đúng cùng một hợp đồng (`Result<void>` trả về `ErrorCode::IoError` khi đĩa đầy, đảm bảo tính nguyên tử của `write_file_atomic`).
4. **I — Interface Segregation Principle:**
   - Thay vì một `IHardwareHal` khổng lồ chứa 50 hàm, tách thành 5 interface nhỏ: `ICameraHal`, `IStorageHal`, `IWatchdogHal`, `IKeyStoreHal`, `IBootControlHal`. `AbSlotManager` chỉ cần nhận `IBootControlHal` + `IStorageHal`.
5. **D — Dependency Inversion Principle:**
   - Module bậc cao (`DeviceController`) phụ thuộc vào abstraction (`ICameraHal`, `IClock`), không phụ thuộc vào chi tiết bậc thấp (`V4L2`, `sysfs`).

### 2.2 Ba Design Patterns quan trọng nhất trong Embedded C++
1. **Table-Driven / Pure Functional State Machine (`RecordingStateMachine`):**
   - Tránh mẫu *GoF State Pattern* cổ điển nếu nó cấp phát `new ConcreteState()` mỗi lần chuyển trạng thái. Dùng `enum class State`, `struct Event`, và hàm thuần `on_event(State, Event) -> TransitionResult{new_state, vector<Action>}`. Dễ đọc trên 1 màn hình, zero heap allocation, kiểm tra tính đầy đủ của bảng chuyển trạng thái ngay lúc compile (`switch` không có `default` + `-Wswitch-enum`).
2. **Strategy / HAL via Dependency Injection:**
   - Dùng đa hình (`virtual`) ở ranh giới phần cứng/I/O (tần suất gọi thấp: 30 lần/giây cho frame hoặc vài lần/giây cho file), hoặc dùng template policy (`ClockT`) nếu nằm ở vòng lặp nanosecond.
3. **Observer / Bounded Pub-Sub (`EventBus` trong MultiCamera):**
   - Tách rời producer và consumer bằng hàng đợi có giới hạn (`BoundedQueue` với chính sách `DropOldest` cho telemetry hoặc `RejectNew` cho lệnh điều khiển).

---

## 3. 12 Bài toán Coding trọng tâm kèm lời giải C++17 chuẩn Production

### Bài 1: Lock-Free SPSC Ring Buffer (Đã hỏi rất nhiều cho vị trí Camera/Audio/Telemetry)
**Đề bài:** Thiết kế ring buffer cho 1 producer (capture thread) và 1 consumer (writer thread) không dùng mutex, không cấp phát động lúc chạy, tránh false sharing.
- **Độ phức tạp:** `try_push` và `try_pop` đều là **$O(1)$ deterministic**, Space **$O(\text{Capacity})$ tĩnh**.
- **Điểm ăn tiền Staff:** giải thích tại sao `Capacity` là lũy thừa của 2 (`index & (Cap - 1)` nhanh hơn `%`), tại sao dùng `acquire`/`release`, và `alignas(64)`.

```cpp
#include <array>
#include <atomic>
#include <cstddef>
#include <utility>

template <typename T, std::size_t CapacityPow2>
class SpscRingBuffer {
  static_assert(CapacityPow2 >= 2 && (CapacityPow2 & (CapacityPow2 - 1)) == 0,
                "CapacityPow2 must be a power of 2");
  static constexpr std::size_t kMask = CapacityPow2 - 1;

 public:
  bool try_push(T item) noexcept {
    const std::size_t w = write_idx_.load(std::memory_order_relaxed);
    const std::size_t r = read_idx_.load(std::memory_order_acquire);
    if (w - r == CapacityPow2) return false; // Full
    slots_[w & kMask] = std::move(item);
    write_idx_.store(w + 1, std::memory_order_release);
    return true;
  }

  bool try_pop(T& out) noexcept {
    const std::size_t r = read_idx_.load(std::memory_order_relaxed);
    const std::size_t w = write_idx_.load(std::memory_order_acquire);
    if (w == r) return false; // Empty
    out = std::move(slots_[r & kMask]);
    read_idx_.store(r + 1, std::memory_order_release);
    return true;
  }

 private:
  alignas(64) std::atomic<std::size_t> write_idx_{0};
  alignas(64) std::atomic<std::size_t> read_idx_{0};
  std::array<T, CapacityPow2> slots_{};
};
```

---

### Bài 2: Thread-Safe Bounded Blocking Queue với Drop Policy (`DropOldest` vs `Reject`)
**Đề bài:** Hiện thực hàng đợi MPMC có giới hạn dung lượng `capacity`, hỗ trợ `close()` để đánh thức tất cả các thread đang chờ khi hệ thống tắt (graceful shutdown).
- **Độ phức tạp:** $O(1)$ mỗi thao tác push/pop.

```cpp
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

enum class OverflowPolicy { RejectNew, DropOldest };

template <typename T>
class BoundedQueue {
 public:
  explicit BoundedQueue(size_t cap, OverflowPolicy p = OverflowPolicy::RejectNew)
      : cap_(cap), policy_(p) {}

  bool try_push(T item) {
    std::lock_guard<std::mutex> lk(mu_);
    if (closed_) return false;
    if (q_.size() >= cap_) {
      if (policy_ == OverflowPolicy::RejectNew) return false;
      q_.pop_front(); // DropOldest
    }
    q_.push_back(std::move(item));
    cv_.notify_one();
    return true;
  }

  std::optional<T> pop_blocking() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return closed_ || !q_.empty(); });
    if (q_.empty()) return std::nullopt; // Closed and drained
    T val = std::move(q_.front());
    q_.pop_front();
    return val;
  }

  void close() {
    std::lock_guard<std::mutex> lk(mu_);
    closed_ = true;
    cv_.notify_all();
  }

 private:
  size_t cap_;
  OverflowPolicy policy_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<T> q_;
  bool closed_ = false;
};
```

---

### Bài 3: Pre-Event Video Buffer với Keyframe Alignment (Bài toán thực tế của Body Cam)
**Đề bài:** Cho luồng frame có `(seq, ts_ms, is_keyframe)`. Giữ cửa sổ tối đa `window_ms` (ví dụ 30,000 ms), nhưng khi xả (`drain`) hoặc cắt bớt (`evict`), **frame đầu tiên bắt buộc phải là keyframe (I-frame)** để trình giải mã đọc được.
- **Độ phức tạp:** `push` amortized $O(1)$, `drain` $O(N)$ với $N$ là số frame trong cửa sổ.

```cpp
#include <cstdint>
#include <deque>
#include <vector>

struct VideoFrame {
  uint64_t seq;
  int64_t ts_ms;
  bool keyframe;
};

class KeyframePreEventBuffer {
 public:
  explicit KeyframePreEventBuffer(int64_t window_ms) : window_ms_(window_ms) {}

  void push(VideoFrame f) {
    frames_.push_back(f);
    evict_old(f.ts_ms);
  }

  std::vector<VideoFrame> drain() {
    std::vector<VideoFrame> out;
    bool seen_kf = false;
    for (auto& f : frames_) {
      if (f.keyframe) seen_kf = true;
      if (seen_kf) out.push_back(f);
    }
    frames_.clear();
    return out;
  }

 private:
  void evict_old(int64_t newest_ts) {
    const int64_t cutoff = newest_ts - window_ms_;
    // Tìm keyframe mới nhất nằm tại hoặc trước cutoff để không làm mồ côi các P-frame sau cutoff
    size_t keep_from = 0;
    for (size_t i = 0; i < frames_.size(); ++i) {
      if (frames_[i].ts_ms <= cutoff && frames_[i].keyframe) {
        keep_from = i;
      }
    }
    // Nếu toàn bộ các frame từ một keyframe > cutoff đã đủ giữ cửa sổ, có thể tiến thêm:
    for (size_t i = keep_from + 1; i < frames_.size(); ++i) {
      if (frames_[i].ts_ms <= cutoff && frames_[i].keyframe) keep_from = i;
    }
    while (keep_from > 0) {
      frames_.pop_front();
      --keep_from;
    }
  }

  int64_t window_ms_;
  std::deque<VideoFrame> frames_;
};
```

---

### Bài 4: Trigger Debounce & Priority Arbiter
**Đề bài:** Camera có 4 nguồn kích hoạt: `Remote(1) < Signal(2) < Button(3) < Holster(4)`.
1. Hai tín hiệu `Trigger` từ **cùng một nguồn** trong khoảng `< debounce_ms` (50 ms) bị coi là rung dội (bounce) và bỏ qua.
2. Khi đang ghi hình do nguồn $A$, nếu nguồn $B$ có ưu tiên cao hơn kích hoạt, nâng `active_source` lên $B$.
3. Lệnh `Stop` từ nguồn $S$ **bị từ chối** nếu $\text{priority}(S) < \text{priority}(\text{active\_source})$ (ví dụ: người điều khiển từ xa không được phép tắt camera đang ghi do cảnh sát rút súng khỏi bao!).
- **Độ phức tạp:** Time **$O(1)$**, Space **$O(1)$** (mảng 4 phần tử, không dùng `std::map`).

```cpp
#include <array>
#include <cstdint>
#include <optional>

enum class Source : uint8_t { Remote = 0, Signal = 1, Button = 2, Holster = 3 };

class TriggerArbiter {
 public:
  explicit TriggerArbiter(int64_t debounce_ms = 50) : debounce_ms_(debounce_ms) {
    last_ts_.fill(-1'000'000'000LL);
  }

  bool on_trigger(Source src, int64_t now_ms) {
    const auto idx = static_cast<size_t>(src);
    if (now_ms - last_ts_[idx] < debounce_ms_) return false; // Debounced!
    last_ts_[idx] = now_ms;
    if (!active_.has_value() || static_cast<uint8_t>(src) > static_cast<uint8_t>(*active_)) {
      active_ = src;
    }
    return true;
  }

  bool can_stop(Source src) {
    if (!active_.has_value()) return false;
    if (static_cast<uint8_t>(src) < static_cast<uint8_t>(*active_)) return false;
    active_.reset();
    return true;
  }

 private:
  int64_t debounce_ms_;
  std::array<int64_t, 4> last_ts_{};
  std::optional<Source> active_;
};
```

---

### Bài 5: Token Bucket Rate Limiter (Điều tiết băng thông Upload / Log Rate)
**Đề bài:** Hiện thực bộ giới hạn tốc độ theo thuật toán Token Bucket với tốc độ nạp `rate_bytes_per_sec` và dung lượng bồn tối đa `burst_bytes`. Cho phép kiểm tra `allow(now_ms, bytes)`.
- **Độ phức tạp:** Time **$O(1)$**, Space **$O(1)$**.

```cpp
#include <algorithm>
#include <cstdint>

class TokenBucket {
 public:
  TokenBucket(double rate_per_sec, double burst, int64_t start_ms)
      : rate_per_ms_(rate_per_sec / 1000.0),
        burst_(burst),
        tokens_(burst),
        last_ms_(start_ms) {}

  bool allow(int64_t now_ms, double cost) {
    if (now_ms > last_ms_) {
      const double added = static_cast<double>(now_ms - last_ms_) * rate_per_ms_;
      tokens_ = std::min(burst_, tokens_ + added);
      last_ms_ = now_ms;
    }
    if (tokens_ < cost) return false;
    tokens_ -= cost;
    return true;
  }

 private:
  double rate_per_ms_;
  double burst_;
  double tokens_;
  int64_t last_ms_;
};
```

---

### Bài 6: LRU Cache ($O(1)$ `get` và `put`) — Quản lý DNS Cache / Metadata Cache
**Đề bài:** Thiết kế bộ nhớ đệm LRU có sức chứa `capacity`.
- **Độ phức tạp:** `get` và `put` đều **$O(1)$**. (Ghi chú khi phỏng vấn: trên thiết bị nhúng thực tế, để tránh cấp phát node rời rạc của `std::list`, ta dùng mảng `std::vector<Node>` cấp phát sẵn 1 lần với chỉ số `prev`/`next` kiểu `uint32_t`).

```cpp
#include <list>
#include <optional>
#include <unordered_map>

template <typename K, typename V>
class LruCache {
 public:
  explicit LruCache(size_t cap) : cap_(cap) {}

  std::optional<V> get(const K& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return std::nullopt;
    items_.splice(items_.begin(), items_, it->second); // Move to front in O(1)
    return it->second->second;
  }

  void put(K key, V val) {
    auto it = map_.find(key);
    if (it != map_.end()) {
      it->second->second = std::move(val);
      items_.splice(items_.begin(), items_, it->second);
      return;
    }
    if (items_.size() == cap_) {
      map_.erase(items_.back().first);
      items_.pop_back();
    }
    items_.emplace_front(std::move(key), std::move(val));
    map_[items_.front().first] = items_.begin();
  }

 private:
  size_t cap_;
  std::list<std::pair<K, V>> items_;
  std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> map_;
};
```

---

### Bài 7: Gộp các khoảng thời gian ghi hình chồng lấp (Merge Recording Intervals)
**Đề bài:** Nhiều cảm biến trên xe tuần tra (camera trước, camera sau, radar, còi hụ) gửi về các khoảng thời gian sự kiện `[start_ms, end_ms]`. Hãy gộp các khoảng giao nhau hoặc cách nhau không quá `gap_ms` (post-record Coalescing) để cắt video bằng chứng thành các incident liên tục.
- **Độ phức tạp:** Time **$O(N \log N)$** (do bước sort), Space **$O(1)$** phụ trợ (nếu gộp tại chỗ).

```cpp
#include <algorithm>
#include <cstdint>
#include <vector>

struct Interval { int64_t start; int64_t end; };

std::vector<Interval> merge_incidents(std::vector<Interval> ivs, int64_t gap_ms = 0) {
  if (ivs.empty()) return {};
  std::sort(ivs.begin(), ivs.end(),
            [](const Interval& a, const Interval& b) { return a.start < b.start; });
  std::vector<Interval> merged;
  merged.reserve(ivs.size());
  merged.push_back(ivs[0]);
  for (size_t i = 1; i < ivs.size(); ++i) {
    if (ivs[i].start <= merged.back().end + gap_ms) {
      merged.back().end = std::max(merged.back().end, ivs[i].end);
    } else {
      merged.push_back(ivs[i]);
    }
  }
  return merged;
}
```

---

### Bài 8: Thao tác Bit & Thanh ghi phần cứng (Bit Manipulation / Register Packing)
**Đề bài:** Viết hàm đọc/ghi một trường bit `[lsb, msb]` trong thanh ghi 32-bit, kiểm tra lũy thừa của 2, căn lề địa chỉ bộ nhớ DMA theo bội số của `align` (ví dụ 64 byte), và đổi endianness (network byte order $\leftrightarrow$ host).
- **Độ phức tạp:** **$O(1)$** (1–3 chu kỳ lệnh CPU).

```cpp
#include <cstdint>

// 1. Tạo mask an toàn cho trường bit độ rộng width (1..32) bắt đầu tại bit lsb
constexpr uint32_t bit_mask(unsigned lsb, unsigned width) noexcept {
  return (width >= 32 ? 0xFFFFFFFFu : ((1u << width) - 1u)) << lsb;
}

// 2. Ghi giá trị field_val vào trường [lsb, lsb + width - 1] của thanh ghi reg
constexpr uint32_t reg_set_field(uint32_t reg, unsigned lsb, unsigned width, uint32_t field_val) noexcept {
  const uint32_t m = bit_mask(lsb, width);
  return (reg & ~m) | ((field_val << lsb) & m);
}

// 3. Căn lề kích thước bộ đệm DMA lên bội số của align (align là lũy thừa của 2)
constexpr size_t align_up(size_t n, size_t align) noexcept {
  return (n + (align - 1)) & ~(align - 1);
}
```

---

### Bài 9: Xác minh chuỗi băm bằng chứng (Tamper-Evident Hash Chain Verifier)
**Đề bài:** Cho danh sách các mã băm SHA-256 của từng segment $S_1, S_2, \dots, S_n$ và các giá trị mắt xích lưu trong manifest $C_1, C_2, \dots, C_n$. Quy tắc: $C_0 = 0^{32}$ (32 byte 0), $C_i = \text{SHA256}(C_{i-1} \parallel S_i)$. Hãy viết hàm trả về chỉ số segment đầu tiên bị can thiệp (hoặc `-1` nếu toàn vẹn).
- **Độ phức tạp:** Time **$O(N)$**, Space **$O(1)$**.

```cpp
#include <array>
#include <vector>

using Digest = std::array<uint8_t, 32>;
Digest sha256_concat(const Digest& left, const Digest& right); // Từ aegis::crypto

int find_first_tampered_segment(const std::vector<Digest>& seg_hashes,
                                const std::vector<Digest>& recorded_chain) {
  if (seg_hashes.size() != recorded_chain.size()) return 0;
  Digest prev{}; // 32 zero bytes (genesis)
  for (size_t i = 0; i < seg_hashes.size(); ++i) {
    Digest expected = sha256_concat(prev, seg_hashes[i]);
    // So sánh hằng thời gian (constant-time) để tránh side-channel
    uint8_t diff = 0;
    for (size_t b = 0; b < 32; ++b) diff |= (expected[b] ^ recorded_chain[i][b]);
    if (diff != 0) return static_cast<int>(i);
    prev = expected;
  }
  return -1; // All segments verified OK
}
```

---

### Bài 10: Tìm thứ tự khởi tạo dịch vụ có phụ thuộc & Phát hiện chu trình (Topological Sort / Kahn's Algorithm)
**Đề bài:** Một thiết bị nhúng có $N$ dịch vụ/module (`0..N-1`) với danh sách phụ thuộc `{u, v}` nghĩa là `u` phải khởi tạo xong trước `v` (và tắt sau `v`). Trả về thứ tự khởi tạo hợp lệ, hoặc mảng rỗng nếu có phụ thuộc vòng (circular dependency).
- **Độ phức tạp:** Time **$O(V + E)$**, Space **$O(V + E)$**.

```cpp
#include <queue>
#include <vector>

std::vector<int> build_startup_order(int n, const std::vector<std::pair<int, int>>& deps) {
  std::vector<std::vector<int>> adj(n);
  std::vector<int> in_deg(n, 0);
  for (auto [u, v] : deps) {
    adj[u].push_back(v);
    ++in_deg[v];
  }
  std::queue<int> q;
  for (int i = 0; i < n; ++i) {
    if (in_deg[i] == 0) q.push(i);
  }
  std::vector<int> order;
  order.reserve(n);
  while (!q.empty()) {
    int u = q.front();
    q.pop();
    order.push_back(u);
    for (int v : adj[u]) {
      if (--in_deg[v] == 0) q.push(v);
    }
  }
  return static_cast<int>(order.size()) == n ? order : std::vector<int>{};
}
```

---

### Bài 11: Sliding Window Percentile / Anomaly Detector cho Telemetry Latency
**Đề bài:** Theo dõi độ trễ ghi đĩa của $W$ segment gần nhất để phát hiện ổ flash đang xuống cấp. Nếu khoảng giá trị độ trễ bị chặn trong `[0, 1000] ms`, làm sao tính P99 trong $O(1)$ bộ nhớ?
- **Giải pháp Embedded:** Thay vì giữ hai heap (`std::priority_queue`) tốn cấp phát động, dùng **Fixed-Bucket Histogram** (ví dụ 200 bucket, mỗi bucket 5 ms) kết hợp một ring buffer kích thước $W$ lưu các mẫu gần nhất. Khi thêm mẫu mới và đẩy mẫu cũ ra khỏi ring buffer, tăng bucket mới và giảm bucket cũ trong $O(1)$; quét 200 bucket tìm phân vị 99% tốn đúng 200 phép cộng (hằng số $O(B)$ cực nhỏ và cache-friendly).

---

### Bài 12: Zero-Allocation Line Protocol Parser (`std::string_view`)
**Đề bài:** Phân tích dòng lệnh IPC `"UPLOAD_ACK INC-AEG-0001-1700 42"` thành tên lệnh và các tham số mà **không cấp phát bất kỳ `std::string` hay `std::vector` nào trên heap**, chống mọi lỗi buffer overflow.
- **Độ phức tạp:** Time **$O(\text{length})$**, Heap Allocations = **0**.

```cpp
#include <array>
#include <charconv>
#include <string_view>

struct ParsedTokens {
  std::array<std::string_view, 4> tokens{};
  size_t count = 0;
};

ParsedTokens tokenize_line_no_alloc(std::string_view line) noexcept {
  if (!line.empty() && line.back() == '\n') line.remove_suffix(1);
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  ParsedTokens out{};
  while (!line.empty() && out.count < out.tokens.size()) {
    const size_t sp = line.find(' ');
    if (sp == 0) return {}; // Không chấp nhận 2 dấu cách liên tiếp (strict protocol)
    if (sp == std::string_view::npos) {
      out.tokens[out.count++] = line;
      break;
    }
    out.tokens[out.count++] = line.substr(0, sp);
    line.remove_prefix(sp + 1);
  }
  return out;
}
```

---

## 4. Code Quality Verification — Quy trình 5 lớp kiểm chứng chất lượng code

Khi interviewer hỏi *"How do you verify code quality across a squad?"*, hãy trả lời theo 5 lớp trong Spec §7 của AegisCam:
1. **Compile-time Enforcement:** `-Wall -Wextra -Wpedantic -Werror -Wconversion -Wswitch-enum`, dùng strong types (`enum class`, `Result<T>` với `[[nodiscard]]`).
2. **Automated Formatting & Static Analysis:** `clang-format --dry-run -Werror`, `clang-tidy` (kiểm tra `bugprone-*`, `concurrency-*`, `cppcoreguidelines-*`, `modernize-*`), `go vet` / `staticcheck`.
3. **Dynamic Analysis (Sanitizers):** chạy toàn bộ unit & integration tests dưới `ASan + UBSan` và `TSan` (`go test -race`) trong mỗi PR.
4. **Coverage & Fuzzing Gates:** cổng chặn tự động $\ge 80\%$ line coverage cho các module lõi + libFuzzer chạy trên mọi parser giao thức.
5. **Structured Human Code Review ([`code_review_guidelines.md`](../aegiscam/docs/process/code_review_guidelines.md)):** checklist tập trung vào cái máy khó thấy: thread ownership, thứ tự khoá (lock hierarchy), hành vi khi mất điện/đầy đĩa, và khả năng rollback.
