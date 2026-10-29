# 12. Technical Leadership, Mentoring & Behavioral STAR Bank

> **Interviewer đang đánh giá gì?** Hãy nhìn 6 dòng đầu tiên của JD:
> 1. *Own the technical direction for the squad, translating product goals into a clear engineering strategy and driving architectural decisions for embedded software systems.*
> 2. *Lead design and architecture reviews, setting a high bar on stability, performance, and safety-critical quality.*
> 3. *Write well-tested, maintainable code and lead technical design and code reviews that elevate the team's engineering standards.*
> 4. *Mentor and coach middle and senior engineers, helping them grow in technical depth, ownership, and engineering judgment.*
> 5. *Collaborate closely with Product Management, Design, and cross-functional Engineering teams to deliver impactful, end-to-end solutions.*
> 6. *Improve engineering processes, including long-term technical planning, sprint execution, and operational readiness.*
>
> Vòng Behavioral / Hiring Manager / Bar Raiser sẽ kiểm tra xem bạn có thực sự hành xử như một **Multiplier (người nhân bản năng lực cho cả đội)** hay chỉ là một "Lone Wolf" (kỹ sư giỏi nhưng làm một mình).

Liên hệ tài liệu quy trình lãnh đạo kỹ thuật đã viết trong AegisCam:
- Mẫu Design Review cấp Staff: [`aegiscam/docs/process/design_review_template.md`](../aegiscam/docs/process/design_review_template.md)
- Chuẩn mực Code Review: [`aegiscam/docs/process/code_review_guidelines.md`](../aegiscam/docs/process/code_review_guidelines.md)
- Danh mục sẵn sàng vận hành (Operational Readiness Checklist): [`aegiscam/docs/process/operational_readiness.md`](../aegiscam/docs/process/operational_readiness.md)
- Lộ trình kỹ thuật dài hạn (Tech Roadmap): [`aegiscam/docs/process/tech_roadmap.md`](../aegiscam/docs/process/tech_roadmap.md)
- Khung huấn luyện kỹ sư Mid/Senior (Mentoring Framework): [`aegiscam/docs/process/mentoring_framework.md`](../aegiscam/docs/process/mentoring_framework.md)

---

## 1. Năm Trụ cột Lãnh đạo Kỹ thuật (Staff / Tech Lead Playbook)

### 1.1 Làm chủ Định hướng Kỹ thuật (Owning Technical Direction & Roadmap)
Làm sao biến mục tiêu của Product Manager (*"Cảnh sát không bao giờ mất khoảnh khắc quan trọng và video phải tự động có mặt trên cloud khi về đồn"*) thành chiến lược kỹ thuật?
1. **Dịch yêu cầu sản phẩm sang Invariants (Bất biến hệ thống) & SLAs có con số:**
   - *"Không mất khoảnh khắc"* $\implies$ Pre-event 30s trên RAM + Trigger latency < 100 ms + Power-loss safe segment 10s.
   - *"Bằng chứng tin cậy trước toà"* $\implies$ SHA-256 hash chain + HMAC manifest + Chained audit log.
2. **Quản lý Nợ kỹ thuật như một Danh mục đầu tư (Tech Debt as a Portfolio):**
   - Không xin PM *"cho team nghỉ 2 tháng chỉ để refactor"*. Thay vào đó, gắn việc trả nợ kỹ thuật với **rủi ro vận hành và tốc độ giao tính năng tiếp theo** (ví dụ: *"Đưa phần cứng sau lớp HAL tuần này tốn thêm 2 ngày, nhưng giúp toàn bộ 6 tính năng trong quý tới test tự động trên CI trong 5 giây thay vì xếp hàng chờ 2 chiếc board thật ở lab"*).
