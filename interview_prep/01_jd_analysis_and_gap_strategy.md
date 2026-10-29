# 01. JD Analysis & Gap Strategy

> Mục tiêu: hiểu **interviewer thực sự đang tìm gì** sau mỗi dòng JD, chuẩn bị bằng chứng cho từng dòng, và có chiến lược trung thực cho hai khoảng trống lớn nhất: **"10+ years"** và **"5+ years mentoring/leading"**.

---

## 1. Interviewer thực sự đánh giá gì ở role Staff/Lead?

Một Staff/Lead Embedded Engineer **không** được tuyển vì code nhanh nhất. Họ được tuyển vì:

| Năng lực | Câu hỏi ngầm của interviewer |
|---|---|
| **Scope** | Người này có từng sở hữu một thứ đủ lớn, đủ mơ hồ, đủ rủi ro không? |
| **Judgment** | Khi có 3 phương án, họ chọn thế nào, nói rõ trade-off không? |
| **Multiplier effect** | Team có giỏi lên nhờ họ không (review, mentoring, docs, process)? |
| **Reliability mindset** | Họ có nghĩ đến power loss, field failures, rollback, observability trước khi bị hỏi không? |
| **Technical depth** | Khi drill 4–5 tầng (e.g. `memory_order_acquire` → cache coherence → false sharing) họ có đứng vững không? |
| **Communication** | Giải thích được cho PM, cho junior, cho director với cùng một nội dung? |

> [!IMPORTANT]
> Với ứng viên ~5 năm ứng tuyển Staff: **depth + ownership + judgment** phải bù cho số năm. Bạn cần interviewer kết luận *"operates at Staff level already, just fewer years"* — chứ không phải *"strong senior"*.

---

## 2. Giải mã JD từng dòng

### Responsibilities

| JD line | Ý nghĩa thực tế | Cách thể hiện |
|---|---|---|
| *Own the technical direction for the squad, translating product goals into a clear engineering strategy* | Bạn viết tech strategy / roadmap; nối "product muốn X" thành "kiến trúc Y, milestones Z, rủi ro W" | Kể eCall: từ yêu cầu OEM (latency, retry, callback window) → state machine + HLD. AegisCam: [`tech_roadmap.md`](../aegiscam/docs/process/tech_roadmap.md) |
| *driving architectural decisions for embedded software systems* | ADR, build-vs-buy, interface boundaries | [ADR list](../aegiscam/docs/adr/), MultiCamera wrap DeepStream sau `IDeepStreamHAL` ([`../include/hal/i_deepstream_hal.h`](../include/hal/i_deepstream_hal.h)) |
| *Lead design and architecture reviews ... high bar on stability, performance, safety-critical quality* | Bạn chủ trì review, có checklist, dám nói "no" | [`design_review_template.md`](../aegiscam/docs/process/design_review_template.md) — failure modes, resource budget, rollback |
| *Write well-tested, maintainable code and lead technical design and code reviews that elevate standards* | Vẫn phải code (IC-heavy role), và review tạo chuẩn | AegisCam CI gates (spec §7), [`code_review_guidelines.md`](../aegiscam/docs/process/code_review_guidelines.md) |
| *Mentor and coach middle and senior engineers* | Mentoring **senior** khó hơn junior: không dạy syntax, mà dạy judgment, scope, influence | [`mentoring_framework.md`](../aegiscam/docs/process/mentoring_framework.md), file 12 |
| *Drive AI-assisted development practices ... quality gates, review protocols, reusable workflows ... practical and safe* | Họ muốn người **có quan điểm** về AI: không cuồng, không bài xích; có guardrail đo được | [`../aegiscam/docs/ai_assisted_development/`](../aegiscam/docs/ai_assisted_development/), file 11 |
| *Collaborate closely with Product Management, Design, cross-functional Engineering* | Device team phải làm với cloud (Evidence backend), mobile app, hardware, QA, firmware vendor | eCall: OEM, hardware, modem vendor, test team |
| *Identify and integrate the latest open source technologies ... while maintaining reliability in a mission-critical environment* | Đánh giá OSS: maturity, license, CVE history, maintainers, exit plan | WireGuard [ADR-0006](../aegiscam/docs/adr/0006-wireguard-over-ipsec-openvpn.md), Yocto, SBOM (file 09) |
| *Improve engineering processes, including long-term technical planning, sprint execution, operational readiness* | Bạn sửa process khi nó hỏng: planning, estimation, on-call, release readiness | [`operational_readiness.md`](../aegiscam/docs/process/operational_readiness.md) |

