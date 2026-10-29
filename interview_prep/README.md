# Interview Prep — Staff / Lead Embedded Software Engineer

> Bộ tài liệu chuẩn bị phỏng vấn cho **Trần Xuân Tịnh** theo JD tại [`../JD.txt`](../JD.txt).
> Ngôn ngữ: giải thích tiếng Việt + thuật ngữ tiếng Anh; **model answers bằng tiếng Anh** (vì vòng phỏng vấn kỹ thuật thường dùng tiếng Anh).

> [!NOTE]
> **Giả định về nhà tuyển dụng (chưa xác nhận):** repo tên `EmbeddedAxon` → rất có thể là **Axon** (Axon Body cameras, Axon Fleet in-car cameras, TASER, Evidence.com/Axon Evidence; có engineering center tại TP.HCM). Mọi ví dụ trong bộ tài liệu được "tune" theo thiết bị public-safety (body-worn camera, in-car camera, evidence chain-of-custody). **Trước buổi phỏng vấn hãy xác nhận lại với recruiter** — nếu không phải Axon, các nguyên lý vẫn đúng, chỉ cần đổi ví dụ sản phẩm.

---

## 1. Chân dung ứng viên (dùng nhất quán trong mọi câu trả lời)

| Hạng mục | Nội dung (chỉ dùng sự thật) |
|---|---|
| Học vấn | Kỹ sư + **ThS. Điều khiển & Tự động hoá — ĐH Bách Khoa TP.HCM** |
| Kinh nghiệm | ~5 năm phần mềm nhúng / hệ thống |
| Điểm nhấn #1 | **Function Owner eCall** trong Telematics Control Unit (TCU) ô tô — C++11/14 multi-threaded, CAN, HTTP, ASPICE / AUTOSAR / ISO 26262, HLD/LLD, field issue triage |
| Điểm nhấn #2 | AUTOSAR BSW (CAN signal mapping theo K-Matrix, CANoe), on-site Hàn Quốc (MBD), ADAS traceability |
| Điểm nhấn #3 | Bài báo **IEEE Xplore**: vision-based drone tracking & landing (ROS / Gazebo / Pixhawk) |
| Dự án #1 | **MultiCamera DeepStream** edge analytics trên Jetson — repo gốc: [`../README.md`](../README.md), [`../src/`](../src/), [`../docs/`](../docs/) |
| Dự án #2 (portfolio cho JD này) | **AegisCam** — nền tảng phần mềm body-worn evidence camera: [`../aegiscam/docs/architecture/PROJECT_SPEC.md`](../aegiscam/docs/architecture/PROJECT_SPEC.md) |

---

## 2. Index

| # | File | Trọng tâm |
|---|---|---|
| 00 | `README.md` | Master guide, pitch, kế hoạch 14 ngày, checklist |
| 01 | [01_jd_analysis_and_gap_strategy.md](01_jd_analysis_and_gap_strategy.md) | Giải mã JD từng dòng, evidence matrix, chiến lược gap "10+ năm", interview loop |
| 02 | [02_cpp_systems_programming.md](02_cpp_systems_programming.md) | Modern C++ cho embedded: RAII, memory model, atomics, lock-free SPSC, UB, no-heap RT path |
| 03 | [03_go_and_python_for_embedded.md](03_go_and_python_for_embedded.md) | Go (goroutines, context, GC, cross-compile) & Python (pytest, tooling); vì sao netagent dùng Go |
| 04 | [04_embedded_linux_rtos_realtime.md](04_embedded_linux_rtos_realtime.md) | Boot chain, DT, PREEMPT_RT, scheduling, priority inversion, RTOS, Yocto |
| 05 | [05_debugging_and_shipping_in_the_field.md](05_debugging_and_shipping_in_the_field.md) | gdb, core dump, sanitizers, perf/ftrace/eBPF, field crash reporting, OTA rollout, war stories |
| 06 | [06_testing_strategy.md](06_testing_strategy.md) | Test pyramid, HAL/DI, FakeClock, HIL, fault injection, fuzzing, coverage gates |
| 07 | [07_cs_fundamentals_dsa.md](07_cs_fundamentals_dsa.md) | DSA, complexity, OOD/SOLID, 12 bài coding có lời giải C++ |
| 08 | [08_networking_tcpip_vpn_dns_dhcp.md](08_networking_tcpip_vpn_dns_dhcp.md) | TCP/IP, MTU/PMTUD, NAT, DNS, DHCP, VPN/WireGuard, tunneling, TLS, troubleshooting |
| 09 | [09_embedded_security.md](09_embedded_security.md) | Secure boot, TPM, dm-verity, signed OTA, hardening, STRIDE, chain of custody, SBOM |
| 10 | [10_system_design_embedded.md](10_system_design_embedded.md) | Framework + 3 walkthrough: body cam pipeline, fleet OTA 100k, in-car multi-cam |
| 11 | [11_ai_assisted_development.md](11_ai_assisted_development.md) | Dẫn dắt AI adoption an toàn: guardrails, quality gates, metrics, trend vs practice |
| 12 | [12_technical_leadership_and_mentoring.md](12_technical_leadership_and_mentoring.md) | Technical direction, design reviews, mentoring, influence, 14 STAR stories |
| 13 | [13_project_deep_dive_aegiscam.md](13_project_deep_dive_aegiscam.md) | Trình bày AegisCam 2/5/15 phút, ADR, drill-down; MultiCamera & eCall |
| 14 | [14_question_bank_and_reverse_questions.md](14_question_bank_and_reverse_questions.md) | 70+ rapid-fire Q&A + câu hỏi ngược cho interviewer |

