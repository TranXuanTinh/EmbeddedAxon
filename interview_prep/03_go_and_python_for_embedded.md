# 03. Go & Python for Embedded Systems

> **Interviewer đang đánh giá gì?** JD nêu *"10+ years of experience with C, C++, Go, or Python"*. Với thiết bị Linux nhúng hiện đại (body camera, in-car hub, edge gateway), không ai viết mọi thứ bằng C++ nữa:
> - **C++17 (`aegisd`)** cho **data plane / real-time path** (capture, ring buffer, crypto, segment writer, hardware watchdog).
> - **Go (`netagent`)** cho **control/connectivity plane** (link failover, DNS/DHCP monitor, VPN supervisor, resumable HTTP/TLS upload).
> - **Python (`tests/system`, `tools/verify_evidence.py`)** cho **system/HIL testing, fault injection harness, và forensic verification CLI**.
>
> Ở level Staff, câu hỏi quan trọng nhất là: **"Why did you draw the language boundary there, and what are the operational costs on a constrained ARM device?"**

Liên hệ dự án:
- AegisCam Go agent: [`netagent/`](../aegiscam/netagent/), [ADR-0005: Go for Connectivity Agent](../aegiscam/docs/adr/0005-go-for-connectivity-agent.md), [networking.md](../aegiscam/docs/architecture/networking.md)
- AegisCam Python verifier & tests: [`verify_evidence.py`](../aegiscam/tools/verify_evidence.py), [`tests/system/`](../aegiscam/tests/system/)

---

## 1. Kiến trúc đa ngôn ngữ trên thiết bị nhúng: Khi nào dùng C++ vs Go vs Python?

| Tiêu chí | C / C++17 (`aegisd`) | Go 1.22 (`netagent`) | Python 3 (`pytest`, `verify_evidence.py`) |
|---|---|---|---|
| **Độ trễ (Latency)** | Deterministic, < 100 µs (no GC, `SCHED_FIFO`, lock-free) | Soft real-time; GC pause ~0.5–2 ms (không hợp cho frame capture 30 fps với DMA hạn hẹp) | Không dùng trên hot path thiết bị; GIL + interpreter overhead |
| **Memory footprint** | Rất thấp (vài MB RSS, pre-allocated pools) | Trung bình (~10–25 MB RSS tối thiểu do Go runtime + heap overhead) | Cao (~30–50 MB mỗi interpreter process) |
| **Memory safety** | Tự quản lý (RAII, sanitizers bắt lỗi) | Bounds check, GC, không có dangling pointer hay buffer overflow trong pure Go | Memory safe (trừ C extensions) |
| **Concurrency** | `std::thread`, atomics, lock-free | Goroutines (~2 KB stack ban đầu), channels, `select`, `context.Context` | `asyncio` (I/O-bound), `multiprocessing` (bypass GIL) |
| **Networking / TLS / HTTP2** | Phải ghép `libcurl` + `openssl` + `c-ares` (dễ dính CVE, callback phức tạp) | **Stdlib hạng nhất** (`net/http`, `crypto/tls`, `net`), memory-safe TLS stack | Rất tiện cho test harness & mock server |
| **Deployment trên ARM** | Binary nhỏ, phụ thuộc `libc`/`libstdc++` | **Single static binary** (`CGO_ENABLED=0 GOOS=linux GOARCH=arm64`) | Chạy trên host/CI hoặc tool offline |

### Model Answer — *"Why did you split AegisCam into a C++ daemon (`aegisd`) and a Go daemon (`netagent`) instead of writing one C++ binary?"*

> "I split them along the **real-time and trust boundary**, documented in [ADR-0005](../aegiscam/docs/adr/0005-go-for-connectivity-agent.md).
>
> `aegisd` owns the real-time capture path, the pre-event ring buffer, cryptographic hashing, and the hardware watchdog. It has a strict < 100 ms trigger-to-record latency budget and zero-allocation rules on the capture thread. A garbage-collected runtime is the wrong tool there.
>
> `netagent` owns untrusted network I/O: parsing DHCP leases, probing DNS for captive portals, supervising WireGuard, and running resumable chunked HTTPS uploads with exponential backoff. Doing that in C++ means pulling in `libcurl`, `OpenSSL`, and `c-ares` — historically a huge source of CVEs and tricky callback lifetime bugs. Go gives us a memory-safe TLS/HTTP stack in the standard library, trivial concurrency with goroutines and `context.Context` cancellation, and a single static `arm64` binary with `CGO_ENABLED=0`.
>
> Best of all, process isolation means that even if `netagent` panics or gets OOM-killed on a malformed network response, `aegisd` keeps recording evidence uninterrupted. The cost is ~15 MB of extra RSS for the Go runtime and a simple Unix domain socket IPC protocol, which is well within our 512 MB RAM budget."

