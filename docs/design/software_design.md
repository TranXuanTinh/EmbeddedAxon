# Software Design Document (SWDD) — AegisCam Platform
## Modular Software Architecture, HAL Interface Contracts, Dependency Injection & IPC Specification

Tài liệu **Software Design Document (SWDD)** này mô tả cấu trúc tổ chức mã nguồn, hợp đồng giao diện phần cứng (HAL Contracts), cơ chế tiêm phụ thuộc (Dependency Injection), đặc tả giao thức truyền thông liên tiến trình (IPC Line Protocol), và kiến trúc kiểm thử tự động của **AegisCam**.

---

## 1. Cấu Trúc Thư Mục & Phân Rã Gói Phần Mềm (Package Structure)

```
aegiscam/
├── core/                                   # C++17 Real-Time Evidence Daemon (aegisd)
│   ├── apps/
│   │   └── aegisd_main.cpp                 # CLI parser, POSIX signal handlers, DI bootstrapper
│   ├── include/aegis/
│   │   ├── app/device_controller.h         # Top-level orchestrator & command dispatcher
│   │   ├── common/                         # Result<T>/Status, IClock/FakeClock, thread-safe Logger
│   │   ├── util/                           # Lock-free SpscRingBuffer<T>, MPMC BoundedQueue<T>
│   │   ├── crypto/                         # FIPS 180-4 Sha256, RFC 2104 Hmac, binary HashChain
│   │   ├── hal/                            # Pure abstract HAL interfaces (ICameraHal, IStorageHal, ...)
│   │   ├── evidence/                       # Frame, PreEventBuffer, SegmentWriter, EvidenceStore, AuditLog
│   │   ├── recording/                      # Table-driven RecordingStateMachine
│   │   ├── trigger/                        # Debounced priority TriggerArbiter
│   │   ├── ota/                            # A/B slot manager with boot-counter rollback
│   │   ├── health/                         # Per-thread deadline WatchdogSupervisor
│   │   └── ipc/                            # Fuzz-tested CommandProtocol parser & formatter
│   ├── src/                                # Module implementations (.cpp)
│   ├── hal/
│   │   ├── sim/                            # Host simulation HALs with fault injection
│   │   └── linux/                          # Target Linux HALs (V4L2, POSIX fsync, /dev/watchdog)
│   ├── tests/
│   │   ├── unit/                           # 11 GoogleTest unit test suites
│   │   └── integration/                    # Full DeviceController integration & power-loss tests
│   └── fuzz/
│       └── fuzz_command_protocol.cpp       # Clang libFuzzer target
│
├── netagent/                               # Go 1.22 Connectivity, VPN & Resumable Upload Daemon
│   ├── cmd/netagent/main.go                # Daemon entrypoint & signal lifecycle
│   └── internal/
│       ├── link/                           # eth > wifi > lte hysteresis failover & metered policy
│       ├── dhcp/                           # udhcpc/dhclient parser & T1/T2 lease state machine
│       ├── dnscheck/                       # Primary/secondary DNS failover & captive-portal canary
│       ├── vpn/                            # WireGuard config renderer, handshake monitor, kill-switch
│       ├── uploader/                       # Resumable 4 MiB chunked uploader + token bucket
│       └── ipc/                            # Line-protocol client & async EVT stream reader
│
├── tools/verify_evidence.py                # Standalone forensic chain-of-custody verifier
└── tests/system/                           # Black-box pytest E2E test suite driving `aegisd --sim`
```

---

## 2. Hợp Đồng Giao Diện Trừu Tượng Phần Cứng (HAL Interface Contracts)

Toàn bộ 5 giao diện trong [`core/include/aegis/hal/`](../../core/include/aegis/hal/i_storage_hal.h) là các lớp cơ sở thuần ảo (Pure Abstract Classes) không chứa biến thành viên:

### 2.1 `ICameraHal` ([`i_camera_hal.h`](../../core/include/aegis/hal/i_camera_hal.h))
```cpp
class ICameraHal {
 public:
  virtual ~ICameraHal() = default;
  virtual Result<evidence::Frame> read_frame() = 0;
};
```
* **Bản thực thi Linux (`V4l2CameraHal`)**: Đọc khung hình từ `/dev/video0` qua vùng nhớ `V4L2_MEMORY_MMAP`.
* **Bản mô phỏng (`SimCameraHal`)**: Dựa trên thời gian hiện tại của `FakeClock` và tốc độ `fps` (mặc định 30 FPS), sinh ra chuỗi khung hình tất định với `keyframe = (seq % 30 == 0)`.

