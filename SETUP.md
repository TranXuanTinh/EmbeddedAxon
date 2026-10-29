# Hướng Dẫn Cài Đặt, Biên Dịch & Vận Hành (How to Set Up — AegisCam Platform)

Tài liệu này hướng dẫn chi tiết từng bước từ cài đặt môi trường, biên dịch phần lõi C++17 (`aegisd`), biên dịch tác nhân mạng Go (`netagent`), chạy toàn bộ 4 tầng kiểm thử tự động (Unit, Integration, Fuzz, System E2E `pytest`), kiểm định pháp y chuỗi bằng chứng (`verify_evidence.py`), cho đến biên dịch chéo ARM64 và triển khai trên Embedded Linux (`systemd` / Yocto).

---

## 1. Yêu Cầu Hệ Thống & Cài Đặt Công Cụ (Prerequisites)

Hệ thống được thiết kế để phát triển và kiểm thử 100% trên **Linux (Ubuntu 22.04 / 24.04 LTS, Debian 12)** hoặc **WSL2 trên Windows** mà không cần phần cứng camera chuyên dụng nhờ chế độ mô phỏng tất định (`--sim`).

### 1.1. Danh sách công cụ cần thiết
| Thành phần | Phiên bản yêu cầu | Mục đích sử dụng |
|---|---|---|
| **CMake** | $\ge 3.16$ (khuyến nghị $\ge 3.21$ để dùng `CMakePresets.json`) | Cấu hình và quản lý build C++17 |
| **Trình biên dịch C++17** | `GCC >= 11` hoặc `Clang >= 14` | Biên dịch `aegis_core`, `aegisd`, Sanitizers & libFuzzer |
| **GoogleTest** | `libgtest-dev` (hoặc tự động tải v1.14.0 qua CMake `FetchContent`) | Chạy Unit Test & Integration Test cho C++ |
| **ARM64 Toolchain** *(Tùy chọn)* | `aarch64-linux-gnu-gcc` / `aarch64-linux-gnu-g++` | Biên dịch chéo (Cross-compile) cho bo mạch nhúng ARM64 |
| **Go Toolchain** | `Go >= 1.22` (chỉ dùng thư viện chuẩn, 0 phụ thuộc ngoài) | Biên dịch và kiểm thử `netagent` |
| **Python 3 & Pytest** | `Python >= 3.10`, `pytest >= 7.4` | Chạy kiểm thử E2E hộp đen (`tests/system/`) và `tools/verify_evidence.py` |

### 1.2. Cài đặt nhanh trên Ubuntu / Debian / WSL2
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  clang \
  clang-format \
  clang-tidy \
  llvm \
  lld \
  libgtest-dev \
  gcc-aarch64-linux-gnu \
  g++-aarch64-linux-gnu \
  golang-go \
  python3 \
  python3-pip \
  python3-pytest \
  lcov
```

*(Nếu dùng `pip` hoặc môi trường ảo `venv` cho Python)*:
```bash
pip install -r tests/system/requirements.txt
```

---

## 2. Biên Dịch & Kiểm Thử Phần Lõi C++17 (`aegisd`)

Dự án cung cấp sẵn tệp [`CMakePresets.json`](./CMakePresets.json) với 5 cấu hình tiêu chuẩn: `dev`, `asan-ubsan`, `tsan`, `coverage`, và `release-aarch64`.

### 2.1. Cấu hình & Build chế độ Phát triển (`dev`)
```bash
# 1. Cấu hình CMake (Debug + bật toàn bộ cảnh báo -Werror + Tests)
cmake --preset dev

# 2. Biên dịch toàn bộ thư viện, binary aegisd và bộ kiểm thử aegis_tests
cmake --build --preset dev -j$(nproc)

# 3. Chạy toàn bộ Unit Tests (11 suites) + Integration Tests
ctest --preset dev --output-on-failure
```

> **Lưu ý khi máy không có kết nối Internet và chưa cài `libgtest-dev`**:
> Nếu chỉ muốn biên dịch nhanh daemon `aegisd` mà không cần tải GoogleTest:
> ```bash
> cmake -B build/dev -DCMAKE_BUILD_TYPE=Debug -DAEGIS_BUILD_TESTS=OFF
> cmake --build build/dev -j$(nproc)
> ```

### 2.2. Chạy Kiểm Thử Với Sanitizers (ASan + UBSan & ThreadSanitizer)
Để phát hiện lỗi tràn bộ đệm, rò rỉ bộ nhớ, hành vi không xác định (Undefined Behavior) hoặc tranh chấp dữ liệu đa luồng (Data Race):
```bash
# AddressSanitizer + UndefinedBehaviorSanitizer
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan -j$(nproc)
ctest --preset asan-ubsan --output-on-failure

# ThreadSanitizer (Kiểm tra SpscRingBuffer, BoundedQueue, WatchdogSupervisor)
cmake --preset tsan
cmake --build --preset tsan -j$(nproc)
ctest --preset tsan --output-on-failure
```

### 2.3. Biên Dịch Chéo Cho Bo Mạch Nhúng ARM64 (`aarch64-linux-gnu`)
```bash
cmake --preset release-aarch64
cmake --build --preset release-aarch64 -j$(nproc)

