# 02. Modern C++ for Embedded Systems Programming

> **Interviewer đang đánh giá gì?** Không phải bạn thuộc bao nhiêu feature C++17, mà là: (1) bạn **kiểm soát được lifetime, memory, concurrency** trên thiết bị chạy 24/7; (2) bạn biết **cái gì có chi phí ẩn** (heap, exceptions, virtual, `std::function`, locks) trên hot path; (3) bạn đọc được code người khác và chỉ ra UB/race trong review. Ở level Staff, họ sẽ drill: *"why acquire/release và không phải seq_cst?"*, *"điều gì xảy ra nếu head và tail nằm cùng cache line?"*.

Liên hệ dự án:
- AegisCam: [`spsc_ring_buffer.h`](../aegiscam/core/include/aegis/util/spsc_ring_buffer.h), [`bounded_queue.h`](../aegiscam/core/include/aegis/util/bounded_queue.h), [`result.h`](../aegiscam/core/include/aegis/common/result.h), [`clock.h`](../aegiscam/core/include/aegis/common/clock.h), [ADR-0002](../aegiscam/docs/adr/0002-lock-free-pre-event-buffer.md), [threading_and_realtime.md](../aegiscam/docs/architecture/threading_and_realtime.md)
- MultiCamera: bounded `EventBus` ([`../src/service/event_bus.cpp`](../src/service/event_bus.cpp)), HAL interfaces ([`../include/hal/`](../include/hal/))
- eCall: multi-threaded C++11/14 state machine, CAN RX thread, retry timers

---

## 1. RAII & ownership — nền tảng của mọi thứ

**RAII (Resource Acquisition Is Initialization):** mọi resource (memory, fd, mutex, GPIO, DMA buffer, watchdog fd) gắn với lifetime của một object; destructor giải phóng. Đảm bảo exception-safety và early-return safety.

```cpp
// Unique ownership of a POSIX fd — non-copyable, movable.
class UniqueFd {
 public:
  UniqueFd() = default;
  explicit UniqueFd(int fd) noexcept : fd_(fd) {}
  ~UniqueFd() { reset(); }
  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;
  UniqueFd(UniqueFd&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
  UniqueFd& operator=(UniqueFd&& o) noexcept {
    if (this != &o) { reset(); fd_ = std::exchange(o.fd_, -1); }
    return *this;
  }
  int get() const noexcept { return fd_; }
  int release() noexcept { return std::exchange(fd_, -1); }
  void reset(int fd = -1) noexcept {
    if (fd_ >= 0) ::close(fd_);   // close() errors: log, never throw from dtor
    fd_ = fd;
  }
 private:
  int fd_ = -1;
};
```

**Ownership vocabulary (nói trong review):**
| Type | Nghĩa | Ghi chú embedded |
|---|---|---|
| `T` by value | Sở hữu, copy/move | Ưu tiên cho small types |
| `std::unique_ptr<T>` | Sở hữu duy nhất | Zero overhead (với default deleter) |
| `std::shared_ptr<T>` | Sở hữu chung | Atomic refcount (cache-line ping-pong), control block alloc — **tránh trên hot path** |
| `T&` / `const T&` | Mượn, non-null | Lifetime phải dài hơn người mượn |
| `T*` | Mượn, nullable, **không sở hữu** | Quy ước rõ trong guideline |
| `std::weak_ptr<T>` | Quan sát, phá cycle | Observer với lifetime không chắc chắn |

**Rule of 0 / 3 / 5:** nếu class tự quản resource → định nghĩa đủ 5 (dtor, copy ctor/assign, move ctor/assign) hoặc `= delete`. Tốt nhất: **Rule of 0** — compose từ các RAII member.

> **Trap:** `std::thread` destructor gọi `std::terminate()` nếu thread vẫn joinable. Trong AegisCam, `DeviceController` join tất cả thread trong shutdown có thứ tự (stop producer → drain → seal → join), tương tự 4-phase shutdown của MultiCamera ([`../src/app/edge_ai_manager.cpp`](../src/app/edge_ai_manager.cpp)).

---

## 2. Move semantics & value categories