3. **Ghi lại các quyết định bằng ADR (Architecture Decision Records):**
   - Mỗi quyết định lớn (như [ADR-0001](../aegiscam/docs/adr/0001-hal-and-dependency-injection.md) đến [ADR-0006](../aegiscam/docs/adr/0006-wireguard-over-ipsec-openvpn.md)) chỉ dài 1–2 trang, ghi rõ: **Context $\rightarrow$ Options Considered $\rightarrow$ Decision $\rightarrow$ Consequences / What we give up**. Điều này giúp thành viên mới hiểu *tại sao* hệ thống được xây như vậy và tránh việc cả team tranh cãi lại cùng một vấn đề sau 6 tháng.

### 1.2 Chủ trì Design & Architecture Reviews ([`design_review_template.md`](../aegiscam/docs/process/design_review_template.md))
Khi chủ trì một buổi Design Review cho hệ thống nhúng, mình yêu cầu bản thiết kế trả lời 5 câu hỏi trước khi bàn về code:
1. **Non-Goals (Cái gì chúng ta chủ động KHÔNG làm trong phiên bản này?)**
2. **Resource Budgets:** Tốn bao nhiêu MB RAM ở trường hợp xấu nhất? Bao nhiêu % CPU? Tốc độ ghi Flash bao nhiêu MB/s (ảnh hưởng tuổi thọ TBW của eMMC thế nào)? Tốn bao nhiêu mW pin?
3. **Threading & Concurrency Model:** Hàm này chạy trên thread nào? Có chạm vào `capture` RT thread không? Thứ tự lấy lock là gì? Hàng đợi đầy thì `DropOldest` hay `Reject`?
4. **Failure Modes Table:** Điều gì xảy ra nếu rớt pin đúng dòng lệnh này? Nếu đĩa đầy (`ENOSPC`)? Nếu mạng đứt ở 99%? Nếu đồng hồ hệ thống nhảy lùi/tiến khi đồng bộ NTP?
5. **Testability & Rollback Plan:** Làm sao test tính năng này trên laptop với `FakeClock`? Nếu đẩy ra hiện trường bị lỗi, tắt bằng feature flag hay rollback A/B OTA thế nào?

### 1.3 Nâng chuẩn Code Review ([`code_review_guidelines.md`](../aegiscam/docs/process/code_review_guidelines.md))
- **Để máy bắt lỗi cú pháp, để người bắt lỗi kiến trúc:** Không tốn thời gian review dấu ngoặc hay đặt tên biến (đã có `clang-format` và `clang-tidy` chặn ở CI).
- Người review tập trung vào: (1) Lifetime & Ownership, (2) Concurrency & Memory Ordering, (3) Error Propagation & Recovery, (4) Power-Loss & Security invariants, (5) Chất lượng của Unit Test đi kèm.
- **Văn hoá phản hồi:** Phân biệt rõ **`[BLOCKING]`** (vi phạm an toàn bộ nhớ, thiếu test, có nguy cơ deadlock) và **`[NIT / OPTIONAL]`** (gợi ý phong cách, không chặn merge) để giữ tốc độ cho team.

### 1.4 Huấn luyện Kỹ sư Mid & Senior ([`mentoring_framework.md`](../aegiscam/docs/process/mentoring_framework.md))
> **Câu hỏi phỏng vấn cực hay:** *"How does mentoring a Senior Engineer differ from mentoring a Junior/Mid Engineer?"*
> - **Với Junior / Mid Engineer:** Họ cần học **cách thực thi đúng (How to build it right)** — kỹ thuật C++ RAII, cách dùng gdb/sanitizers, cách chia nhỏ task, cách viết unit test với `FakeClock` và hoàn thành một module rõ ràng.
> - **Với Senior Engineer (tiến lên Tech Lead / Staff):** Họ đã biết viết code tốt. Thứ họ cần là **Judgment, Scope & Influence (What to build and why)**:
>   1. Chuyển từ *"giải bài toán được giao"* sang *"định hình bài toán mơ hồ cùng PM và viết ADR trình bày 3 phương án với trade-off rõ ràng"*.
>   2. Học cách thiết kế biên giao diện (interfaces) để **3 kỹ sư khác có thể làm việc song song** mà không dẫm chân nhau.
>   3. Học cách dẫn dắt buổi Design Review, lắng nghe ý kiến trái chiều và đưa ra quyết định dứt khoát (Disagree and Commit) thay vì tự mình ôm hết code khó về viết một mình.