### 2.2 `IStorageHal` ([`i_storage_hal.h`](../../core/include/aegis/hal/i_storage_hal.h))
```cpp
class IStorageHal {
 public:
  virtual ~IStorageHal() = default;
  virtual Status write_file_atomic(std::string_view rel_path,
                                   const uint8_t* data, std::size_t len) = 0;
  Status write_file_atomic(std::string_view rel_path, std::string_view content);
  Status write_file_atomic(std::string_view rel_path,
                           const std::vector<uint8_t>& data);
  virtual Status append_line(std::string_view rel_path,
                             std::string_view line) = 0;
  virtual Result<std::vector<uint8_t>> read_file(std::string_view rel_path) = 0;
  Result<std::string> read_text(std::string_view rel_path);
  virtual bool exists(std::string_view rel_path) const = 0;
  virtual Result<std::vector<std::string>> list_dirs(
      std::string_view rel_dir) const = 0;
  virtual Result<std::vector<std::string>> list_files(
      std::string_view rel_dir) const;
  virtual Status remove_file(std::string_view rel_path);
  virtual Status mkdir_p(std::string_view rel_dir) = 0;
  virtual std::string root_dir() const = 0;
};
```
* **Bản thực thi Linux (`PosixStorageHal`)**: Thực thi giao thức 5 bước `.tmp` + `fsync(fd)` + `rename()` + `fsync(dir_fd)` đảm bảo tính nguyên tử ngay cả khi mất nguồn.
* **Bản mô phỏng (`SimStorageHal`)**: Hỗ trợ thêm các phương thức tiêm lỗi kiểm thử (Fault Injection):
  * `set_fail_next_write(true)`: Giả lập lỗi I/O ở đúng lần ghi kế tiếp.
  * `set_disk_full(true)`: Giả lập tình huống đầy bộ nhớ Flash để kiểm tra nhánh chuyển sang trạng thái `Fault`.

### 2.3 `IWatchdogHal`, `IKeyStoreHal` & `IBootControlHal`
* **[`IWatchdogHal`](../../core/include/aegis/hal/i_watchdog_hal.h)**: `start(timeout_ms)`, `kick()`, `stop()` (`LinuxWatchdogHal::stop()` ghi ký tự Magic Close `'V'` vào `/dev/watchdog`).
* **[`IKeyStoreHal`](../../core/include/aegis/hal/i_key_store_hal.h)**: `get_device_hmac_key()` trả về khóa bí mật ký `manifest.json` (từ TPM 2.0 hoặc khóa mô phỏng).
* **[`IBootControlHal`](../../core/include/aegis/hal/i_boot_control_hal.h)**: `load_state()` và `save_state(BootSlotState)` quản lý phân vùng `active_slot`, `pending_slot`, `boot_attempts_left`, `marked_good`.

---

## 3. Khởi Tạo Dependency Injection & Quản Lý Vòng Đời (`aegisd_main.cpp`)

Toàn bộ các đối tượng HAL và `DeviceController` được khởi tạo trực tiếp trên **Stack (RAII)** trong hàm `main()`, không sử dụng biến toàn cục phức tạp hay cấp phát con trỏ thô (`new`/`delete`):

```cpp
aegis::common::FakeClock fake_clock(1700000000000ULL);
aegis::hal::sim::SimCameraHal camera_hal(fake_clock, config.fps, 30);
aegis::hal::sim::SimStorageHal storage_hal(store_dir);
aegis::hal::sim::SimWatchdogHal watchdog_hal;
aegis::hal::sim::SimKeyStoreHal key_store_hal;
aegis::hal::sim::SimBootControlHal boot_control_hal(storage_hal);

aegis::app::DeviceController controller(
    config, fake_clock, &fake_clock, camera_hal, storage_hal, watchdog_hal,
    key_store_hal, boot_control_hal);
```

---

## 4. Đặc Tả Giao Thức Dòng Lệnh Liên Tiến Trình (IPC Line Protocol Specification)

Giao thức giữa `aegisd` và `netagent` (hoặc trình kiểm thử hệ thống `pytest`) là giao thức văn bản từng dòng (ASCII Line Protocol, tối đa 512 byte mỗi dòng), giúp dễ dàng gỡ lỗi bằng `socat`/`nc`, ghi nhật ký trực tiếp và kiểm thử fuzzing bằng `libFuzzer`:

### 4.1 Bảng Lệnh Yêu Cầu & Phản Hồi (`Request -> Response`)