- `std::move` **không di chuyển gì**, chỉ cast sang rvalue (`static_cast<T&&>`). Move thật xảy ra trong move ctor/assign.
- Moved-from object ở trạng thái **valid but unspecified** — chỉ được gán lại hoặc huỷ.
- Move ctor **phải `noexcept`** nếu muốn `std::vector` dùng move khi reallocate (strong exception guarantee) — nếu không nó sẽ copy.
- **Perfect forwarding:** `template<class T> void push(T&& v) { q.emplace_back(std::forward<T>(v)); }` — `T&&` ở đây là forwarding reference.
- **RVO/NRVO:** C++17 bảo đảm copy elision cho prvalue (`return T{...}`). Đừng `return std::move(local);` — nó **chặn** NRVO.

```cpp
// Frame payload moved through the pipeline — no copies of pixel data.
struct Frame {
  uint64_t seq{};
  int64_t ts_ms{};
  bool keyframe{};
  std::vector<uint8_t> payload;   // in prod: handle into a pre-allocated pool
};
void Writer::consume(Frame&& f) { segment_.append(std::move(f.payload)); }
```

> **Embedded nuance:** move một `std::vector` là O(1) (swap 3 pointer), nhưng vector vẫn từng được allocate ở đâu đó. Trên RT path AegisCam, payload nằm trong **slot pre-allocated** của ring buffer; capture thread ghi vào slot, không allocate.

---

## 3. Memory model & atomics (phần bị drill nhiều nhất)

### 3.1 Khái niệm cốt lõi
- **Data race** = hai thread truy cập cùng vị trí nhớ, ít nhất một ghi, không có happens-before → **UB** (không chỉ là "giá trị sai").
- **happens-before** được tạo bởi: mutex lock/unlock, `std::atomic` với release/acquire, thread create/join.
- CPU (ARM là weakly-ordered!) và compiler đều có thể **reorder**. x86 là TSO — nhiều bug ordering chỉ lộ trên ARM (Jetson, i.MX, body cam SoC).

| Memory order | Đảm bảo | Dùng khi |
|---|---|---|
| `relaxed` | Atomicity, không ordering | Counters, statistics (dropped_frames) |
| `acquire` (load) | Không read/write nào sau nó bị đưa lên trước | Consumer đọc index rồi đọc data |
| `release` (store) | Không read/write nào trước nó bị đưa xuống sau | Producer ghi data rồi publish index |
| `acq_rel` | Cả hai, cho RMW | `fetch_add` trong refcount decrement |
| `seq_cst` | Total order toàn cục | Default; cần khi có ≥ 2 biến atomic độc lập phải nhìn thấy cùng thứ tự (Dekker) |

**Release/acquire pairing:** nếu thread B `load(acquire)` đọc được giá trị do thread A `store(release)` ghi, thì mọi write của A trước store **visible** với B sau load.

### 3.2 False sharing
Hai biến khác nhau trên **cùng cache line** (thường 64 B; Apple M-series 128 B) bị hai core ghi → line ping-pong qua coherence protocol (MESI) → throughput giảm 10×.
Fix: `alignas(64)` (hoặc `std::hardware_destructive_interference_size` — C++17, nhưng GCC cảnh báo ABI, nhiều team hard-code 64).

### 3.3 Lock-free SPSC ring buffer (AegisCam pre-event buffer core)