# Kiểm tra định dạng tệp thực thi ARM64 vừa tạo
file build/release-aarch64/aegisd
# Kết quả kỳ vọng: ELF 64-bit LSB pie executable, ARM aarch64, ...
```

---

## 3. Chạy Thử Nghiệm Tương Tác Trực Tiếp Với `aegisd --sim`

Sau khi biên dịch xong `./build/dev/aegisd`, bạn có thể chạy trực tiếp daemon ở chế độ mô phỏng tất định và gõ từng lệnh trong giao thức dòng lệnh ASCII qua `stdin`:

```bash
mkdir -p /tmp/aegis_demo_store
./build/dev/aegisd --sim --store /tmp/aegis_demo_store --device-id AEG-0001
```

Gõ lần lượt các lệnh sau vào Terminal để trải nghiệm toàn bộ vòng đời ghi hình và niêm phong bằng chứng:
```text
STATUS
TICK 35000
TRIGGER holster
TICK 10000
STOP remote
STOP holster
TICK 5000
VERIFY
QUIT
```

**Giải thích kết quả:**
1. `TICK 35000`: Tua nhanh đồng hồ giả lập 35 giây, nạp đầy vòng đệm 30 giây trước sự kiện (`PreEventBuffer`) ở tốc độ 30 FPS.
2. `TRIGGER holster`: Kích hoạt ghi hình khẩn cấp từ cảm biến bao súng, tự động xả 30 giây video trước đó bắt đầu từ một `Keyframe` (I-frame).
3. `TICK 10000`: Ghi thêm 10 giây, tự động cắt và niêm phong phân đoạn `seg_000001.bin` kèm sự kiện `EVT {"type":"segment_sealed",...}`.
4. `STOP remote`: Thử tắt máy từ nguồn ưu tiên thấp (`remote < holster`) $\rightarrow$ bị `TriggerArbiter` chặn ngay với lỗi `ERR INVALID_STATE`.
5. `STOP holster` + `TICK 5000`: Chuyển sang `PostRecord`, ghi thêm 5 giây cuối, cắt `seg_000002.bin`, ký `HMAC-SHA256` vào `manifest.json` và phát `EVT {"type":"incident_sealed",...}`.
6. `VERIFY`: Tự kiểm tra lại toàn bộ chuỗi băm phân đoạn và nhật ký `audit.log` $\rightarrow$ `OK {"verified":true,"status":"OK"}`.

---

## 4. Biên Dịch & Kiểm Thử Go Connectivity Agent (`netagent`)

Tiến trình `netagent` nằm tại thư mục [`netagent/`](./netagent) và chỉ sử dụng thư viện chuẩn của Go (không cần tải bất kỳ thư viện bên thứ ba nào).

```bash
cd netagent

# 1. Chạy toàn bộ Unit Tests với bộ phát hiện tranh chấp luồng (-race)
go test -race -v ./...

# 2. Biên dịch binary netagent cho máy host
go build -o ../build/netagent ./cmd/netagent

# 3. Kiểm tra khởi tạo cấu hình (Dry-run)
../build/netagent --dry-run

# 4. Biên dịch chéo (Cross-compile) tĩnh cho thiết bị ARM64 Linux
CGO_ENABLED=0 GOOS=linux GOARCH=arm64 go build -ldflags="-s -w" -o ../build/netagent-arm64 ./cmd/netagent
cd ..
```

---

## 5. Kiểm Thử Hệ Thống Đầu-Cuối (`pytest`) & Công Cụ Kiểm Định Pháp Y (`verify_evidence.py`)

### 5.1. Chạy Bộ Kiểm Thử Hộp Đen E2E (`tests/system/`)
Bộ kiểm thử `pytest` sẽ tự động tìm binary `build/dev/aegisd` (hoặc bạn có thể chỉ định qua biến môi trường `AEGISD_BIN`), khởi chạy tiến trình con `aegisd --sim` và kiểm tra 6 kịch bản nghiệp vụ lớn (Pre-event + Trigger + Segment Cut, Chặn STOP ưu tiên thấp, Phát hiện làm giả bằng chứng lật 1 bit, A/B OTA Rollback sau 3 lần boot lỗi, v.v.):

```bash
# Chạy tự động (khi đã build ở build/dev/aegisd)
pytest tests/system -v