---

## 2. Ngân hàng 12 Câu chuyện Hành vi (STAR Stories) theo đúng Hồ sơ Thực tế

> [!IMPORTANT]
> **Nguyên tắc trung thực & linh hoạt:** Các câu chuyện dưới đây được xây dựng bám sát 3 trụ cột thật trong hồ sơ của bạn (**Function Owner eCall**, **MultiCamera Jetson**, **Nghiên cứu IEEE Xplore** và **AegisCam**). Hãy dùng số liệu và chi tiết khớp chính xác với trải nghiệm thực tế của bạn khi interviewer hỏi sâu.

---

### Story #1: End-to-End Technical Ownership dưới áp lực Safety-Critical (eCall Function Owner)
- **Competency:** Ownership, Architecture, Safety-Critical Quality.
- **Situation:** Trong dự án phát triển bộ điều khiển viễn thông ô tô (TCU) cho một OEM lớn, tính năng **eCall (Emergency Call)** là hạng mục quan trọng bậc nhất vì liên quan trực tiếp đến tính mạng hành khách khi xảy ra va chạm và chịu sự giám sát chặt chẽ của chuẩn ASPICE / ISO 26262. Lúc mình nhận vai trò **Function Owner**, yêu cầu từ phía OEM có nhiều điểm phức tạp về kịch bản gọi lại (callback window), xử lý mất nguồn chính chuyển sang pin dự phòng (backup battery), và tương tác đa luồng giữa CAN bus, Audio, GPS và Modem.
- **Task:** Chịu trách nhiệm kỹ thuật trọn vẹn (end-to-end) cho phân hệ eCall: từ phân tích yêu cầu hệ thống, viết HLD/LLD, thiết kế state machine đa luồng bằng C++, phối hợp với đội phần cứng/modem/test cho đến khi vượt qua kiểm định và đưa vào sản xuất.
- **Action:**
  1. Mình làm việc trực tiếp với System Architect và đại diện OEM để làm rõ mọi kịch bản biên (ví dụ: có tín hiệu va chạm mới khi đang trong thời gian chờ PSAP gọi lại thì ưu tiên thế nào), biến các yêu cầu văn bản thành **bảng chuyển trạng thái (State Transition Table)** không có vùng xám.
  2. Thiết kế kiến trúc tách biệt rõ luồng nhận tín hiệu CAN thời gian thực, luồng điều khiển máy trạng thái trung tâm và lớp trừu tượng giao tiếp với modem/audio, đảm bảo mọi yêu cầu đều có mã truy vết hai chiều (bidirectional traceability) từ Requirement $\leftrightarrow$ HLD/LLD $\leftrightarrow$ C++ Code $\leftrightarrow$ Test Case.
  3. Chủ trì các buổi review thiết kế và trực tiếp điều tra các lỗi khó nhất ở hiện trường (như lỗi race condition khi rớt mạng trong hầm).
- **Result:** Phân hệ eCall đạt 100% độ phủ truy vết yêu cầu, vượt qua kỳ đánh giá ASPICE và kiểm thử chứng nhận mà không có lỗi nghiêm trọng (zero critical open defects), trở thành một trong những module ổn định nhất của TCU.

---