```cpp
// aegis/util/spsc_ring_buffer.h (simplified)
template <typename T, size_t CapacityPow2>
class SpscRingBuffer {
  static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0, "capacity must be power of 2");
  static_assert(std::is_nothrow_move_assignable_v<T>);
 public:
  // Producer thread only.
  bool try_push(T&& v) noexcept {
    const size_t head = head_.load(std::memory_order_relaxed);        // own index
    const size_t tail = tail_cache_;                                    // cached consumer idx
    if (head - tail == CapacityPow2) {                                  // looks full → refresh
      tail_cache_ = tail_.load(std::memory_order_acquire);
      if (head - tail_cache_ == CapacityPow2) return false;             // really full
    }
    slots_[head & kMask] = std::move(v);
    head_.store(head + 1, std::memory_order_release);                   // publish slot
    return true;
  }
  // Consumer thread only.
  std::optional<T> try_pop() noexcept {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_cache_) {
      head_cache_ = head_.load(std::memory_order_acquire);              // see producer's data
      if (tail == head_cache_) return std::nullopt;
    }
    T v = std::move(slots_[tail & kMask]);
    tail_.store(tail + 1, std::memory_order_release);                   // free slot
    return v;
  }
 private:
  static constexpr size_t kMask = CapacityPow2 - 1;
  alignas(64) std::atomic<size_t> head_{0};
  alignas(64) size_t tail_cache_{0};          // producer-local
  alignas(64) std::atomic<size_t> tail_{0};
  alignas(64) size_t head_cache_{0};          // consumer-local
  alignas(64) std::array<T, CapacityPow2> slots_{};
};
```

Giải thích khi được hỏi:
1. **Power-of-2** → `& kMask` thay vì `%` (division chậm trên một số core); indices tăng đơn điệu, `head - tail` vẫn đúng khi `size_t` wrap (unsigned arithmetic modulo 2^64).
2. **Own index load `relaxed`**: chỉ chính thread đó ghi index của nó.
3. **Cached opposite index**: giảm cross-core traffic — chỉ đọc atomic của bên kia khi "có vẻ" full/empty.
4. **Release khi publish / acquire khi observe** — đủ, không cần seq_cst vì chỉ có một cặp đồng bộ.
5. **alignas(64)** chống false sharing giữa head/tail.
6. **Lock-free vs wait-free:** SPSC này wait-free (mỗi op bounded steps).
7. **Drop policy**: pre-event buffer muốn **giữ khung mới nhất** → khi full, consumer (writer) hoặc một cơ chế overwrite-oldest; trong AegisCam, writer thread drain liên tục và pre-event buffer giữ cửa sổ thời gian N ms, drain bắt đầu từ **keyframe** ([ADR-0002](../aegiscam/docs/adr/0002-lock-free-pre-event-buffer.md)).

> **Trap questions:**
> - *"Có thể dùng cho 2 producer không?"* → Không. MPSC cần CAS trên head (hoặc per-producer rings). Nói rõ: **"SPSC is a contract; I'd enforce it with a debug-only thread-id assert."**
> - *"`std::atomic<size_t>::is_always_lock_free`?"* → Kiểm tra bằng `static_assert`; trên ARMv7 64-bit atomics có thể không lock-free.
> - *"Sao không dùng mutex?"* → Capture thread SCHED_FIFO 80: mutex có thể bị giữ bởi writer SCHED_OTHER → **priority inversion**, jitter không bounded. Lock-free loại bỏ blocking. (Nếu bắt buộc mutex: `PTHREAD_PRIO_INHERIT`.)

### 3.4 Mutex/condition variable đúng cách (bounded MPMC queue)

```cpp
template <typename T>
class BoundedQueue {
 public:
  enum class Overflow { DropOldest, Reject };
  BoundedQueue(size_t cap, Overflow p) : cap_(cap), policy_(p) {}
  bool push(T v) {
    std::lock_guard<std::mutex> lk(m_);
    if (closed_) return false;
    if (q_.size() == cap_) {
      if (policy_ == Overflow::Reject) { ++rejected_; return false; }
      q_.pop_front(); ++dropped_;
    }
    q_.push_back(std::move(v));
    cv_.notify_one();          // notify under lock is fine; outside is a micro-opt
    return true;
  }
  std::optional<T> pop() {     // blocks until item or closed
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [&] { return closed_ || !q_.empty(); });   // predicate handles spurious wakeups
    if (q_.empty()) return std::nullopt;                     // closed & drained
    T v = std::move(q_.front()); q_.pop_front();
    return v;
  }
  void close() { { std::lock_guard<std::mutex> lk(m_); closed_ = true; } cv_.notify_all(); }
 private:
  std::mutex m_; std::condition_variable cv_;
  std::deque<T> q_; size_t cap_; Overflow policy_; bool closed_ = false;
  uint64_t dropped_ = 0, rejected_ = 0;
};
```