---

## 2. Go cho Embedded Linux (Deep Dive)

### 2.1 Goroutines, Scheduler (M:N / GMP) & Channels
- **GMP Model:**
  - **G (Goroutine):** lightweight user-space thread, stack khởi điểm **2 KB**, tự grow/shrink.
  - **M (Machine):** OS thread do kernel lập lịch.
  - **P (Processor):** logical context giữ runqueue cục bộ; số lượng P mặc định bằng `GOMAXPROCS` (trên thiết bị nhúng 4 core, ta thường giới hạn `GOMAXPROCS=1` hoặc `2` cho `netagent` để không tranh CPU với `aegisd`).
- **Work stealing:** khi P hết G trong local queue, nó lấy từ global queue hoặc steal một nửa từ P khác.
- **Blocking syscall:** nếu G gọi blocking file/syscall, P tách khỏi M đang block và gắn vào M khác để tiếp tục chạy các G còn lại. Network I/O đi qua **netpoller (`epoll` trên Linux)** nên không chiếm OS thread.

#### Channels & Select patterns trong `netagent`
- **Unbuffered channel (`make(chan T)`):** đồng bộ hoá (rendezvous) — gửi block cho đến khi nhận.
- **Buffered channel (`make(chan T, N)`):** tách nhịp độ, tạo backpressure có giới hạn.
- **Quy tắc đóng channel:** **Người gửi (producer) đóng channel**, không bao giờ đóng từ phía consumer (gửi vào closed channel gây `panic`).
- **Đọc từ `nil` hoặc closed channel:**
  - Gửi/nhận trên `nil` channel → block vĩnh viễn (hữu ích để tắt một nhánh trong `select`).
  - Nhận từ closed channel → trả về zero-value ngay lập tức (`v, ok := <-ch` với `ok == false`).

```go
// Pattern chuẩn trong netagent: periodic probe + graceful shutdown qua context
func (m *Supervisor) Run(ctx context.Context) error {
	ticker := time.NewTicker(m.interval)
	defer ticker.Stop() // LUÔN Stop() để tránh leak timer trong runtime

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			if err := m.checkTunnel(ctx); err != nil {
				m.log.Printf("tunnel check failed: %v", err)
			}
		}
	}
}
```

### 2.2 `context.Context` — Kiểm soát Lifetime & Cancellation
Trên thiết bị di động (xe tuần tra rời trạm Wi-Fi chuyển sang LTE, hoặc rút khỏi dock), mọi request mạng đang bay **phải huỷ được ngay** để không treo socket:
1. Truyền `ctx context.Context` làm tham số đầu tiên của mọi hàm I/O (`UploadChunk(ctx, ...)`).
2. Dùng `context.WithTimeout(parent, 10*time.Second)` cho mỗi HTTP chunk hoặc DNS probe, và **luôn `defer cancel()`** ngay sau đó.
3. Khi link Wi-Fi rớt, `LinkManager` gọi `cancel()` trên context của phiên upload hiện tại → `http.Request.WithContext(ctx)` đóng TCP socket ngay lập tức.

### 2.3 Goroutine Leaks — Kẻ giết thiết bị 24/7
Mỗi goroutine bị leak giữ ít nhất 2 KB stack + toàn bộ object mà biến trên stack trỏ tới (prevent GC). Sau vài ngày chạy ở hiện trường, `netagent` sẽ bị OOM killer giết.
- **Nguyên nhân #1:** gửi vào channel không buffer khi consumer đã return sớm vì lỗi:
  ```go
  // BUG: nếu ctx bị huỷ trước khi hàm đọc ch, goroutine con bị kẹt mãi mãi ở ch <- res
  ch := make(chan result) // FIX: make(chan result, 1)
  go func() { ch <- doProbe() }()
  select {
  case r := <-ch: return r, nil
  case <-ctx.Done(): return result{}, ctx.Err()
  }
  ```