---

## 3. JD → File map

| Dòng trong JD | File chính | Bằng chứng |
|---|---|---|
| Own technical direction, engineering strategy, architectural decisions | 12, 10, 01 | AegisCam ADRs, [`tech_roadmap.md`](../aegiscam/docs/process/tech_roadmap.md) |
| Lead design & architecture reviews (stability, performance, safety-critical) | 12, 10 | [`design_review_template.md`](../aegiscam/docs/process/design_review_template.md), eCall HLD/LLD reviews |
| Well-tested maintainable code; lead code reviews | 06, 02, 07 | [`code_review_guidelines.md`](../aegiscam/docs/process/code_review_guidelines.md), CI gates (spec §7) |
| Mentor mid/senior engineers | 12 | [`mentoring_framework.md`](../aegiscam/docs/process/mentoring_framework.md) |
| AI-assisted development: quality gates, review protocols, reusable workflows | 11 | [`../aegiscam/docs/ai_assisted_development/`](../aegiscam/docs/ai_assisted_development/) |
| Collaborate with PM / Design / cross-functional | 12, 10 | eCall OEM requirement negotiation |
| Integrate latest open source, keep reliability | 01, 09, 12 | WireGuard choice ([ADR-0006](../aegiscam/docs/adr/0006-wireguard-over-ipsec-openvpn.md)), DeepStream wrapped behind HAL |
| Improve processes: long-term planning, sprint execution, operational readiness | 12, 05 | [`operational_readiness.md`](../aegiscam/docs/process/operational_readiness.md) |
| C/C++, Go, Python | 02, 03 | aegisd (C++17), netagent (Go), system tests (pytest) |
| Develop, debug, ship embedded Linux / RTOS | 04, 05 | eCall TCU, MultiCamera on Jetson/Yocto |
| Unit / integration / system testing | 06 | `core/tests/unit`, `core/tests/integration`, `tests/system` |
| CS fundamentals, OOD, code quality verification | 07 | state machine, ring buffer, token bucket trong AegisCam |
| Networking TCP/IP, VPN, DNS, DHCP, tunneling | 08 | `netagent/internal/{link,dnscheck,dhcp,vpn,uploader}` |
| Security, low latency, high stability | 09, 04, 02 | hash chain + HMAC, SCHED_FIFO capture, watchdog supervisor |
| Promising practices vs passing trends | 11, 01 | Section "trend vs practice" |

---

## 4. Elevator pitch 2 phút (Staff/Lead framing)

**Nguyên tắc:** Không kể CV theo thời gian. Kể theo **3 trụ: ownership của hệ thống safety-critical → nâng chuẩn chất lượng cho team → chủ động định hướng kỹ thuật**, kết bằng "vì sao role này / công ty này".