### Required skills

| JD line | Ý nghĩa | Rủi ro với bạn | Chiến lược |
|---|---|---|---|
| 10+ yrs C/C++/Go/Python | Kỳ vọng breadth + depth | **Gap ~5 năm** | §4 |
| Developing, debugging, **shipping** embedded on Linux/RTOS | "Shipping" = production, field issues, OTA, không phải demo | Thấp — eCall đã ship | Kể field defect triage eCall, Yocto image MultiCamera |
| Well-tested software: unit, integration, system | Họ sẽ hỏi "how do you test X without hardware" | Thấp | HAL + FakeClock + pytest system tests (file 06) |
| 5+ yrs mentoring / leading small teams | Kỳ vọng leadership proven | **Gap một phần** | §4 |
| CS fundamentals, run-time analysis, OOD, code quality verification | Có vòng coding | Trung bình — cần luyện | File 07 |
| Networking TCP/IP, VPN, DNS, DHCP, tunneling (plus) | Body cam upload qua dock / Wi-Fi / LTE / VPN khách hàng (police network) | Trung bình | File 08 + netagent |

### Preferred

| JD line | Ghi chú |
|---|---|
| Linux embedded + real-time constraints | File 04: PREEMPT_RT, SCHED_FIFO, priority inheritance |
| Consumer/professional hardware with tight security, low latency, high stability | Body cam = cả ba. eCall = latency + stability + safety |
| Follow latest developments, distinguish promising practices vs passing trends | Chuẩn bị 3 ví dụ (§6) |
| AI tools across SDLC with review/validation/safety guardrails | File 11 |

---

## 3. Evidence matrix — JD item → story/project proof

| JD competency | Story / proof (file 12 / 13) | Mức độ bằng chứng |
|---|---|---|
| End-to-end ownership | eCall Function Owner: requirement → HLD/LLD → code → test → field | **Production, mạnh** |
| Architecture decisions | MultiCamera: HAL abstraction, bounded queues, 4-phase shutdown; AegisCam: 6 ADRs | Production-like + portfolio |
| Safety-critical quality | ASPICE traceability, ISO 26262 awareness, eCall failure scenarios (power loss, no coverage) | **Production** |
| Stability / 24-7 | MultiCamera watchdog, EOS isolation, PTS liveness monitor; AegisCam watchdog supervisor | Production-like |
| Low latency | AegisCam capture thread SCHED_FIFO, lock-free SPSC; MultiCamera pad probe < 50 µs | Portfolio + measured |
| Security | AegisCam hash chain + HMAC, hardened systemd, WireGuard kill-switch | Portfolio |
| Testing (unit/integration/system) | AegisCam GoogleTest + `go test -race` + pytest; MultiCamera mock HAL tests ([`../tests/`](../tests/)) | Portfolio + project |
| Code reviews | eCall team reviews; review checklist AegisCam | Production |
| Mentoring | Kèm junior trong eCall team (điều chỉnh theo trải nghiệm thật), onboarding docs | Production, scope nhỏ |
| AI-assisted dev | Dùng AI tools để build AegisCam với policy/quality gates rõ ràng | Portfolio, **điểm khác biệt** |
| Cross-functional | eCall: OEM, hardware, modem vendor, QA; on-site Korea | **Production** |
| Networking | eCall: cellular/HTTP/retry; AegisCam netagent: failover, DNS, DHCP, WireGuard | Production (một phần) + portfolio |
| Research rigor | IEEE Xplore paper: vision-based landing, simulation-first validation | Published |

---

## 4. Gap strategy — trung thực nhưng tự tin

### Nguyên tắc
1. **Không bao giờ nói dối hoặc làm tròn số năm.** Recruiter đã thấy CV. Lie = loại ngay ở background check.
2. **Không xin lỗi.** Đừng mở đầu bằng "I know I don't have 10 years, but...". Hãy chuyển sang **scope & depth**.
3. **Reframe sang "years ≠ scope"**: số năm là proxy cho (a) đã thấy nhiều failure mode, (b) đã sở hữu thứ lớn, (c) đã giúp người khác giỏi lên. Chứng minh trực tiếp a, b, c.
4. **Chủ động đưa evidence**, rồi **mời kiểm chứng**: "Please drill into any of it."