- **Nguyên nhân #2:** `http.Response.Body` không được đọc hết và `Close()` → kết nối TCP không trả về pool, giữ goroutine readLoop/writeLoop của `net/http`.
- **Nguyên nhân #3:** `time.After(d)` trong vòng lặp nóng (tạo timer không thu hồi cho đến khi hết hạn) → dùng `time.NewTimer` / `time.NewTicker` + `Stop()`.

### 2.4 Go Garbage Collector (GC) & Memory Limit trên Embedded
- Go dùng **concurrent, tri-color mark-and-sweep GC**, non-compacting, write-barrier.
- **STW (Stop-The-World) pauses** hiện nay rất ngắn (< 0.5–1 ms), nhưng GC tiêu tốn CPU (~10–25% trong phase mark nếu cấp phát nhiều) và RAM (mặc định `GOGC=100` nghĩa là heap tăng gấp đôi sau mỗi chu kỳ).
- **Kỹ thuật tối ưu trên thiết bị nhúng (512 MB RAM):**
  1. **`GOMEMLIMIT` (Go 1.19+):** đặt soft memory limit, ví dụ `GOMEMLIMIT=32MiB` trong systemd service của `netagent`. Khi tiến gần ngưỡng này, GC chạy quyết liệt hơn thay vì để Linux OOM killer bắn.
  2. **Tái sử dụng buffer (`sync.Pool` hoặc pre-allocated buffer):** Trong `internal/uploader`, khi cắt segment thành các chunk 4 MiB để băm SHA-256 và upload, **không** `make([]byte, 4<<20)` ở mỗi chunk. Cấp phát một buffer cố định cho mỗi worker goroutine và dùng `io.ReadFull` đọc đè lên.
  3. **Escape Analysis (`go build -gcflags="-m"`):** hiểu khi nào biến nằm trên stack vs escape lên heap (trả về con trỏ, gán vào `interface{}` lớn, giữ trong closure sống lâu hơn hàm).

### 2.5 Race Detector, Cross-Compilation & Testing trong Go
- **`go test -race ./...`:** tích hợp ThreadSanitizer vào compiler. Bắt buộc bật trong CI (Spec §7). Lưu ý: `-race` làm tăng RAM 5–10× và chậm 2–10× nên chỉ chạy trong CI/integration test, không bật trên binary ship xuống thiết bị.
- **Cross-compile cho ARM64:**
  ```bash
  CGO_ENABLED=0 GOOS=linux GOARCH=arm64 \
    go build -trimpath -ldflags="-s -w" -o netagent ./cmd/netagent
  ```
  - `CGO_ENABLED=0`: không phụ thuộc `glibc` trên target (tránh lỗi lệch version `GLIBC_2.34 not found` giữa Ubuntu build host và Yocto rootfs), dùng pure-Go DNS resolver và TLS.
  - `-trimpath`: bỏ đường dẫn máy build để đạt **reproducible builds**.
  - `-ldflags="-s -w"`: strip symbol table và DWARF (giảm ~30% kích thước binary trên flash; giữ bản có debug info trên server để decode stack trace).

### 2.6 Ánh xạ trực tiếp vào `netagent` của AegisCam
1. **`internal/link` ([`link.go`](../aegiscam/netagent/internal/link/link.go)):**
   - Ưu tiên `eth > wifi > lte`.
   - **Hysteresis (chống flapping):** khi đang ở LTE mà Wi-Fi có sóng yếu chập chờn, không chuyển ngay sang Wi-Fi sau 1 ping thành công. Yêu cầu `N` lần probe liên tiếp thành công (`HysteresisCount`, ví dụ 3 lần) mới failback lên interface ưu tiên cao hơn.
   - **Metered policy:** khi ở LTE, chỉ đẩy các incident đánh dấu `priority=high` (ví dụ trigger từ `holster` hoặc `signal`), hoãn upload thông thường chờ về dock/Wi-Fi.
2. **`internal/dnscheck` ([`dnscheck.go`](../aegiscam/netagent/internal/dnscheck/dnscheck.go)):**
   - Tiêm interface `LookupFunc` để test không cần mạng thật.
   - Phát hiện **Captive Portal / DNS Hijack:** khi cảnh sát mang camera vào quán cà phê hoặc mạng khách sạn, DNS trả về IP giả (ví dụ `192.168.1.1`) cho tên miền gateway. `dnscheck` đối chiếu kết quả với dải IP bogon/expected hoặc kiểm tra mTLS/WireGuard handshake trước khi báo link `Healthy`.