### Story #2: Đưa ra Quyết định Kiến trúc Khó khăn & Thuyết phục Team (HAL Abstraction vs Tốc độ ngắn hạn)
- **Competency:** Architectural Judgment, Influencing without Authority, Testing Strategy.
- **Situation:** Khi xây dựng hệ thống **MultiCamera DeepStream** (và áp dụng tiếp vào **AegisCam**), áp lực ban đầu là phải có bản chạy demo thật nhanh bằng cách gọi trực tiếp các hàm C của GStreamer/DeepStream và V4L2 khắp nơi trong logic điều khiển. Tuy nhiên, mỗi lần sửa logic khôi phục lỗi hoặc state machine, việc test phải chạy trên thiết bị Jetson thật với 4 luồng RTSP, rất chậm và hay gặp lỗi ngẫu nhiên do mạng.
- **Task:** Thuyết phục mọi người đầu tư thiết kế lớp **Hardware Abstraction Layer (HAL)** (`IDeepStreamHAL`, `ICameraHal`, `IStorageHal`, `IClock`) ngay từ đầu mà không làm trễ tiến độ chung.
- **Action:**
  1. Thay vì tranh luận lý thuyết suông, mình viết một bản ADR ngắn và làm mẫu (prototype) trong 1 ngày cho thấy việc bọc pipeline sau interface mỏng chỉ tốn vài trăm dòng code và không hề ảnh hưởng hiệu năng (vì lệnh gọi đi theo cấp độ sự kiện/frame chứ không nằm trong vòng lặp từng pixel).
  2. Mình chứng minh lợi ích tức thì: nhờ có Mock/Sim HAL và `FakeClock`, mình viết bộ test kiểm tra kịch bản đứt luồng camera (EOS isolation) và 4-phase shutdown chạy xong trong **vài giây ngay trên laptop và GitHub Actions** mà không cần phần cứng GPU.
- **Result:** Toàn bộ dự án áp dụng mẫu HAL + Dependency Injection; thời gian phát hiện lỗi hồi quy giảm từ hàng giờ chờ flash/chạy trên board xuống còn vài giây trên mỗi commit, đồng thời bắt được nhiều lỗi deadlock lúc tắt máy trước khi lên phần cứng thật.

---

### Story #3: Mentoring & Nâng tầm Kỹ sư từ "Viết code chạy được" lên "Tư duy Hệ thống"
- **Competency:** Mentoring, Coaching, Raising the Bar.
- **Situation:** Trong nhóm phát triển phần mềm nhúng ô tô (eCall / TCU), có các kỹ sư trẻ rất chăm chỉ nhưng khi gặp bài toán đồng bộ hoá đa luồng hoặc xử lý timeout thường có thói quen thêm các biến cờ toàn cục (`bool is_busy`, `sleep(100ms)`) để "vá" lỗi trước mắt. Cách này khiến code ngày càng rối và sinh ra các lỗi race condition mới.
- **Task:** Giúp các bạn nâng cao tư duy thiết kế phần mềm nhúng an toàn (state machine, ownership, deterministic testing) mà không làm các bạn nản lòng hay cảm thấy bị soi mói trong code review.
- **Action:**
  1. Trong các buổi code review, thay vì chỉ phê bình *"Code này bị race condition"*, mình ngồi pair-programming 30 phút vẽ sơ đồ trình tự (sequence diagram) trên giấy để bạn tự nhìn thấy khoảnh khắc 2 thread cùng chạm vào biến cờ khi sự kiện mạng đến trễ.
  2. Mình hướng dẫn bạn tự tay重构 (refactor) đoạn cờ boolean đó sang mẫu **Table-Driven State Machine + Event Queue**, và yêu cầu bạn viết bản thiết kế nhỏ (mini-LLD) dài 1 trang liệt kê các failure modes trước khi code tính năng tiếp theo.
  3. Mình đúc kết các lỗi thường gặp thành tài liệu checklist chung cho nhóm (giống như [`code_review_guidelines.md`](../aegiscam/docs/process/code_review_guidelines.md) và [`mentoring_framework.md`](../aegiscam/docs/process/mentoring_framework.md) trong AegisCam).