| Lệnh gửi tới `aegisd` | Tham số hợp lệ | Phản hồi thành công (`OK {...}`) | Mã lỗi có thể trả về (`ERR <code> <msg>`) |
|---|---|---|---|
| `TRIGGER <src>` | `holster \| button \| signal \| remote` | `OK {"state":"Recording","incident":"INC-...","segments":0,"net":"up","slot":"A"}` | `INVALID_ARGUMENT`, `INVALID_STATE`, `IO_ERROR` |
| `STOP <src>` | `holster \| button \| signal \| remote` | `OK {"state":"PostRecord","incident":"INC-...",...}` | `INVALID_STATE` (nếu ưu tiên thấp hơn nguồn mở phiên ghi) |
| `TICK <ms>` | Số nguyên `1 .. 60000` (chỉ ở `--sim`) | `OK {"state":"...","incident":"...","segments":1,"net":"up","slot":"A"}` | `INVALID_ARGUMENT`, `NOT_SUPPORTED` |
| `STATUS` | Không có | `OK {"state":"Buffering","incident":"","segments":0,"net":"up","slot":"A"}` | `PARSE_ERROR` (nếu thừa tham số) |
| `VERIFY` | Không có | `OK {"verified":true,"status":"OK"}` | `INTEGRITY_ERROR` (nếu tệp phân đoạn hoặc `audit.log` bị sửa) |
| `NET <state>` | `up \| down` | `OK {"state":"...","net":"up","slot":"A"}` | `INVALID_ARGUMENT` |
| `UPLOAD_ACK <inc> <idx>` | `<incident_id>` và chỉ số $\ge 1$ | `OK {"state":"...","incident":"INC-...",...}` | `INVALID_ARGUMENT`, `IO_ERROR` |
| `OTA_STAGE <path> <sha>` | Đường dẫn tệp & 64 ký tự hex | `OK {"slot":"A","pending_slot":"B","boot_attempts_left":3}` | `INTEGRITY_ERROR` (sai SHA-256), `IO_ERROR` |
| `OTA_REBOOT` | Không có | `OK {"slot":"B","pending_slot":"B","boot_attempts_left":2,"marked_good":false}` | `IO_ERROR`, `NOT_SUPPORTED` |
| `OTA_MARK_GOOD` | Không có | `OK {"slot":"B","marked_good":true}` | `IO_ERROR` |
| `QUIT` | Không có | `OK {"state":"Buffering","incident":"","segments":0,"net":"up","slot":"A"}` | — |

### 4.2 Sự Kiện Bất Đồng Bộ (`EVT {...}`)
Khi `aegisd` đóng gói xong một phân đoạn hoặc niêm phong toàn bộ sự cố, nó chủ động phát dòng `EVT ` tới `netagent`:
* `EVT {"type":"segment_sealed","incident":"INC-AEG-0001-1700000000000","index":1,"path":"incidents/INC-AEG-0001-1700000000000/seg_000001.bin","sha256":"<64-hex>"}`
* `EVT {"type":"incident_sealed","incident":"INC-AEG-0001-1700000000000","segments":3}`

---

## 5. Kiến Trúc Kiểm Thử Đa Tầng & Cổng Chất Lượng (Multi-Tier Testing Architecture)

| Tầng Kiểm Thử | Công Cụ & Vị Trí File | Mục Tiêu Kiểm Chứng |
|---|---|---|
| **1. Unit Tests (C++17 & Go)** | GoogleTest ([`core/tests/unit/*.cpp`](../../core/tests/unit/test_recording_state_machine.cpp)) & `go test -race` (`netagent/internal/*/*_test.go`) | Kiểm chứng độc lập vector chuẩn NIST SHA-256, RFC 4231 HMAC, vòng đệm Lock-free SPSC đa luồng, FSM, TriggerArbiter, A/B OTA rollback, DHCP/DNS/VPN/Uploader |
| **2. Integration Tests (C++17)** | [`test_device_controller.cpp`](../../core/tests/integration/test_device_controller.cpp) | Kiểm chứng sự phối hợp giữa tất cả module qua `Sim*Hal`, tiêm lỗi đĩa đầy/ghi lỗi và khả năng khôi phục sau sập nguồn (`recover()`) |
| **3. Fuzz Testing (`libFuzzer`)** | [`fuzz_command_protocol.cpp`](../../core/fuzz/fuzz_command_protocol.cpp) | Bơm hàng triệu chuỗi byte ngẫu nhiên/độc hại vào `CommandProtocol::parse_line()` kèm ASan/UBSan để đảm bảo không bao giờ crash hoặc đọc ngoài vùng nhớ |
| **4. System E2E Tests (`pytest`)** | [`test_system_e2e.py`](../../tests/system/test_system_e2e.py) + [`verify_evidence.py`](../../tools/verify_evidence.py) | Chạy tiến trình `aegisd --sim` như một hộp đen qua ống dẫn Stdin/Stdout, kiểm tra toàn bộ quy trình từ `TICK` $\rightarrow$ `TRIGGER` $\rightarrow$ lật 1 bit làm giả bằng chứng $\rightarrow$ bắt quả tang bằng `VERIFY` và `verify_evidence.py` |