3. **`internal/dhcp` ([`dhcp.go`](../aegiscam/netagent/internal/dhcp/dhcp.go)):**
   - Parse lease file của `udhcpc`/`dhclient` và mô hình hoá chuẩn RFC 2131: `BOUND` → tại **T1 (50% lease)** chuyển `RENEWING` (unicast tới DHCP server cấp phát) → tại **T2 (87.5% lease)** chuyển `REBINDING` (broadcast tìm bất kỳ server nào) → tại **100%** chuyển `INIT` (huỷ IP, ngắt upload).
4. **`internal/vpn` ([`vpn.go`](../aegiscam/netagent/internal/vpn/vpn.go)):**
   - Render cấu hình WireGuard và giám sát tuổi của `latest-handshake`. Nếu quá ngưỡng (ví dụ > 180s khi đang có traffic) → restart tunnel với exponential backoff.
   - **Kill-switch:** nếu `KillSwitch=true` và tunnel `Down`, `CanUpload()` trả về `false` tuyệt đối để không bao giờ rò rỉ dữ liệu bằng chứng ra ngoài đường truyền không mã hoá.
5. **`internal/uploader` ([`uploader.go`](../aegiscam/netagent/internal/uploader/uploader.go)):**
   - Chia file thành các **chunk 4 MiB**, tính SHA-256 từng chunk.
   - Trước khi gửi, gọi `QueryOffset(ctx, fileID)` lên server để biết server đã nhận tới byte thứ bao nhiêu → **resumable** từ đúng vị trí đứt kết nối, không gửi lại từ đầu.
   - **Exponential backoff with full jitter** (`sleep = rand(0, min(maxBackoff, base * 2^attempt))`) để tránh **thundering herd** khi 500 cảnh sát cùng cắm camera vào dock lúc giao ca (shift change).
   - **Token bucket rate limiter** để giới hạn băng thông upload, tránh làm nghẽn đường truyền của xe tuần tra (còn dùng cho MDT / bộ đàm IP).

---

## 3. Python cho Embedded Testing, Tooling & Forensics

### 3.1 Vai trò của Python trong hệ sinh thái Embedded
Python không nằm trong vòng lặp thu nhận khung hình của camera, nhưng là **vũ khí tăng năng suất số 1** quanh thiết bị:
1. **Black-box System & HIL Test Harness (`pytest`):** khởi chạy daemon `aegisd --sim` như một subprocess (hoặc giao tiếp qua UART/SSH với board thật), bơm lệnh IPC, kiểm tra trạng thái, giả lập mất nguồn (`SIGKILL`), kiểm tra cấu trúc file trên đĩa.
2. **Offline Forensic & Verification Tools (`tools/verify_evidence.py`):** công cụ độc lập chỉ dùng Python standard library (`hashlib`, `hmac`, `json`, `pathlib`) để bên kiểm toán/toà án hoặc kỹ sư hiện trường xác minh tính toàn vẹn của thư mục bằng chứng (`manifest.json`, chuỗi băm `seg_*.bin`, và `audit.log`) mà không cần cài đặt toolchain C++.
3. **CI/CD & Build glue:** kiểm tra ngưỡng coverage (`gcovr`/`llvm-cov`), tạo test vectors, phân tích log.

### 3.2 GIL (Global Interpreter Lock) — Hiểu đúng bản chất
- Trong CPython, **GIL** là một mutex bảo vệ các cấu trúc nội bộ của interpreter (đặc biệt là reference count của `PyObject`). Tại một thời điểm chỉ có **1 thread thực thi Python bytecode**.
- **Hệ quả:**
  - **CPU-bound** (tính toán mảng, parse dữ liệu lớn bằng pure Python): `threading` **không** chạy song song trên nhiều core (thậm chí chậm hơn do tranh chấp GIL). Phải dùng `multiprocessing`, C extension (`numpy` nhả GIL khi tính toán), hoặc chuyển sang C++/Go.
  - **I/O-bound** (đọc/ghi socket, serial port, đợi subprocess `aegisd`, HTTP): CPython **nhả GIL quanh các blocking OS syscall**, nên `threading` hoặc `asyncio` hoàn toàn hiệu quả cho test harness điều khiển nhiều thiết bị cùng lúc.

