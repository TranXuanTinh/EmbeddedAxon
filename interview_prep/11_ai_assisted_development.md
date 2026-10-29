# 11. Driving Safe AI-Assisted Development in Embedded Squads

> **Interviewer đang đánh giá gì?** Đây là một trong những điểm đặc trưng và nổi bật nhất của JD này (xuất hiện ở cả phần *Responsibilities* lẫn *Preferred Qualifications*):
> - *"Drive AI-assisted development practices across the squad by establishing quality gates, review protocols, and reusable workflows that make AI adoption practical and safe."*
> - *"Proactively follow the latest software engineering developments and distinguish between promising new practices and passing trends."*
> - *"Experience integrating AI tools across the software development lifecycle (SDLC) with clear review, validation, and safety guardrails."*
>
> **Tâm lý của Hiring Manager:** Công ty đang đẩy mạnh năng suất bằng AI coding agents (Copilot, Cursor, Claude/Gemini agents), nhưng ở mảng **Embedded / Safety-Critical C++**, code do AI sinh ra rất dễ chứa lỗi ngầm chết người: **subtle data races, sai memory ordering, cấp phát heap ngầm trên real-time thread, quên `fsync` thư mục cha, hoặc bịa ra syscall/flag không tồn tại**. Họ cần một Staff Engineer **không bài xích AI**, biết biến AI thành đòn bẩy cho cả squad, nhưng thiết lập được **hàng rào kỹ thuật tự động (automated guardrails)** để không một dòng code AI nguy hiểm nào lọt xuống thiết bị.

Liên hệ bộ tài liệu quản trị AI thực tế đã xây dựng sẵn trong AegisCam:
- Chính sách AI của Squad: [`aegiscam/docs/ai_assisted_development/policy.md`](../aegiscam/docs/ai_assisted_development/policy.md)
- Cổng chất lượng tự động (CI Quality Gates): [`aegiscam/docs/ai_assisted_development/quality_gates.md`](../aegiscam/docs/ai_assisted_development/quality_gates.md)
- Thư viện Prompt chuẩn hoá (Reusable Prompt Workflows): [`aegiscam/docs/ai_assisted_development/prompt_library.md`](../aegiscam/docs/ai_assisted_development/prompt_library.md)
- Checklist Review PR có AI: [`aegiscam/docs/ai_assisted_development/review_checklist.md`](../aegiscam/docs/ai_assisted_development/review_checklist.md)
- Đo lường hiệu quả & chống suy giảm chất lượng: [`aegiscam/docs/ai_assisted_development/metrics.md`](../aegiscam/docs/ai_assisted_development/metrics.md)

---

## 1. Triết lý Cốt lõi: "AI is an Untrusted Junior Pair-Programmer, Never an Accountable Author"

Khi trả lời phỏng vấn, hãy mở đầu bằng 3 nguyên tắc vàng từ [`policy.md`](../aegiscam/docs/ai_assisted_development/policy.md):
1. **Con người chịu trách nhiệm 100% (Human Accountability):** Tác giả mở Pull Request chịu trách nhiệm hoàn toàn về từng dòng code. Câu nói *"AI viết đoạn đó đấy"* bị cấm tuyệt đối trong buổi post-mortem hay code review. Nếu tác giả không giải thích được tại sao dòng code đó dùng `memory_order_acquire` trên bảng trắng, PR đó chưa được phép merge.
2. **Không tin vào cảm giác "Trông có vẻ đúng" (Zero-Trust Verification):** LLM được huấn luyện để tạo ra code **trông giống code chuẩn nhất (plausible)**, chứ không phải code có mô hình trạng thái đúng đắn. Do đó, mọi rào chắn phải là **deterministic gates** (compiler warnings `-Werror`, `clang-tidy`, `ASan/UBSan/TSan`, `go test -race`, fuzzing, `FakeClock` unit tests).
3. **Spec-First, Small-Diff Workflow:** AI làm việc cực tốt khi đã có **tài liệu thiết kế / hợp đồng giao diện rõ ràng** (`PROJECT_SPEC.md`, ADRs, header `.h` với pre/post-conditions) và phạm vi thay đổi nhỏ. AI làm việc cực tệ khi được giao một prompt mơ hồ *"Hãy viết toàn bộ daemon camera cho tôi"*.

---

## 2. Bản đồ Áp dụng AI xuyên suốt SDLC (Đâu là vùng Xanh, Vàng, Đỏ?)