- **Result:** Sau khoảng 2 tháng, các kỹ sư trong nhóm đã chủ động vẽ bảng trạng thái trước khi code, số vòng bình luận qua lại trên PR giảm rõ rệt, và một bạn sau đó đã có thể tự đứng ra bảo vệ thiết kế module của mình trong buổi review với khách hàng.

---

### Story #4: Giải quyết Bất đồng Kỹ thuật với Đồng nghiệp / Đội liên phòng ban (Disagree and Commit / Data-Driven)
- **Competency:** Conflict Resolution, Cross-Functional Collaboration.
- **Situation:** Khi tích hợp tín hiệu CAN và luồng điều khiển giữa tầng ứng dụng (Application) và tầng cơ sở (AUTOSAR BSW / Modem interface), đã có sự bất đồng giữa hai nhóm khi xảy ra lỗi mất gói tin trạng thái lúc khởi động nhanh (fast wake-up): nhóm BSW cho rằng ứng dụng khởi tạo chậm, còn nhóm ứng dụng cho rằng driver nuốt sự kiện đầu tiên.
- **Task:** Giải quyết tranh cãi một cách khách quan, tìm ra giải pháp kỹ thuật tốt nhất cho sản phẩm thay vì đổ lỗi giữa các bên.
- **Action:**
  1. Mình đề xuất dừng tranh luận cảm tính và thiết lập một buổi đo kiểm chung bằng **CANoe + GPIO/DLT timestamp logging** để đo chính xác đến từng mili-giây thời điểm bản tin CAN xuất hiện trên bus, thời điểm ngắt BSW chạy, và thời điểm callback ứng dụng được đăng ký.
  2. Dữ liệu chỉ ra rằng cả hai bên đều có một phần nguyên nhân: bản tin wake-up đầu tiên đến chỉ 15 ms sau khi MCU thức dậy, trước khi dịch vụ ứng dụng kịp hoàn tất việc đăng ký listener.
  3. Mình đề xuất giải pháp hai chiều: tầng BSW giữ một **latched initial state buffer** (lưu trạng thái bản tin đầu tiên), và tầng ứng dụng ngay khi khởi tạo xong sẽ chủ động **poll (đọc kéo)** trạng thái được chốt đó trước khi chuyển sang chế độ nhận sự kiện bất đồng bộ (push).
- **Result:** Cả hai nhóm vui vẻ đồng thuận vì số liệu hoàn toàn rõ ràng; lỗi mất sự kiện lúc khởi động được giải quyết triệt để 100% và mẫu thiết kế "Latched State + Initial Poll" trở thành chuẩn chung cho các dịch vụ khác trên TCU.

---

### Story #5: Thương lượng Yêu cầu & Trade-off với Product Management / OEM
- **Competency:** Product Collaboration, Pragmatism, Resource Constraint Trade-offs.
- **Situation:** Phía khách hàng / Product muốn thiết bị vừa giữ thời gian chờ nhận cuộc gọi lại (callback mode) rất dài sau sự cố, vừa liên tục gửi dữ liệu chẩn đoán/vị trí tần suất cao, trong khi dung lượng pin dự phòng (backup battery) khi nguồn chính của xe bị đứt rất hạn chế.
- **Task:** Bảo vệ yêu cầu cốt lõi (nhận được cuộc gọi cứu hộ từ tổng đài khẩn cấp PSAP) trong giới hạn vật lý của pin dự phòng.
- **Action:**
  1. Mình lập bảng tính công suất tiêu thụ (power budget) cho từng trạng thái của modem (Active TX/RX vs Idle paging) và quy đổi ra số phút sống sót thực tế của pin ở nhiệt độ âm (-20°C, khi dung lượng pin giảm mạnh).
  2. Trình bày với PM/khách hàng 3 phương án kèm con số rõ ràng: nếu giữ truyền dữ liệu liên tục thì pin chỉ trụ được 12 phút (không đạt chuẩn 60 phút chờ gọi lại!).
  3. Đề xuất phương án **Adaptive Power Profile**: khi phát hiện đang chạy bằng pin dự phòng, hệ thống chỉ gửi gói dữ liệu tối thiểu bắt buộc (MSD) lúc đầu, sau đó tắt toàn bộ telemetry phụ và đưa modem về chế độ chỉ lắng nghe cuộc gọi đến (paging mode).