### 3.3 Thiết kế `pytest` Harness chuyên nghiệp cho Embedded (`tests/system/`)
Xem [`aegiscam/tests/system/test_end_to_end.py`](../aegiscam/tests/system/test_end_to_end.py):
- **Fixture quản lý vòng đời thiết bị giả lập:**
  - Tạo thư mục tạm (`tmp_path` fixture của pytest).
  - Spawn `aegisd --sim --store <tmp_path>` qua `subprocess.Popen(..., stdin=PIPE, stdout=PIPE, text=True)`.
  - Trong `finally` / teardown của fixture: gửi lệnh `QUIT`, đợi `proc.wait(timeout=5)`, nếu treo thì `proc.kill()` và fail test ngay (phát hiện deadlock lúc shutdown!).
- **Tách biệt sự kiện bất đồng bộ (`EVT ...`) và phản hồi đồng bộ (`OK ...` / `ERR ...`):**
  - Giao thức dòng của `aegisd` có thể đẩy `EVT {"type":"segment_sealed",...}` xen kẽ trước dòng `OK {...}`. Helper `send_cmd()` trong Python đọc từng dòng, gom các dòng `EVT ` vào danh sách sự kiện để test assert, và trả về dòng `OK`/`ERR` cho lệnh vừa gọi.
- **Zero external dependency cho công cụ pháp증 (`verify_evidence.py`):**
  - Chỉ dùng stdlib (`hashlib.sha256`, `hmac.new(..., digestmod=hashlib.sha256)`, `hmac.compare_digest` để chống timing attack).
  - Trả về exit code rõ nghĩa cho automation: `0` (verified OK), `2` (tampering detected), `1` (usage/IO error).

---

## 4. Câu hỏi phỏng vấn thường gặp (Go & Python)

### Q1: *"How would you investigate a memory leak or high memory usage in a Go service running on an embedded Linux device?"*
> "First, I distinguish between a true leak and the Go runtime holding onto freed virtual memory for reuse. I check `/proc/<pid>/status` (`VmRSS`) and `runtime.ReadMemStats` (`HeapInuse`, `HeapAlloc`, `NumGoroutine`).
>
> Almost every 'memory leak' I've seen in Go services is actually a **goroutine leak** holding references on its stack. So my first check is whether `NumGoroutine()` grows monotonically over hours. In `netagent`, we expose `net/http/pprof` only on loopback (`127.0.0.1:6060`) when diagnostic mode is enabled. I grab a goroutine dump (`/debug/pprof/goroutine?debug=2`) and a heap profile (`/debug/pprof/heap`), pull them off the device over SSH, and inspect them with `go tool pprof`.
>
> On constrained hardware, I also set `GOMEMLIMIT` (e.g., `32MiB`) so the GC becomes aggressive before the Linux OOM killer steps in, and I pool large buffers — like our 4 MiB upload chunk buffer — instead of allocating per request."

### Q2: *"Why did you use full jitter with exponential backoff in your Go uploader instead of simple exponential backoff?"*
> "Consider a police precinct at shift change: 100 officers walk in at 07:00 and dock their body cameras within the same two-minute window. If the local gateway or cloud ingest briefly returns `503 Service Unavailable` and every camera uses deterministic exponential backoff (1s, 2s, 4s, 8s), all 100 cameras will retry in synchronized waves and keep knocking the server over — a classic thundering herd.
>
> With **full jitter** (`sleep = random(0, min(cap, base * 2^attempt))`), retries spread uniformly across the window, desynchronizing the fleet immediately while still backing off exponentially under sustained load."

### Q3: *"When would you NOT use Go on an embedded system?"*
> "Three cases:
> 1. **Bare-metal / RTOS (FreeRTOS, Zephyr) or tiny MCUs** with kilobytes of RAM and flash — Go's runtime and minimum ~2 MB binary / ~10 MB RAM footprint don't fit (TinyGo exists, but has language/stdlib limitations compared to C/C++).
> 2. **Hard real-time paths** where a 1–2 ms GC mark-assist or stop-the-world pause violates a deadline — like our `SCHED_FIFO` frame capture thread or an audio DSP loop.
> 3. **Heavy C hardware SDK integration** (like V4L2 ioctls, DMA-BUF, or NVIDIA DeepStream): `cgo` transitions have non-trivial overhead (~50–100+ ns per call, plus goroutine/OS thread pinning), complicate cross-compilation, and lose Go's memory-safety guarantees at the C boundary. That belongs in C++ behind a HAL."