### English (≈ 2 minutes)

> "Hi, I'm Tinh. I'm an embedded software engineer with about five years building software for devices where failure is not an option, and a Master's in Control and Automation from HCMUT.
>
> The thread through my career is **owning safety-critical features end to end**. In automotive telematics I was the **Function Owner for eCall** — the emergency call that fires when an airbag deploys. I owned it from OEM requirements through HLD/LLD, the multi-threaded C++ implementation, CAN integration and field defect triage, under ASPICE and ISO 26262. That role taught me to design for the bad day: power loss, no network, a crash in a tunnel — and to make every requirement traceable to a test.
>
> Second, I care about **raising the engineering bar for the whole team**, not just my own code. On eCall I drove design reviews and coached junior engineers on threading and state-machine design; on my edge-AI project, MultiCamera on Jetson, I put the hardware behind HAL interfaces so the whole pipeline is testable on a laptop, with sanitizers and bounded-queue policies enforced in CI.
>
> Most recently I built **AegisCam**, a reference platform for a body-worn evidence camera — because that's exactly the class of product your team ships. It has a lock-free pre-event buffer on a real-time capture thread, a SHA-256 hash chain with an HMAC-sealed manifest for chain of custody, A/B OTA with automatic rollback, a watchdog supervisor, and a Go connectivity agent handling link failover, DNS/DHCP health and WireGuard with resumable upload. It also has written ADRs, a design-review template and an **AI-assisted development policy with concrete quality gates** — I used AI tools heavily to build it, and the guardrails are what made that safe.
>
> I'm looking for a Staff/Lead role where I can own technical direction for a squad, grow mid and senior engineers, and ship devices that officers and the public rely on. That's why this role is a strong fit."

### Tiếng Việt

> "Chào anh/chị, em là Tịnh, kỹ sư phần mềm nhúng với khoảng 5 năm kinh nghiệm, Thạc sĩ Điều khiển & Tự động hoá Bách Khoa TP.HCM.
>
> Sợi chỉ xuyên suốt sự nghiệp của em là **làm chủ trọn vẹn các tính năng safety-critical**. Ở mảng telematics ô tô, em là **Function Owner của eCall** — cuộc gọi cứu hộ tự động khi túi khí bung. Em chịu trách nhiệm từ yêu cầu OEM, HLD/LLD, hiện thực C++ đa luồng, tích hợp CAN đến xử lý lỗi thực địa theo ASPICE và ISO 26262. Vai trò đó dạy em thiết kế cho 'ngày tồi tệ nhất': mất nguồn, mất sóng, tai nạn trong hầm — và mọi yêu cầu đều phải truy vết được tới test.
>
> Thứ hai, em quan tâm đến việc **nâng chuẩn kỹ thuật của cả team**: dẫn design review, kèm các bạn junior về threading và state machine; ở dự án MultiCamera trên Jetson, em đặt toàn bộ phần cứng sau HAL để test được trên laptop, sanitizer và bounded-queue policy được enforce trong CI.
>
> Gần đây em xây **AegisCam** — nền tảng tham chiếu cho body-worn evidence camera: pre-event buffer lock-free trên thread real-time, hash chain SHA-256 + manifest HMAC cho chain of custody, OTA A/B có rollback, watchdog supervisor, agent Go xử lý failover, DNS/DHCP, WireGuard và resumable upload — kèm ADR, design-review template và **chính sách AI-assisted development với quality gate cụ thể**.
>
> Em tìm vai trò Staff/Lead để định hướng kỹ thuật cho squad, phát triển các kỹ sư mid/senior và ship thiết bị mà cảnh sát và cộng đồng tin cậy."

> [!TIP]
> Kết thúc pitch bằng một "hook" để interviewer hỏi tiếp vào vùng mạnh nhất: *"Happy to go deep on the pre-event buffer or the evidence integrity design."*

---

## 5. Kế hoạch 14 ngày