- **Result:** Khách hàng đồng ý ngay khi nhìn thấy bảng tính toán công suất; sản phẩm vừa vượt qua bài kiểm tra thời lượng pin dự phòng ở nhiệt độ khắc nghiệt, vừa giữ trọn vẹn tính năng cứu hộ cốt lõi.

---

### Story #6: Đưa Phương pháp Nghiên cứu Bài bản vào Thực tế (Simulation-First từ bài báo IEEE Xplore)
- **Competency:** Innovation, Rigor, Simulation-First Engineering.
- **Situation:** Trong dự án nghiên cứu về điều khiển drone tự động bám mục tiêu và hạ cánh bằng thị giác máy tính (đã công bố trên **IEEE Xplore**), việc thử nghiệm thuật toán điều khiển và xử lý ảnh trực tiếp trên drone bay thật ngoài trời rất rủi ro (một lỗi nhỏ trong bộ ước lượng trạng thái hoặc mất dấu marker khi gió tạt có thể làm rơi hỏng toàn bộ phần cứng).
- **Task:** Xây dựng quy trình kiểm chứng an toàn, lặp nhanh và định lượng được sai số trước khi bay thật.
- **Action:**
  1. Thiết lập môi trường **Software-in-the-Loop (SITL) kết nối ROS + Gazebo + Pixhawk (PX4)** mô phỏng đầy đủ động lực học của drone, camera ảo và nhiễu gió.
  2. Giữ giao diện node điều khiển hoàn toàn giống nhau giữa mô phỏng và phần cứng thật: thuật toán không biết nó đang đọc ảnh từ Gazebo hay từ camera thật trên drone.
  3. Chạy hàng trăm kịch bản biên trong mô phỏng (mất marker đột ngột ở độ cao thấp, trễ khung hình camera) để tinh chỉnh bộ điều khiển trước khi ra sân bay thử.
- **Result:** Khi đưa lên drone thực tế, hệ thống hạ cánh chính xác ngay từ những lần bay đầu tiên mà không gặp sự cố rơi máy nào; công trình được bình duyệt và xuất bản trên **IEEE Xplore**, đồng thời hình thành triết lý *"Simulation-First + HAL Interface"* mà mình áp dụng xuyên suốt trong MultiCamera và AegisCam.

---

### Story #7: Dẫn dắt Áp dụng AI-Assisted Development An toàn (AegisCam Governance)
- **Competency:** AI Leadership, Quality Gates, Engineering Process.
- **Situation:** Khi sử dụng các công cụ AI coding để tăng tốc phát triển dự án **AegisCam**, mình nhận thấy AI có thể viết các module C++ và Go rất nhanh, nhưng lại từng sinh ra những lỗi tiềm ẩn cực kỳ nguy hiểm cho thiết bị nhúng: dùng `std::this_thread::sleep_for` trong test, quên đồng bộ hoá khi truy cập biến trạng thái giữa hai thread, hoặc so sánh HMAC bằng `operator==`.
- **Task:** Xây dựng một quy trình và bộ rào chắn kỹ thuật để bất kỳ kỹ sư nào trong squad dùng AI cũng đạt được tốc độ cao nhưng chất lượng đạt chuẩn safety-critical.
- **Action:**
  1. Viết `PROJECT_SPEC.md` đóng vai trò "Single Source of Truth" để cung cấp ngữ cảnh chuẩn xác cho AI (ngân sách real-time, cấm cấp phát động trên capture thread, dùng `Result<T>` và `IClock`).
  2. Thiết lập bộ tài liệu quản trị AI ([`docs/ai_assisted_development/`](../aegiscam/docs/ai_assisted_development/)) gồm: phân vùng rủi ro (Red Zone cho RT/Crypto/OTA yêu cầu 2 người duyệt), thư viện prompt chuẩn hoá, checklist review lỗi đặc thù của AI, và 8 cổng CI tự động (`clang-tidy`, `ASan+UBSan`, `TSan`, `go test -race`, `libFuzzer`, coverage gate $\ge 80\%$).