| Giai đoạn SDLC | Ứng dụng AI mang lại ROI cao nhất | Mức độ rủi ro | Guardrail bắt buộc |
|---|---|---|---|
| **1. Architecture & Design (ADRs, Threat Modeling)** | Dùng AI làm "Devil's Advocate": liệt kê các kịch bản mất nguồn, race conditions, hoặc tấn công STRIDE mà kỹ sư có thể bỏ sót | **Xanh (Thấp)** | Kỹ sư xác minh lại từng giả định phần cứng/kernel; ghi thành ADR |
| **2. Boilerplate, HAL Stubs & Serialization** | Sinh khung CMake, mock/sim HAL classes, JSON/line parser theo đúng BNF spec | **Vàng (Trung bình)** | Bắt buộc đi kèm bài test **libFuzzer** (như `fuzz_command_protocol.cpp`) vì AI rất hay quên kiểm tra biên chuỗi! |
| **3. Test Generation & Fault Injection (ROI SỐ 1!)** | Đưa header `.h` + state table cho AI để sinh hàng chục table-driven unit tests (GoogleTest / Go `_test.go`) phủ hết các nhánh lỗi `ErrorCode` | **Xanh (Cao nhất về năng suất)** | Reviewer kiểm tra **chất lượng của lệnh `ASSERT`/`EXPECT`** (tránh trường hợp AI viết test chỉ gọi hàm mà không assert kết quả, tạo "fake coverage") |
| **4. Hot-Path Real-Time, Lock-Free & Crypto (`Red Zone`)** | `SpscRingBuffer`, `capture` thread (`SCHED_FIFO`), `sha256`/`hmac`/`hash_chain`, `AbSlotManager` | **Đỏ (Cực cao)** | Nhãn `ai-assisted` trên vùng này yêu cầu **2 Senior/Staff approvals**, chạy TSan/ASan, và đối chiếu với test vectors chuẩn (NIST FIPS 180-4 / RFC 2104) |
| **5. Debugging & Log Triage** | Tóm tắt stack trace, viết script Python/awk lọc log DLT/journald, gợi ý giả thuyết từ `dmesg` | **Xanh/Vàng** | Tuyệt đối **không dán khóa mật mã, PII, hay dữ liệu bằng chứng thật** lên cloud LLM công cộng |

---

## 3. 7 Lỗi Điển hình của AI trong C++ / Embedded (Và cách bắt gọn trong Review)

Interviewer sẽ hỏi: *"What specific failure modes have you seen when engineers use AI tools for embedded C++ or Go?"* — Hãy nêu 7 lỗi thực tế này (trích từ [`review_checklist.md`](../aegiscam/docs/ai_assisted_development/review_checklist.md)):

1. **Cấp phát Heap ngầm trên Real-Time Path (Hidden Allocations):**
   - *Lỗi của AI:* Dùng `std::function`, `std::stringstream`, hoặc `std::vector` không `reserve` bên trong vòng lặp `capture` thread (`SCHED_FIFO`).
   - *Cách bắt:* Quy tắc code review + custom check kiểm tra không có `new`/`malloc` trên luồng thu nhận.
2. **Ảo tưởng về Thread-Safety và Atomics:**
   - *Lỗi của AI:* Đặt `std::atomic<bool> running_` hoặc dùng `volatile` rồi tin rằng toàn bộ struct `Frame` đi kèm đã được đồng bộ hoá, hoặc dùng `memory_order_relaxed` cho cả cờ báo hiệu dữ liệu sẵn sàng!
   - *Cách bắt:* Bắt buộc chạy **ThreadSanitizer (`TSan`)** và `go test -race` trong CI; yêu cầu comment giải thích cặp `release`/`acquire`.
3. **Ghi file "giả nguyên tử" (Broken Power-Loss Safety):**
   - *Lỗi của AI:* Ghi thẳng vào `manifest.json` bằng `std::ofstream`, hoặc ghi ra `.tmp` rồi `rename()` nhưng **quên gọi `fsync(fd)` trước khi đóng và `fsync(dir_fd)` sau khi rename** — khiến file rỗng 0 byte khi rút pin!
   - *Cách bắt:* Gom toàn bộ thao tác ghi nguyên tử vào một hàm duy nhất `IStorageHal::write_file_atomic` và cấm dùng `std::ofstream` trực tiếp trong module nghiệp vụ.
4. **Dùng `memcmp` / `==` cho HMAC Signature (Timing Side-Channel):**
   - *Lỗi của AI:* Viết `if (computed_hmac == manifest.hmac)` bằng `std::string::operator==`.
   - *Cách bắt:* Kiểm tra trong security review; bắt buộc dùng `constant_time_equal`.