# Hoặc chỉ định đường dẫn binary tường minh
AEGISD_BIN=./build/dev/aegisd pytest tests/system -v
```

### 5.2. Kiểm Định Ngoại Tuyến Chuỗi Bảo Toàn Bằng Chứng (`tools/verify_evidence.py`)
Công cụ [`tools/verify_evidence.py`](./tools/verify_evidence.py) dùng để kiểm tra độc lập bất kỳ thư mục lưu trữ bằng chứng nào:
```bash
python3 tools/verify_evidence.py /tmp/aegis_demo_store
```
* **Mã thoát `0` (`VERIFIED`)**: Toàn bộ `audit.log`, các tệp `seg_*.bin`, chuỗi băm nhị phân `chain_head` và chữ ký `HMAC-SHA256` trong `manifest.json` đều toàn vẹn 100%.
* **Mã thoát `2` (`TAMPER`)**: Phát hiện có ít nhất 1 bit trong tệp video bị sửa, có phân đoạn bị xóa/thêm trái phép, hoặc nhật ký kiểm toán bị chỉnh sửa.
* **Mã thoát `1`**: Lỗi tham số dòng lệnh hoặc thư mục không tồn tại.

---

## 6. Kiểm Thử Fuzzing (`libFuzzer`) & Kiểm Tra Chất Lượng Mã Nguồn (Code Quality Gates)

### 6.1. Chạy Fuzzing Giao Thức IPC Với Clang `libFuzzer`
```bash
cmake -B build/fuzz \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DAEGIS_ENABLE_FUZZ=ON \
  -DAEGIS_BUILD_TESTS=OFF

cmake --build build/fuzz -j$(nproc)

# Chạy fuzzer trong 30 giây với độ dài chuỗi tối đa 600 bytes
./build/fuzz/fuzz_command_protocol -max_len=600 -max_total_time=30
```

### 6.2. Tạo Báo Cáo Độ Bao Phủ Mã Nguồn (Code Coverage)
```bash
cmake --preset coverage
cmake --build --preset coverage -j$(nproc)
ctest --preset coverage --output-on-failure

lcov --capture --directory build/coverage --output-file build/coverage/coverage.info
lcov --remove build/coverage/coverage.info '/usr/*' '*/tests/*' --output-file build/coverage/filtered.info
lcov --list build/coverage/filtered.info
```

### 6.3. Kiểm Tra Định Dạng (Formatting) & Phân Tích Tĩnh (Static Analysis)
```bash
# Kiểm tra định dạng C++17 (.clang-format)
find core -name '*.h' -o -name '*.cpp' | xargs clang-format --dry-run -Werror

# Kiểm tra định dạng và phân tích tĩnh Go
cd netagent && test -z "$(gofmt -l .)" && go vet ./... && cd ..
```

---

## 7. Triển Khai Trên Thiết Bị Nhúng Linux (`systemd` & Yocto Project)

### 7.1. Cài Đặt Trực Tiếp Với `systemd` Trên Target Linux
1. Sao chép các tệp thực thi đã biên dịch vào `/usr/bin/`:
   ```bash
   sudo install -m 0755 build/release-aarch64/aegisd /usr/bin/aegisd
   sudo install -m 0755 build/netagent-arm64 /usr/bin/aegis-netagent
   ```
2. Tạo tài khoản hệ thống tách biệt đặc quyền và thư mục lưu trữ bằng chứng:
   ```bash
   sudo useradd --system --no-create-home --shell /usr/sbin/nologin aegis || true
   sudo useradd --system --no-create-home --shell /usr/sbin/nologin aegis-net || true
   sudo install -d -o aegis -g aegis -m 0750 /var/lib/aegis /var/lib/aegis/incidents /var/lib/aegis/ota
   ```
3. Cài đặt và kích hoạt 2 dịch vụ [`deploy/systemd/aegisd.service`](./deploy/systemd/aegisd.service) và [`deploy/systemd/netagent.service`](./deploy/systemd/netagent.service):
   ```bash
   sudo cp deploy/systemd/aegisd.service /etc/systemd/system/
   sudo cp deploy/systemd/netagent.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable --now aegisd.service netagent.service
   ```

### 7.2. Tích Hợp Vào Bản Dựng Yocto Project (`meta-aegiscam`)
Thêm layer [`deploy/yocto/meta-aegiscam`](./deploy/yocto/meta-aegiscam) vào môi trường Yocto (Kirkstone / Scarthgap):
```bash
bitbake-layers add-layer /path/to/EmbeddedAxon/deploy/yocto/meta-aegiscam
bitbake aegiscam-image
```

---

## 8. Xử Lý Sự Cố Thường Gặp (Troubleshooting)

1. **`pytest` báo `Skipped: aegisd binary not found`**:
   * **Nguyên nhân**: Chưa biên dịch target `aegisd` hoặc thư mục build nằm ở vị trí khác.
   * **Cách khắc phục**: Chạy `cmake --preset dev && cmake --build --preset dev` trước khi chạy `pytest`, hoặc truyền biến `AEGISD_BIN=/path/to/aegisd pytest tests/system`.
2. **CMake báo lỗi khi tải `googletest` (`FetchContent` failed)**:
   * **Nguyên nhân**: Môi trường chạy không có kết nối mạng ra GitHub và máy chưa cài gói `libgtest-dev`.
   * **Cách khắc phục**: Cài gói `sudo apt-get install libgtest-dev` (tệp `core/tests/CMakeLists.txt` sẽ tự động nhận diện qua `find_package(GTest QUIET)` mà không cần mạng), hoặc tắt build test bằng `-DAEGIS_BUILD_TESTS=OFF`.
3. **Lỗi `aarch64-linux-gnu-g++: not found` khi chạy preset `release-aarch64`**:
   * **Cách khắc phục**: Cài đặt trình biên dịch chéo bằng lệnh `sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu`.