Điểm review: predicate wait (spurious wakeup + lost wakeup), `close()` để shutdown không treo, **telemetry counters** cho drop — "a drop you can't see is a bug you can't find".

---

## 4. Undefined behavior — danh sách phải thuộc

| UB | Ví dụ | Hậu quả thực tế |
|---|---|---|
| Signed overflow | `int32_t ts_ms` sau 24.8 ngày | Compiler giả định không overflow → xoá check `if (a + b < a)` |
| Out-of-bounds | `buf[len]` | Corrupt stack, exploit |
| Use-after-free / dangling ref | Lambda capture `[&]` chạy trên thread khác sau khi scope kết thúc | Crash ngẫu nhiên sau nhiều giờ |
| Data race | `bool running_` không atomic | Loop vô hạn (compiler hoist load) |
| Strict aliasing | `*(uint32_t*)bytes` để parse packet | Sai giá trị ở `-O2` |
| Misaligned access | Cast `uint8_t*` sang struct | SIGBUS trên một số ARM |
| Uninitialized read | `int state;` | Hành vi khác giữa Debug/Release |
| Shift ≥ width | `1u << 32` | Kết quả khác giữa x86/ARM |
| `nullptr` deref | | Compiler có thể xoá check null sau deref |

**Parse binary safely** (CAN frame, protocol header):
```cpp
uint32_t read_be32(const uint8_t* p) {               // no aliasing, no alignment issue
  return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
}
// or: std::memcpy(&v, p, sizeof v); then ntohl(v);  // memcpy is the blessed type-pun (C++20: std::bit_cast)
```

> **Time types:** dùng `int64_t` ms / `std::chrono::steady_clock` cho duration. **Không dùng wall clock (`system_clock`) để đo timeout** — NTP/GPS có thể nhảy giờ. AegisCam `IClock::now_ms()` là monotonic; `FakeClock` cho test.

---

## 5. Error handling: exceptions vs error codes trên hot path

| | Exceptions | Error codes / `Result<T>` |
|---|---|---|
| Happy-path cost | ~0 (table-based, zero-cost model) | Branch nhỏ |
| Throw cost | Rất cao, **không deterministic** (unwind, allocation của exception object) | Bounded |
| Binary size | Unwind tables (+10–20%) | Nhỏ |
| Visibility | Ẩn trong signature | Hiển thị, `[[nodiscard]]` ép xử lý |
| Safety standards | MISRA/AUTOSAR C++14 hạn chế | Ưu tiên |

**AegisCam policy** ([`result.h`](../aegiscam/core/include/aegis/common/result.h), [threading_and_realtime.md](../aegiscam/docs/architecture/threading_and_realtime.md)): không exception trên capture/writer path; `Result<T>`/`Status` với `ErrorCode` enum (`IO_ERROR`, `INTEGRITY_ERROR`, …) map trực tiếp sang IPC response `ERR <CODE> <msg>`. Exceptions chỉ chấp nhận ở init/config parsing (fail fast trước khi vào RT).

```cpp
enum class ErrorCode { Ok, ParseError, InvalidArgument, InvalidState, IoError, IntegrityError, NotSupported };
template <typename T>
class [[nodiscard]] Result {
 public:
  static Result ok(T v) { return Result(std::move(v)); }
  static Result err(ErrorCode c, std::string msg) { return Result(c, std::move(msg)); }
  bool is_ok() const noexcept { return code_ == ErrorCode::Ok; }
  const T& value() const& { assert(is_ok()); return *value_; }
  ErrorCode code() const noexcept { return code_; }
 private:
  explicit Result(T v) : value_(std::move(v)) {}
  Result(ErrorCode c, std::string m) : code_(c), msg_(std::move(m)) {}
  std::optional<T> value_; ErrorCode code_ = ErrorCode::Ok; std::string msg_;
};
```
> **Trap:** `std::string msg_` allocate khi có lỗi — chấp nhận được vì error path không phải RT path; nếu cần tuyệt đối không alloc thì dùng `const char*` literal.

---

## 6. Allocation & the real-time path

**Quy tắc:** *"No allocation, no locks, no syscalls that can block, no unbounded loops on the RT path after init."*