5. **Dùng `sleep()` trong Unit Tests thay vì Dependency Injection thời gian:**
   - *Lỗi của AI:* Khi được yêu cầu viết test cho debounce 50 ms hoặc timeout 5s, AI luôn sinh `std::this_thread::sleep_for(50ms)`.
   - *Cách bắt:* Cấm `sleep_for` trong `core/tests/`; ép truyền `FakeClock` vào mọi class thông qua prompt template của squad.
6. **Goroutine Leak trong Go khi Timeout:**
   - *Lỗi của AI:* Tạo `ch := make(chan err)` (unbuffered) trong một goroutine con rồi `select` cùng `ctx.Done()` ở hàm cha — nếu context hết hạn trước, goroutine con bị kẹt vĩnh viễn!
   - *Cách bắt:* Checklist Go review (`make(chan T, 1)`) + kiểm tra `defer resp.Body.Close()`.
7. **Bịa đặt API / Cờ cấu hình (Hallucinated Syscalls / systemd Directives / License Risk):**
   - *Lỗi của AI:* Bịa ra tuỳ chọn `ioctl` hoặc directive systemd không có trong phiên bản kernel/systemd của Yocto, hoặc chép nguyên đoạn thuật toán từ một dự án GPLv3 vào codebase thương mại.
   - *Cách bắt:* Cross-compile thực tế trên Yocto SDK trong CI + quét bản quyền/SBOM.

---

## 4. Thiết lập Reusable Workflows & Quality Gates cho Squad

Làm sao biến việc dùng AI từ "mạnh ai nấy dùng tự phát" thành **hệ thống kỹ thuật của cả squad**?
1. **Chuẩn hoá `PROJECT_SPEC.md` và `.clang-tidy` làm Context cho AI:**
   AI agent chỉ thông minh bằng ngữ cảnh ta cung cấp. Trong AegisCam, chúng ta duy trì [`PROJECT_SPEC.md`](../aegiscam/docs/architecture/PROJECT_SPEC.md) ngắn gọn, chính xác (chứa rõ ngân sách thread, tên namespace, quy ước `Result<T>`, định dạng JSON/IPC) và thư mục [`prompt_library.md`](../aegiscam/docs/ai_assisted_development/prompt_library.md) gồm các mẫu prompt chuẩn của team:
   - *Prompt #1: Tạo module mới theo hợp đồng HAL + FakeClock (No-Heap, `Result<T>`).*
   - *Prompt #2: Sinh bộ test biên & Fault Injection từ bảng trạng thái.*
   - *Prompt #3: Kiểm tra Adversarial Security & Concurrency trước khi mở PR.*
2. **8 Cổng Chất lượng Tự động trong CI ([`quality_gates.md`](../aegiscam/docs/ai_assisted_development/quality_gates.md)):**
   Một PR (dù do người hay AI hỗ trợ) muốn merge phải vượt qua 8 cổng tự động: Format $\rightarrow$ Static Analysis (`clang-tidy`, `go vet`) $\rightarrow$ Cross-Build (`aarch64`) $\rightarrow$ Unit/Integration Tests $\rightarrow$ Sanitizers (`ASan+UBSan`, `TSan`, `-race`) $\rightarrow$ Coverage Gate ($\ge 80\%$) $\rightarrow$ Fuzz Smoke (`libFuzzer` 60s) $\rightarrow$ End-to-End `pytest` + `verify_evidence.py`.
3. **Đo lường bằng số liệu thực chất ([`metrics.md`](../aegiscam/docs/ai_assisted_development/metrics.md)):**
   - **Không bao giờ** đo thành công bằng *"tỷ lệ % dòng code do AI viết"* hay *"số dòng code sinh ra mỗi tuần"* — thước đo đó khuyến khích kỹ sư bơm code phình to (code bloat)!
   - Chúng ta đo **Cặp chỉ số Cân bằng (Speed vs Safety Counter-Metrics)**:
     - *Tốc độ:* PR Lead Time, thời gian viết test cho tính năng mới.
     - *Chất lượng đối trọng:* **Change Failure Rate**, **Code Churn trong 14 ngày sau khi merge** (nếu code mới phải sửa đi sửa lại trong 2 tuần sau merge $\implies$ AI tạo nợ kỹ thuật!), **Defect Escape Rate ra HIL/Field**, và **Review Turnaround Time**.

---

## 5. Phân biệt "Promising Practices vs Passing Trends" (Câu hỏi JD rất thích!)