- **Result:** Hoàn thiện toàn bộ nền tảng C++17 + Go + Python với độ phủ test cao, chạy sạch tuyệt đối dưới mọi sanitizer và fuzzer, tạo ra một khuôn mẫu thực tế có thể áp dụng ngay cho squad tại công ty.

---

### Story #8: Làm việc Đa văn hoá & Phối hợp Quốc tế (On-site Hàn Quốc)
- **Competency:** Adaptability, Cross-team Communication, Execution under Pressure.
- **Situation:** Mình được cử đi công tác trực tiếp (on-site) tại **Hàn Quốc** để phối hợp cùng đội kỹ sư bản địa trong dự án phần mềm ô tô (Model-Based Design & AUTOSAR integration) với tiến độ gấp và rào cản ngôn ngữ/văn hoá làm việc.
- **Task:** Nhanh chóng nắm bắt kiến trúc hệ thống tại chỗ, đồng bộ quy trình kiểm thử/tích hợp giữa đội on-site và đội offshore tại Việt Nam.
- **Action:**
  1. Thay vì trao đổi qua email dài dòng dễ hiểu lầm, mình dùng sơ đồ khối, bảng tín hiệu K-Matrix và sóng đo CANoe trực quan để thảo luận kỹ thuật trực tiếp với kỹ sư Hàn Quốc.
  2. Viết lại tài liệu hướng dẫn tích hợp và quy ước kiểm tra chất lượng rõ ràng, tổ chức buổi đồng bộ ngắn hàng ngày để giải tỏa ngay các điểm nghẽn cho đội ở Việt Nam.
- **Result:** Rút ngắn thời gian vòng lặp tích hợp, hoàn thành toàn bộ các mốc bàn giao đúng hạn và xây dựng được sự tin tưởng rất cao từ phía đối tác Hàn Quốc.

---

## 3. Bảng Tham chiếu Nhanh: Câu hỏi Phỏng vấn $\rightarrow$ Chọn Story nào?

| Câu hỏi Behavioral của Interviewer | Story chính | Story dự phòng |
|---|---|---|
| *"Tell me about the most complex system you owned end to end."* | **Story #1** (eCall Function Owner) | **Story #7 / AegisCam** |
| *"Tell me about a tough architectural trade-off you drove."* | **Story #2** (HAL Abstraction + Sim) | [ADR-0005](../aegiscam/docs/adr/0005-go-for-connectivity-agent.md) (C++ vs Go split) |
| *"Tell me about a time you mentored an engineer and helped them grow."* | **Story #3** (Mentoring on State Machine & Concurrency) | [`mentoring_framework.md`](../aegiscam/docs/process/mentoring_framework.md) |
| *"Tell me about a technical disagreement with a peer or another team."* | **Story #4** (CAN wake-up race — data-driven resolution) | **Story #2** |
| *"Tell me about a time you pushed back on a PM or customer requirement."* | **Story #5** (eCall Backup Battery Power Budget) | Fleet OTA Evidence-First Bandwidth |
| *"Tell me about a nasty field bug or production incident you debugged."* | **File 05 Story #1** (eCall tunnel modem race) | **File 05 Story #2** (MultiCamera EOS isolation) |
| *"How have you used AI tools, and how would you lead AI adoption in our squad?"* | **Story #7** + **File 11** | — |