Lý do: `malloc` có thể lấy lock của allocator, gọi `brk/mmap`, gây page fault → latency spikes ms-level.

Kỹ thuật:
1. **Pre-allocate at init:** pools, rings, `std::vector::reserve`.
2. **`mlockall(MCL_CURRENT | MCL_FUTURE)`** + prefault stack → tránh page fault (file 04).
3. **Object pool / free list** với fixed-size blocks.
4. **`std::pmr`** (C++17): `monotonic_buffer_resource` trên buffer stack/static; `unsynchronized_pool_resource` cho single thread.
5. **Tránh ẩn alloc:** `std::function` (small-buffer chỉ ~16–32 B), `std::string` concat, `std::shared_ptr` creation, `std::map` insert, exceptions, `iostream`.
6. **Kiểm chứng:** hook `operator new` trong test để assert không alloc trong RT section; hoặc `LD_PRELOAD` counter; `perf trace -e syscalls` trên capture thread.

```cpp
// Test helper: fail if the RT section allocates.
thread_local bool g_forbid_alloc = false;
void* operator new(std::size_t n) {
  if (g_forbid_alloc) std::abort();         // in tests only
  if (void* p = std::malloc(n)) return p;
  throw std::bad_alloc{};
}
```

---

## 7. Virtual dispatch, templates, constexpr

**Virtual call cost:** 1 indirect load (vptr) + indirect call; chặn inlining; có thể branch-mispredict. Thực tế vài ns — **không đáng kể ở HAL boundary** (gọi một lần mỗi frame), nhưng đáng kể trong inner loop per-pixel/per-sample.

- HAL interface (`ICameraHal`, `IStorageHal`) dùng virtual → testability > vài ns. **Đây là trade-off có chủ đích** ([ADR-0001](../aegiscam/docs/adr/0001-hal-and-dependency-injection.md)).
- Inner loop (SHA-256 compress) → không virtual, `inline`, templates.
- `final` giúp devirtualization.
- **Static polymorphism** (CRTP, templates) khi interface cố định ở compile time; đổi lại code bloat & compile time.

**constexpr / templates:**
```cpp
// SHA-256 round constants computed/validated at compile time; CRC tables too.
constexpr std::array<uint32_t, 256> make_crc32_table() {
  std::array<uint32_t, 256> t{};
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    t[i] = c;
  }
  return t;
}
inline constexpr auto kCrcTable = make_crc32_table();   // lives in .rodata (flash), no runtime init
```

- `if constexpr` thay SFINAE cho nhiều trường hợp.
- `static_assert` để khoá contract (capacity power-of-2, `sizeof(Header) == 16`, `is_trivially_copyable`).
- `std::variant` + `std::visit` cho event types của state machine (không heap, type-safe).

---

## 8. C interop

- `extern "C"` cho API xuất cho C / dlopen; không ném exception qua biên C (UB) → bắt tại biên, đổi sang error code.
- Struct layout: chỉ chia sẻ **standard-layout, trivially copyable** types; cố định kích thước (`uint32_t`, không `long`), cẩn thận padding/endianness — dùng serialization tường minh cho wire format.
- Ownership qua biên: ai `free`? Dùng `std::unique_ptr<T, decltype(&c_free)>`.
```cpp
using GstElementPtr = std::unique_ptr<GstElement, decltype(&gst_object_unref)>;
```
(MultiCamera dùng pattern ref-count GStreamer tường minh — request pad release, [`../src/hal/jetson/jetson_deepstream_hal.cpp`](../src/hal/jetson/jetson_deepstream_hal.cpp).)
- `volatile` **chỉ** cho memory-mapped I/O registers / signal handler `sig_atomic_t`, **không** dùng để đồng bộ thread.

---

## 9. Câu hỏi phỏng vấn & model answers

### Q1. "Explain acquire/release semantics using your ring buffer."
> "The producer writes the frame into the slot, then stores the new head with release. The consumer loads head with acquire; if it sees the new value, the release-acquire pair guarantees it also sees the slot contents written before. Each side's own index can be loaded relaxed because only that thread writes it. I don't need seq_cst because there's only one synchronization pair — seq_cst would add a full barrier on ARM (`dmb ish`) for no correctness gain. And I align head and tail to separate cache lines to avoid false sharing between the capture and writer cores."