JD ghi: *"Proactively follow the latest software engineering developments and distinguish between promising new practices and passing trends."*

### Bảng phân tích quan điểm của một Staff Embedded Engineer

| Xu hướng / Thực hành | Phân loại | Lý giải sắc bén dưới góc nhìn Embedded Mission-Critical |
|---|---|---|
| **1. Spec-Driven AI Generation + Sanitizers/Fuzzing Gates** | **Promising Practice (Áp dụng ngay)** | Dùng AI để viết unit test, fuzz harness, sim HAL và boilerplate dựa trên spec chặt chẽ, được kiểm chứng bằng ASan/TSan/libFuzzer giúp tăng tốc độ phủ test lên 2–3× mà không giảm độ an toàn. |
| **2. "Vibe Coding" / Để AI tự sinh cả hệ thống không cần hiểu sâu hay review từng dòng** | **Passing Trend / Nguy hiểm chết người** | Trong phần mềm nhúng 24/7, một lỗi race condition hoặc quên `fsync` chỉ nổ ra sau 2 tuần ngoài hiện trường khi cảnh sát đang làm nhiệm vụ. Không hiểu từng dòng code = không thể debug khi thiết bị treo lúc 2 giờ sáng. |
| **3. Memory-Safe Languages (Go / Rust) ở ranh giới Mạng & Parser có chọn lọc** | **Promising Practice (Thực tế & Bền vững)** | Thay vì khẩu hiệu cực đoan *"viết lại toàn bộ firmware đang chạy ổn định sang ngôn ngữ mới trong 6 tháng"* (passing hype gây rủi ro lớn), việc dùng **Go cho `netagent`** (TLS/HTTP/VPN) hoặc Rust cho các parser mới ở ranh giới không tin cậy là bước đi thực dụng, loại bỏ 70% CVE bộ nhớ ở tầng mạng. |
| **4. Nhét LLM lớn chạy trực tiếp trên thiết bị nhúng nhỏ chỉ vì "AI on Edge" đang hot** | **Passing Trend (Nếu không giải quyết bài toán sản phẩm thực)** | Trên body camera chạy pin 12 tiếng, mỗi mili-watt đều quý giá. Chỉ đưa mô hình nhẹ (như Wake-word, Gunshot/Holster detection, hoặc ALPR YOLOv8 tối ưu INT8 trên Jetson trong xe có nguồn ắc-quy) khi có bài toán độ trễ/offline thực sự; các tác vụ nặng như tự động chép lời (transcription) và làm mờ khuôn mặt (redaction) nên thực hiện trên Cloud sau khi thiết bị dock và upload xong. |
| **5. eBPF & Structured Telemetry cho Embedded Linux Observability** | **Promising Practice** | Cho phép chẩn đoán độ trễ scheduling, block I/O và rớt gói mạng trên kernel Linux sản xuất với chi phí overhead cực thấp mà không phải biên dịch lại kernel modules. |

---

## 6. Model Answer — *"How would you roll out AI-assisted development across an embedded squad of 8 engineers?"*

> "I treat AI adoption as a **systems engineering problem**, not a tooling purchase. When I built AegisCam using AI assistants, I codified the exact rollout framework in [`docs/ai_assisted_development/`](../aegiscam/docs/ai_assisted_development/). I roll it out in three phases:
>
> **Phase 1 — Establish Automated Guardrails Before Increasing Velocity:**
> If you speed up code generation without automated verification, you just create a review bottleneck and ship subtle bugs faster. Before encouraging heavy AI use, I make sure our CI enforces `clang-tidy`, `ASan+UBSan`, `TSan`, `go test -race`, `libFuzzer` on parsers, and `FakeClock`-based deterministic tests.
>
> **Phase 2 — Start with High-ROI, Low-Risk Workflows and Shared Prompts:**
> I start the squad on test generation, simulation HAL fakes, and fault-injection harnesses. We maintain a version-controlled prompt library in the repo that instructs the AI on our non-negotiable constraints: no heap allocations on real-time paths, explicit `Result<T>` error handling instead of exceptions, and `IClock` injection instead of `sleep()`.
>
> **Phase 3 — Tiered Review Protocol and Balanced Metrics:**
> We classify modules into risk tiers. AI-assisted changes touching our **Red Zone** — real-time threads, lock-free structures, cryptography, and OTA rollback — require two senior approvals and an explicit concurrency/power-loss review checklist. Finally, we track **14-day code churn** and **escaped defects** alongside PR cycle time so we know AI is actually improving engineering quality, not just inflating lines of code."