### Câu hỏi: *"The role asks for 10+ years. You have about five. Why should we consider you for Staff?"*

> "That's fair, and I won't pretend the calendar says otherwise. What I'd ask you to evaluate is the scope I've operated at. As Function Owner for eCall, I wasn't implementing tickets — I owned a safety-relevant feature end to end: negotiating requirements with the OEM, writing the HLD/LLD, making the threading and state-machine decisions, defining the test strategy, and being the person the field team called when a defect came in. That's the kind of ownership the 10-year bar is a proxy for.
>
> On top of that, I've deliberately compressed learning: I published peer-reviewed research, built a production-style edge analytics system on Jetson, and most recently designed AegisCam, a body-camera platform with written ADRs, CI quality gates and an AI governance policy — the artifacts a Staff engineer produces for a team.
>
> Where I'm still growing is breadth across many product generations, and I'd lean on the team for that. I'm confident in the depth — please drill into any part of it."

### Câu hỏi: *"Tell me about your experience mentoring or leading engineers."*

Cấu trúc trả lời (không phóng đại):
- **Formal scope**: số người, vai trò thật (ví dụ: "As Function Owner I coordinated the developers and testers working on eCall" — *điều chỉnh theo trải nghiệm thật của bạn: bao nhiêu người, bao lâu*).
- **Informal leadership**: onboarding newcomer, review threading code, giải thích AUTOSAR/CAN cho người mới.
- **Kết quả đo được**: newcomer tự close defect sau X tuần; review defects giảm; tài liệu onboarding được tái sử dụng.
- **Framework hiện tại**: GROW, "expand scope gradually", pairing → shadowing → reverse-shadowing (file 12).

> "Formally, I haven't held a people-manager title. In practice, as eCall Function Owner I was the technical point of contact for the engineers and testers on the feature — I reviewed their designs and code, onboarded newcomers onto CAN and the state machine, and ran the defect triage. *(Điều chỉnh: số người, thời gian.)* What I learned is that mentoring mid-level engineers is less about answers and more about giving them ownership of a decision and reviewing their reasoning. For senior engineers, it's about scope: helping them write the design doc that changes the team's direction, then stepping back."

### Những điều KHÔNG nói
- ❌ "I'm a fast learner" (rỗng).
- ❌ "Years don't matter" (nghe kiêu).
- ❌ Gọi portfolio project là "production at my company".
- ❌ Bịa team size, bịa tên công ty khách hàng.

---

## 5. Likely interview loop & cách chuẩn bị

> Loop thực tế của công ty sẽ khác; đây là pattern phổ biến cho Staff embedded ở các công ty thiết bị Mỹ có engineering center ở VN.

| Vòng | Thời lượng | Đánh giá | Chuẩn bị | File |
|---|---|---|---|---|
| **Recruiter screen** | 30' | Motivation, level fit, lương, notice period, tiếng Anh | Pitch 2', lý do ứng tuyển, khoảng lương có research | README, 01 |
| **Hiring manager** | 45–60' | Scope, leadership, culture, cách bạn làm việc với PM | 3 stories ownership + 1 conflict + 1 failure | 12 |
| **Coding** | 45–60' | DSA, code sạch, test cases, complexity | Luyện 12 bài file 07; nói to; viết test case trước | 07, 02 |
| **Embedded deep dive** | 60' | C++ memory model, RTOS/Linux, real-time, debugging, drivers | Drill 5 tầng mỗi topic | 02, 04, 05 |
| **System design (embedded)** | 60' | Requirements → architecture → failure modes → trade-offs | Framework file 10, luyện 3 walkthrough | 10, 08, 09 |
| **Behavioral / leadership** | 45–60' | Influence, mentoring, conflict, process improvement | STAR, mỗi story ≤ 3', có số liệu | 12 |
| **Project deep dive / bar raiser** | 60' | Tính trung thực, độ sâu, judgment | AegisCam 15', sẵn sàng bị hỏi "why not X" | 13 |
| **AI practices (có thể lồng vào HM/behavioral)** | 15–20' | Quan điểm + guardrails | 3' answer + ví dụ cụ thể | 11 |

### Mẹo cho từng vòng
- **Coding:** clarify → ví dụ → brute force → tối ưu → code → test bằng tay → complexity. Với embedded, nhắc *"no dynamic allocation after init"* khi phù hợp.
- **Deep dive:** khi không biết, nói phương pháp tìm ra ("I'd check with `perf sched` / read the kernel doc for …"). Không đoán bừa.
- **System design:** dành 5–8' cho requirements & numbers. Interviewer Staff đánh giá phần này nặng nhất.
- **Behavioral:** dùng "I" cho việc bạn làm, "we" cho việc team làm. Luôn có "what I'd do differently".