### Q2. "Your 24/7 device's RSS grows 2 MB per day. How do you approach it?"
> "First confirm it's a leak versus fragmentation or caching: track RSS, `/proc/<pid>/smaps_rollup`, and `mallinfo2` over time. If heap-in-use grows, run the soak with ASan's LeakSanitizer or heaptrack on a host build with the sim HAL. If in-use is flat but RSS grows, it's fragmentation — typical when mixing long-lived and short-lived allocations of varied size; fixes are pools for the hot sizes, `malloc_trim`, or tuning `M_ARENA_MAX` since glibc creates per-thread arenas. Structurally, I'd prevent recurrence with a soak test in CI that asserts RSS slope, and a no-allocation-after-init rule on the hot path."

### Q3. "When is `shared_ptr` the wrong choice?"
> "When ownership is actually unique — it hides the design and costs an atomic increment per copy, which on multi-core ARM means cache-line bouncing. Also when it creates cycles, like a controller holding callbacks that capture shared_ptr to the controller. On the RT path I avoid it entirely; for observers I pass raw non-owning pointers with documented lifetime or weak_ptr."

### Q4. "What's wrong with this code?"
```cpp
bool running = true;
void worker() { while (running) { process(); } }
void stop()   { running = false; }
```
> "Data race on a non-atomic bool — UB. In practice the compiler may hoist the load out of the loop and spin forever. Fix: `std::atomic<bool>` with release store / acquire load, or relaxed if no data is published with the flag. Better, if the worker blocks on a queue, use the queue's `close()` so it wakes up instead of polling."

### Q5. "Why `noexcept` on move constructors?"
> "`std::vector` uses `std::move_if_noexcept` during reallocation to keep the strong exception guarantee. If the move ctor may throw, it copies instead — silently turning O(1) moves into deep copies of frame buffers."

### Q6. "How do you guarantee the capture thread never blocks?"
> "By construction and by verification. Construction: it only calls the camera HAL read, writes into a pre-allocated SPSC slot, and updates a heartbeat atomic — no mutex, no heap, no file I/O; memory is locked with mlockall. Verification: a test build hooks `operator new` to abort inside the capture section, TSan runs on the sim build, and on target I trace with `perf sched` / ftrace to confirm no sleeping and measure wake-up latency against budget."

### Q7. "Signed vs unsigned for indices/time?"
> "Unsigned for ring indices because wrap-around is defined and I rely on it. Signed 64-bit for timestamps and durations so subtractions can go negative without surprises — and never 32-bit milliseconds, which overflow after ~24.8 days signed or ~49.7 days unsigned; a classic uptime bug on devices that run for months."

### Q8. "How do you review concurrency code from a mid-level engineer?"
> "I ask them to write the invariant in a comment: who owns each piece of data, which thread writes it, what establishes happens-before. If they can't state it, the design isn't done. Then I check shutdown paths — most real races live in teardown — and require the change to pass TSan in CI. I prefer pointing to the pattern we already have, like the bounded queue with close(), over inventing new primitives."

---

## 10. Checklist review C++ cho embedded (dùng trong code review)

- [ ] Ownership rõ ràng; không raw `new/delete`.
- [ ] Không alloc/lock/blocking syscall trên RT path; có test chứng minh.
- [ ] Mọi shared mutable state: atomic hoặc mutex; invariant được ghi.
- [ ] Queue bounded + policy + counter.
- [ ] Shutdown có thứ tự; thread được join; không detach.
- [ ] Không UB khi parse input (fuzzed — xem [`fuzz_command_protocol.cpp`](../aegiscam/core/fuzz/fuzz_command_protocol.cpp)).
- [ ] Time dùng monotonic clock, 64-bit.
- [ ] `[[nodiscard]]` cho `Result`; lỗi không bị nuốt.
- [ ] `-Wall -Wextra -Werror`, clang-tidy, ASan/UBSan/TSan pass.