| Ngày | Sáng (deep study) | Chiều (practice) | Output |
|---|---|---|---|
| D1 | 01 JD analysis, viết lại evidence matrix bằng lời của mình | Tập pitch EN 5 lần, ghi âm | Pitch < 2:15 |
| D2 | 02 C++ (memory model, atomics, RAII) | Tự code SPSC ring buffer từ trí nhớ + test | Code chạy với TSan |
| D3 | 02 (UB, no-heap, error handling) + 07 DSA phần 1 | 4 bài coding (ring buffer, LRU, token bucket, merge intervals) | ≤ 25 phút/bài |
| D4 | 04 Linux/RTOS/real-time | Chạy `cyclictest`, đọc `chrt`, `taskset` trên VM | Ghi chú số đo |
| D5 | 05 Debugging & shipping | Tạo core dump, debug bằng gdb; chạy ASan trên bug cố ý | 2 war stories STAR |
| D6 | 06 Testing strategy | Viết test với FakeClock cho state machine | Giải thích pyramid trong 2 phút |
| D7 | 07 DSA phần 2 + OOD | 4 bài coding còn lại, mock 45 phút | Self-review |
| D8 | 08 Networking | tcpdump TCP handshake, `ss -ti`, `dig +trace`, `wg show` | Walkthrough troubleshooting nói to |
| D9 | 09 Security | Vẽ threat model STRIDE cho body cam | 1 trang diagram |
| D10 | 10 System design (a) + (b) | Mock system design 45 phút (bấm giờ) | Diagram + trade-offs |
| D11 | 10 (c) + 11 AI-assisted dev | Viết câu trả lời "how would you roll out AI tools to the squad" | 3 phút answer |
| D12 | 12 Leadership & mentoring | Luyện 8 STAR stories, mỗi câu ≤ 3 phút | Bảng story → competency |
| D13 | 13 AegisCam deep dive | Trình bày 2/5/15 phút, cho bạn hỏi vặn | Slide/diagram 1 trang |
| D14 | 14 Question bank (rapid-fire) | Mock loop full + chọn 5 câu hỏi ngược | Nghỉ sớm |

---

## 6. Checklist ngày trước phỏng vấn

- [ ] Xác nhận công ty, team (Body camera? Fleet? Device platform?), format từng vòng, ngôn ngữ coding (C++ hay tuỳ chọn), có shared editor không.
- [ ] Đọc lại tin tức sản phẩm gần nhất của công ty (body cam thế hệ mới, real-time features, AI features như transcription/redaction) — để hỏi câu hỏi ngược có chất lượng.
- [ ] Mở sẵn: [`PROJECT_SPEC.md`](../aegiscam/docs/architecture/PROJECT_SPEC.md), [system_overview.md](../aegiscam/docs/architecture/system_overview.md), danh sách ADR.
- [ ] Ôn 5 con số "thuộc lòng": trigger→record < 100 ms; pre-event 30 s; boot attempts = 3; T1 = 50% / T2 = 87.5% lease; chunk 4 MiB.
- [ ] 8 STAR stories (file 12) — mỗi câu có **số liệu** hoặc **hệ quả cụ thể**; đánh dấu phần nào là "điều chỉnh theo trải nghiệm thật".
- [ ] Câu trả lời cho gap "10+ năm" và "5+ năm mentoring" (file 01 §4) — nói thẳng, tự tin, không xin lỗi.
- [ ] Thiết bị: tai nghe, camera, giấy + bút để vẽ diagram, nước.
- [ ] Ngủ đủ. Không học thêm chủ đề mới sau 20h.

---

## 7. Năm nguyên tắc trả lời ở level Staff

1. **Requirements & constraints trước, giải pháp sau.** "Before I pick a design, what's the latency budget / power budget / failure model?"
2. **Luôn nêu trade-off và cái giá phải trả.** Staff engineer không có "best solution", chỉ có "best for these constraints".
3. **Nói về tác động lên team, không chỉ lên code.** Ví dụ: "I wrote the ADR so the next engineer doesn't relitigate the decision."
4. **Thiết kế cho failure.** Power loss, disk full, network partition, clock jump, bad OTA — nhắc tới trước khi bị hỏi.
5. **Trung thực về phạm vi.** Phân biệt rõ "I did", "my team did", "I would do" (portfolio project). Interviewer Staff-level sẽ drill để kiểm tra.