---

## 6. "Promising practices vs passing trends" — chuẩn bị quan điểm

JD hỏi trực tiếp điều này. Chuẩn bị 3 ví dụ có lập luận:

| Công nghệ / practice | Đánh giá | Lập luận ngắn |
|---|---|---|
| **Rust cho embedded / kernel** | Promising, adopt có chọn lọc | Memory safety loại bỏ ~70% class CVE trong C/C++ (số liệu công khai của Microsoft/Chromium). Rust-for-Linux đã merge. Nhưng: toolchain qualification, team skill, FFI cost. → Bắt đầu với component mới, tách biệt, parser untrusted input. |
| **eBPF cho observability on-device** | Promising | Low-overhead tracing production-safe; CO-RE giảm phụ thuộc kernel headers. Cần kernel ≥ 5.x, BTF. |
| **SBOM + reproducible builds (Yocto)** | Must-have, không còn là trend | Regulatory pressure (US EO 14028, EU Cyber Resilience Act). Yocto sinh SPDX sẵn. |
| **Zephyr RTOS** | Promising cho MCU side | Vendor-neutral, Linux Foundation, device tree + Kconfig quen thuộc. |
| **WireGuard** | Mature, adopt | ~4k LOC kernel, crypto hiện đại, roaming tốt cho thiết bị di động ([ADR-0006](../aegiscam/docs/adr/0006-wireguard-over-ipsec-openvpn.md)). |
| **"AI writes all the code"** | Hype phần lớn | AI tăng tốc boilerplate/test/docs; nhưng code concurrency/safety cần human ownership. Đo bằng defect escape rate, không bằng LOC. |
| **Microservices trên MCU / thiết bị nhỏ** | Passing trend cho device | Overhead IPC, memory; với body cam, vài process tách theo failure domain là đủ (aegisd + netagent). |
| **Blockchain cho evidence integrity** | Passing trend | Hash chain + signed manifest + server-side anchoring đủ; blockchain thêm phức tạp mà không thêm trust model phù hợp. |

> **Model answer frame:** *"My filter is: does it remove a class of failures or a class of toil, is it maintained by more than one company, and do we have an exit plan? Rust passes for new parsers; blockchain for evidence doesn't — a hash chain with signed manifests anchored server-side gives the same tamper-evidence with far less complexity."*

---

## 7. Câu hỏi motivation thường gặp

**"Why this company / why public safety?"**
> "I've spent my career on devices where failure has real consequences — an eCall that doesn't connect after a crash. Body cameras and in-car systems are the same class of problem: the recording has to start in under a hundred milliseconds, survive a battery pull, and hold up in court. That combination of real-time, security and reliability is exactly where I want to go deeper — and the Ho Chi Minh City center means I can do that work at Staff level here." *(Điều chỉnh nếu công ty không phải Axon.)*

**"Why leave automotive?"**
> "Automotive gave me rigor — traceability, safety analysis. I want faster iteration cycles and a broader systems surface: connectivity, security, cloud integration, OTA at fleet scale. Public-safety devices have the rigor *and* the breadth."

**"Where do you see yourself in 3 years?"**
> "Owning the technical direction of a device platform — the person who writes the strategy doc, runs the architecture review, and whose mentees are leading their own components."

---

## 8. Self-assessment trước phỏng vấn

Chấm 1–5, ưu tiên ôn các mục < 4:

| Chủ đề | Điểm | Hành động |
|---|---|---|
| C++ memory model / atomics | | File 02 §3 |
| Lock-free SPSC từ trí nhớ | | Tự code lại |
| Linux scheduling, PREEMPT_RT | | File 04 §4 |
| RTOS concepts | | File 04 §7 |
| gdb / core dump workflow | | File 05 |
| Test strategy cho hardware deps | | File 06 |
| 12 coding problems | | File 07 |
| TCP internals, MTU, NAT | | File 08 |
| VPN/WireGuard, DHCP, DNS | | File 08 |
| Secure boot, chain of custody | | File 09 |
| System design framework | | File 10 |
| AI governance answer | | File 11 |
| 8 STAR stories | | File 12 |
| AegisCam 15-min talk | | File 13 |
