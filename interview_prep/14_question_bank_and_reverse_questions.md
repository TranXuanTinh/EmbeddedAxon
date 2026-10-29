# 14. Rapid-Fire Question Bank (65+ Q&A) & Reverse Questions

> Dùng file này để **ôn tập tốc độ cao trong 48 giờ cuối** trước buổi phỏng vấn. Mỗi câu trả lời được cô đọng trong **2–4 câu sắc bén** đúng ngôn ngữ của một Staff Embedded Engineer. Sau khi đọc câu hỏi, hãy che phần trả lời và tự nói to bằng tiếng Anh trong 45 giây.

---

## Phần A: 65 Câu hỏi Kỹ thuật & Lãnh đạo theo từng Mảng của JD

### I. Modern C++ & Concurrency (12 câu)

**1. What is the difference between `std::memory_order_acquire`/`release` and `std::memory_order_seq_cst`?**
> `acquire`/`release` establishes a pairwise *happens-before* relationship between a specific atomic store and a subsequent load of that same variable, using lightweight barriers (`ldar`/`stlr` on ARM64). `seq_cst` additionally enforces a single global total order across *all* `seq_cst` operations on all variables, which requires full memory fences or more expensive instructions. In our SPSC ring buffer, producer and consumer synchronize on distinct monotonic indices, so `acquire`/`release` is both sufficient and faster.

**2. Why can false sharing degrade lock-free queue performance by 10×?**
> If `write_idx_` (written by the producer on Core 3) and `read_idx_` (written by the consumer on Core 1) sit on the same 64-byte cache line, every update invalidates the other core's L1 cache line under the MESI coherence protocol. Aligning each atomic index with `alignas(64)` places them on separate cache lines and eliminates cache-line ping-pong.

**3. Why is a data race Undefined Behavior in C++ rather than just reading a torn or stale value?**
> The C++ compiler assumes race-free programs when optimizing: it may keep variables in registers across loops, reorder accesses, or invent speculative reads/writes. A concurrent unsynchronized modification violates those assumptions and can corrupt control flow (e.g., jumping out of bounds on a switch jump table), not just data.

**4. When does `std::function` allocate on the heap, and why does that matter on a real-time thread?**
> `std::function` uses Small Buffer Optimization (typically 16–32 bytes); if a lambda captures more than that, `std::function` calls `operator new` under the hood. Calling `new` on a `SCHED_FIFO` thread acquires the global allocator mutex and can cause unbounded latency or priority inversion.

**5. Why did you choose `Result<T>` over C++ exceptions in `aegisd`?**
> Zero-cost exceptions have near-zero overhead on the happy path, but throwing an exception walks DWARF unwind tables, takes locks inside `libgcc`/`libunwind`, and has non-deterministic worst-case latency. Returning `Result<T>` with `[[nodiscard]]` makes error handling explicit in the type signature and completely deterministic.

**6. What happens if a `std::thread` object is destroyed while still `joinable()`?**
> Its destructor immediately calls `std::terminate()`, aborting the entire process. Every thread must either be explicitly joined on all exit paths (or wrapped in a `jthread`/RAII guard) during ordered shutdown.

**7. Why does `return std::move(local_var);` hurt performance instead of helping?**
> Returning a local variable by name allows the compiler to perform Named Return Value Optimization (NRVO), constructing the object directly in the caller's slot with zero copies and zero moves. Wrapping it in `std::move` changes the expression type to an rvalue reference, inhibiting NRVO and forcing at least a move constructor call.

**8. What is the difference between `volatile` and `std::atomic`?**
> `volatile` tells the compiler not to optimize away or reorder accesses to **memory-mapped I/O registers (MMIO)** relative to other `volatile` accesses on the *same* thread. It provides **zero atomicity and zero CPU memory barriers** across threads or cores. Inter-thread synchronization in C++ always requires `std::atomic` or a mutex.

**9. How do you prevent deadlocks when a path must acquire more than one lock?**
> First, design single-owner components or lock-free queues so most paths need at most one lock. When multiple locks are unavoidable, enforce a strict global **lock hierarchy** (always acquire Lock A before Lock B) or use `std::scoped_lock(mtx_a, mtx_b)` which uses a deadlock-avoidance algorithm (`std::lock`).

**10. What is the ABA problem in lock-free programming?**
> Thread 1 reads pointer `A` from a shared atomic, gets preempted; Thread 2 pops `A`, frees it, allocates a new node that happens to land at the same address `A`, and pushes it back; Thread 1 wakes up and its `compare_exchange` succeeds even though the underlying data structure changed. Note that our index-based `SpscRingBuffer` does not suffer from ABA because indices increment monotonically (`size_t`) and don't use CAS on recycled pointers.

**11. Why must move constructors and move assignment operators be marked `noexcept`?**
> Standard containers like `std::vector` only move elements during reallocation if the move constructor is `noexcept` (`std::move_if_noexcept`); otherwise, they fall back to copying to preserve the strong exception guarantee.

**12. How do you detect Undefined Behavior like signed integer overflow or misaligned pointer casts in CI?**
> Compile and run the test suite with `-fsanitize=undefined -fno-sanitize-recover=all` (`UBSan`), which instruments arithmetic, shifts, null references, and alignment checks and fails the build immediately on any violation.

---

### II. Go & Python in Embedded Systems (10 câu)

**13. Why is `CGO_ENABLED=0` important when building a Go binary for an embedded Yocto target?**
> It produces a purely static binary that does not link against the build host's `glibc`. That eliminates runtime dynamic linker mismatches (`GLIBC_2.3x not found`) on minimal Yocto images and makes deployment a single self-contained executable.

**14. How do you prevent the Go garbage collector from causing OOM kills on a 512 MB RAM device?**
> Set `GOMEMLIMIT` (e.g., `GOMEMLIMIT=32MiB`) in the systemd unit so the Go runtime triggers GC aggressively as it approaches the soft cap, and reuse large byte slices (such as our 4 MiB upload chunk buffers) instead of allocating per chunk.

**15. What is the most common cause of a goroutine leak in network code?**
> Spawning a child goroutine that writes to an **unbuffered channel** while the parent `select`s on both that channel and `ctx.Done()`. If `ctx.Done()` fires first and the parent returns, the child blocks forever on the channel send. Fix: always use a buffered channel of size 1 (`make(chan res, 1)`).

**16. What happens if you forget `defer resp.Body.Close()` in Go's `net/http`?**
> The underlying TCP connection cannot be returned to the transport's idle pool and the read/write goroutines stay alive, leaking both file descriptors and memory until the device runs out of sockets.

**17. Why did you add hysteresis to `LinkManager` in `netagent`?**
> When a patrol car is at the edge of station Wi-Fi coverage, signal strength fluctuates rapidly. Requiring $N$ consecutive successful probes before failing back from LTE to Wi-Fi prevents link flapping that would repeatedly tear down VPN sessions and stall uploads.

**18. How do you unit-test Go code that makes HTTP and DNS calls without touching the real network?**
> Inject interfaces or function types: `http.RoundTripper` (or a `ChunkClient` interface) for HTTP uploads, and a `LookupFunc(ctx, host)` function for DNS checks. Tests run entirely in memory in milliseconds under `go test -race`.

**19. Does Python's GIL prevent `pytest` from testing concurrent embedded daemons?**
> No. Our `pytest` system suite spawns `aegisd` as a separate OS process (`subprocess.Popen`) and communicates over pipes/sockets; furthermore, CPython releases the GIL around blocking OS I/O calls.

**20. Why did you write `tools/verify_evidence.py` using only the Python standard library?**
> A forensic verification tool used by auditors or field engineers must run on air-gapped machines without requiring `pip install`, compiler toolchains, or internet access, and serves as an independent cross-language oracle against our C++ crypto implementation.

**21. Why use `hmac.compare_digest` in Python instead of `==`?**
> Standard string `==` short-circuits on the first mismatched character, leaking timing information. `hmac.compare_digest` runs in constant time relative to the digest length.

**22. When would you use `cgo` on an embedded device, and what are its downsides?**
> Only when Go must call a hardware C library that has no socket/file interface. Downsides: ~50–100+ ns overhead per call, OS thread pinning, loss of Go's memory safety across the C boundary, and requiring a C cross-compiler toolchain during builds.

---

### III. Embedded Linux, RTOS & Real-Time (12 câu)

**23. What is the difference between a soft real-time system and a hard real-time system?**
> In a hard real-time system (e.g., airbag deployment or flight control), missing a single deadline is a system failure that can cause physical harm. In soft real-time (e.g., live streaming UI), occasional deadline misses degrade quality; Note that evidence frame capture before buffer overrun sits right on the boundary where a missed deadline means permanently lost evidence.

**24. What are the three biggest changes `PREEMPT_RT` makes to the Linux kernel?**
> (1) Converts most kernel spinlocks into sleeping RT-mutexes with Priority Inheritance; (2) turns hardware IRQ handlers into schedulable kernel threads (`SCHED_FIFO`); and (3) uses high-resolution timers (`hrtimers`) for microsecond-level scheduling.

**25. Why must a real-time thread call `mlockall(MCL_CURRENT | MCL_FUTURE)`?**
> Without `mlockall`, the kernel can page out code or data pages, or allocate lazily on first touch. A single page fault on eMMC flash can stall a `SCHED_FIFO` thread for 5–20 ms, blowing past real-time deadlines.

**26. Explain Priority Inversion and how to fix it in two sentences.**
> Priority inversion occurs when a high-priority thread waits on a mutex held by a low-priority thread, and a medium-priority thread preempts the low-priority thread — stalling the high-priority thread indefinitely. Fix it either by using a **lock-free SPSC queue** (no mutex at all) or by enabling **Priority Inheritance (`PTHREAD_PRIO_INHERIT`)** on the mutex so the lock holder temporarily inherits the waiter's high priority.

**27. What is a Device Tree (`.dts` / `.dtb`), and why does ARM Linux need it?**
> Unlike PCI or USB buses, SoC buses (I2C, SPI, MIPI-CSI, GPIO, memory-mapped registers) are not self-discoverable at runtime. The Device Tree is a hardware description data structure passed by U-Boot to the kernel so the kernel knows which drivers to bind (`compatible`), at what physical addresses (`reg`), and on which interrupt lines (`interrupts`).

**28. How do you pass camera frames from kernel V4L2 drivers to user space without copying megabytes of pixels?**
> Use V4L2 `VIDIOC_REQBUFS` with `V4L2_MEMORY_MMAP` or **`V4L2_MEMORY_DMABUF`** so the camera ISP writes via DMA into physical contiguous/IOMMU buffers that are mapped directly into user space or passed straight to the hardware H.264 encoder FD (`zero-copy`).

**29. What happens if a `SCHED_FIFO` thread enters an infinite loop without blocking?**
> Because `SCHED_FIFO` has no time slice, it permanently starves every lower-priority thread (including `SCHED_OTHER` shells, `sshd`, and housekeeping) on that CPU core unless preempted by an even higher-priority RT thread or reset by the watchdog.

**30. How do you measure real-time scheduling latency on an Embedded Linux target?**
> Run `cyclictest --smp -p 80 -m -i 200` while generating heavy background CPU, disk I/O, and network load (`stress-ng` + file writes), and inspect the **maximum (`Max`) wakeup latency** and histogram tail, never just the average.

**31. In FreeRTOS or Zephyr, why can't you lock a standard Mutex inside an ISR?**
> An ISR has no task control block (TCB) and cannot enter the `Blocked` state to sleep if the mutex is already held by a task; doing so would crash the scheduler. ISRs must use non-blocking `...FromISR` queue/notification APIs.

**32. When would you add a small Cortex-M MCU running an RTOS alongside an Embedded Linux Cortex-A SoC?**
> For **ultra-low-power always-on sensing and instant wake-up**: monitoring the power button, holster Hall sensor, accelerometer, BLE wake beacons, and PMIC battery charging at microwatts while the Linux SoC is in deep suspend, waking Linux only when needed.

**33. What is the difference between `DEPENDS` and `RDEPENDS` in a Yocto BitBake recipe?**
> `DEPENDS` lists **build-time** dependencies (headers, libraries, host tools needed during `do_compile`), whereas `RDEPENDS:${PN}` lists **run-time** packages that must be installed into the target rootfs for the package to run.

**34. How do you check where a hung Linux thread is stuck in the kernel without attaching `gdb`?**
> Read `/proc/<pid>/task/<tid>/stack` and `/proc/<pid>/wchan` — they print the exact kernel call stack where the thread is currently sleeping (e.g., stuck in `blkdev_issue_flush` or `futex_wait_queue_me`).

---

### IV. Debugging, Storage, OTA & Field Reliability (11 câu)

**35. Why is writing a file via `open(O_TRUNC) -> write() -> close()` unsafe on a battery-powered device?**
> If power fails between `O_TRUNC` and the flash controller flushing dirty page-cache blocks, you lose both the old file and the new file, ending up with a truncated or zero-byte file. Safe pattern: write to `file.tmp` $\rightarrow$ `fsync(fd)` $\rightarrow$ `close(fd)` $\rightarrow$ `rename("file.tmp", "file")` $\rightarrow$ `fsync(dir_fd)`.

**36. Why is a dedicated thread that just loops `kick_watchdog(); sleep(1);` an anti-pattern?**
> It only proves that the timer thread is alive; if the capture thread deadlocks or the storage thread hangs forever, the watchdog still gets kicked and the device remains silently broken. A proper `WatchdogSupervisor` only kicks hardware if **every registered worker thread** has updated its heartbeat within its deadline.

**37. How do you symbolize a crash address from a stripped production binary?**
> Build with `-g -Wl,--build-id=sha1`, split debug symbols into `aegisd.debug` in CI, and strip the target binary. When a crash report arrives with the `Build-ID`, load address (`/proc/self/maps`), and instruction pointer (`PC`), subtract the ASLR base offset and run `aarch64-linux-gnu-addr2line -e aegisd.debug -f -C -p <offset>`.

**38. Why does AegisCam's A/B OTA manager wait for `aegisd` to call `mark_good()` instead of marking the slot good in U-Boot as soon as the kernel loads?**
> A kernel that boots into user space can still have a broken rootfs library or a crashing `aegisd` binary. Deferring `mark_good()` until `aegisd` initializes all HALs and passes its self-test ensures application-level regressions automatically roll back after 3 failed boots.

**39. How do you distinguish a true user-space heap leak from Linux page-cache growth?**
> Page cache (`Cached` in `/proc/meminfo`) grows whenever files are written and is automatically reclaimed by the kernel under memory pressure. A true process leak shows up as monotonically growing **`VmRSS` / `RssAnon`** in `/proc/<pid>/status` or `/proc/<pid>/smaps_rollup` even after `echo 3 > /proc/sys/vm/drop_caches`.

**40. What is eMMC wear leveling / write amplification, and how does it affect a camera that buffers 24/7?**
> NAND flash has a finite number of Program/Erase (P/E) cycles (~3,000 for TLC eMMC). If we wrote the 30-second pre-event buffer to eMMC continuously 24/7, we would burn through tens of gigabytes per day and wear out the flash prematurely. Keeping the pre-event buffer in **DRAM (`SpscRingBuffer`)** and only writing to eMMC when an incident is triggered preserves flash lifetime for years.

**41. What is `pstore` / `ramoops` in Embedded Linux?**
> A persistent RAM region (preserved across warm watchdog resets) where the kernel writes the tail of `dmesg` when a **kernel panic or oops** occurs, because the filesystem/eMMC driver cannot be trusted during a panic. On the next boot, user space reads `/sys/fs/pstore` to upload the panic trace.

**42. Why can `strace -p <pid>` break a real-time application?**
> `strace` uses `ptrace`, which stops the target process twice on every single system call (entry and exit) and context-switches to the `strace` process. That adds hundreds of microseconds per syscall and will cause `capture` frame drops or watchdog timeouts. Use `ftrace`, `perf trace`, or eBPF instead in production.

**43. What happens during a 4-phase graceful shutdown of a recording daemon?**
> (1) Stop accepting new external triggers and stop the producer (`capture` thread); (2) drain remaining frames from the ring buffer into the active segment; (3) `fsync` and atomically seal the segment, `manifest.json`, and `audit.log`; (4) join all worker threads and close HAL resources cleanly.

**44. How do you prevent an OTA rollout from bricking or degrading an entire fleet of 100,000 cameras?**
> Use **staged rings** (Lab Soak $\rightarrow$ 0.5% Canary $\rightarrow$ 5% $\rightarrow$ 25% $\rightarrow$ 100%) with automated telemetry gates that halt the campaign immediately if `ota_rollback_rate` or `watchdog_reset_rate` increases above baseline.

**45. Under what conditions is a camera allowed to reboot into a newly staged OTA slot?**
> Only when `state == Idle` (never during `Recording` or `PostRecord`), preferably when docked or on charger with battery SoC $\ge 40\%$, and with no critical un-uploaded evidence pending if policy requires offload first.

---

### V. Networking & Security (12 câu)

**46. Walk through DHCP DORA and the exact percentages for T1 and T2.**
> **D**iscover (broadcast) $\rightarrow$ **O**ffer $\rightarrow$ **R**equest (broadcast) $\rightarrow$ **A**ck. At **T1 = 50%** of the lease duration, the client enters `RENEWING` and sends a unicast `DHCPREQUEST` to the original server. If unacknowledged by **T2 = 87.5%**, it enters `REBINDING` and broadcasts `DHCPREQUEST` to any server; at **100%**, the lease expires (`INIT`) and the IP must be dropped immediately.

**47. Why do small HTTP requests (`STATUS`) succeed over a VPN while 4 MiB video chunk uploads hang at 0%?**
> Classic **Path MTU Discovery (PMTUD) Black Hole**: VPN encapsulation (60–80 bytes for WireGuard) reduces the effective MTU below 1500 bytes; large TCP packets with the `DF` bit set get dropped by a router whose ICMP `Fragmentation Needed` messages are blocked by a firewall. Fix: set `wg0` MTU to `1360` and clamp TCP MSS (`--clamp-mss-to-pmtu`).

**48. How does `netagent` detect a Wi-Fi captive portal or DNS hijacking?**
> `dnscheck` resolves our known cloud ingest hostname and validates that the returned IP is not a private/loopback/CGNAT bogon address (e.g., `192.168.x.x` or `10.x.x.x`) and lies within expected prefixes, followed by verifying an authenticated WireGuard/mTLS handshake before declaring the link usable.

**49. Why did you choose WireGuard over OpenVPN or IPsec for an embedded camera?**
> WireGuard runs in the Linux kernel (~4,000 lines of code vs hundreds of thousands), uses `ChaCha20-Poly1305` which is fast even on ARM cores without AES instructions, stays completely silent on battery when idle, and supports **built-in roaming** when switching between station Wi-Fi and patrol-car LTE without tearing down the tunnel.

**50. What is a VPN Kill-Switch, and how is it enforced?**
> A policy that forbids any evidence upload traffic from leaving the device in cleartext or outside the VPN tunnel if `wg0` goes down. We enforce it both in `netagent` (`vpn.CanUpload()` returns `false` when tunnel state != `Up`) and at the OS routing/firewall level (`nftables` only allowing upload traffic out of interface `wg0`).

**51. How does your resumable chunked uploader recover from a dropped LTE connection at 80% of a 500 MB incident?**
> Files are split into 4 MiB chunks, each verified by its own SHA-256 header. On reconnect, `uploader` calls `QueryOffset(ctx, fileID)` on the server, divides the confirmed byte offset by the chunk size, and resumes uploading from the exact next chunk.

**52. Why is per-segment SHA-256 alone insufficient for legal chain of custody?**
> If segments are only hashed individually, an attacker with storage access could delete an incriminating middle segment (`seg_000002.bin`) and rewrite the manifest hashes. Chaining $H_i = \text{SHA256}(H_{i-1} \parallel S_i)$ and signing the final head with a hardware-backed device HMAC makes any deletion, reordering, or truncation mathematically detectable.

**53. What is the difference between Secure Boot and `dm-verity`?**
> Secure Boot verifies the cryptographic signature of the bootloader, kernel, DTB, and `initramfs` **at boot time**. `dm-verity` verifies a Merkle hash tree over every 4 KB block of the read-only root filesystem (`squashfs`/`ext4`) **on every read at runtime**, preventing offline tampering with `/usr/bin/aegisd`.

**54. Where should the device's HMAC / mTLS private key live in production?**
> Inside a hardware **TPM 2.0, Secure Element (e.g., ATECC608), or ARM TrustZone (OP-TEE)** behind `IKeyStoreHal`, provisioned at the factory so the raw private key bytes can never be extracted from the Linux filesystem.

**55. How does clock skew (e.g., a dead RTC coin cell battery) break networking and evidence, and how do you handle it?**
> If system time resets to `1970-01-01`, TLS certificate validation and WireGuard replay counters fail, and incident timestamps are wrong. We use `CLOCK_MONOTONIC` for all internal durations/timeouts (ring buffer, debounce, watchdog), maintain a persisted lower-bound timestamp on disk (`max(rtc_time, last_known_good_ts)`), and sync via authenticated NTP/HTTPS Date header before opening strict TLS sessions.

**56. How did you lock down `aegisd.service` in systemd?**
> We enabled `NoNewPrivileges=yes`, `ProtectSystem=strict`, `ProtectHome=yes`, `PrivateTmp=yes`, `ReadWritePaths=/var/lib/aegis /run/aegis`, and `RestrictAddressFamilies=AF_UNIX` — meaning the `aegisd` process is blocked by the kernel from ever opening a TCP/UDP network socket even if compromised.

**57. Why must GPLv3 libraries be carefully vetted on Secure Boot consumer/public-safety hardware?**
> GPLv3's "anti-tivoization" clause (Section 6) requires providing "Installation Information" (signing keys/methods) allowing users to install modified software on consumer devices, which directly conflicts with locked-down Secure Boot requirements for evidentiary integrity.

---

### VI. Testing, AI-Assisted Dev & Staff Leadership (8 câu)

**58. How do you test a 30-second pre-event buffer and a 5-second post-record timer in under 1 millisecond?**
> Inject an `IClock` interface into every time-dependent class and pass a `FakeClock` in unit/integration tests. Calling `clock.advance(30'000)` advances virtual time deterministically in nanoseconds without sleeping.

**59. Why did you add a `libFuzzer` target for `command_protocol.cpp`?**
> Any C++ string/protocol parser is high-risk for out-of-bounds reads or integer overflows, especially when AI tools assist with boilerplate. Coverage-guided fuzzing (`-fsanitize=fuzzer,address,undefined`) explores edge cases that human-written unit tests miss.

**60. What is your #1 rule for AI-assisted PRs in an embedded squad?**
> **The human author owns 100% of the code.** If an engineer cannot explain why an atomic memory order is safe or what happens on power loss during code review, the PR does not merge — regardless of whether a human or an AI typed it.

**61. What metrics tell you whether AI coding tools are actually helping or silently creating technical debt?**
> Pair velocity metrics (PR lead time) with **quality counter-metrics**: **14-day code churn** (lines rewritten shortly after merge), sanitizer/fuzz failure rates in CI, and escaped defects to HIL/field. High velocity with rising 14-day churn means AI is generating brittle code.

**62. How do you decide between "Build vs Buy / Integrate Open Source" for an embedded component?**
> Build in-house what defines your **core domain invariants and safety boundaries** (e.g., recording state machine, trigger priority arbiter, evidence manifest rules, HAL interfaces). Integrate proven open-source for standardized infrastructure (e.g., WireGuard in kernel, Go's `crypto/tls`, SQLite/Yocto), wrapped behind your own HAL interface so you can swap it if maintenance or licensing changes.

**63. How do you mentor a Senior Engineer vs a Mid-Level Engineer?**
> With a Mid-level engineer, I coach **execution and craft** (RAII, lock-free/state-machine idioms, deterministic testing). With a Senior engineer, I coach **scope, trade-offs, and influence**: writing ADRs with explicit downsides, designing interfaces that unblock 3 teammates in parallel, and leading design reviews.

**64. What do you check before declaring an embedded feature "Operationally Ready" to ship (`operational_readiness.md`)?**
> Five gates: (1) Resource budgets verified under soak (no RSS growth over 24h, `cyclictest` max latency within budget); (2) Failure modes tested (power-cut during write, disk full, network drop); (3) Observability in place (health metrics, structured audit log); (4) Security reviewed (least privilege, no secrets in logs); (5) Rollback path verified (A/B OTA rollback tested on HIL).

**65. In one sentence, why should we hire you for this Staff/Lead role with ~5 years of experience?**
> Because I have already operated as the end-to-end **Function Owner** of a safety-critical automotive system in production, and I backed that up by building **AegisCam** — complete with lock-free real-time C++, a Go connectivity agent, cryptographic chain of custody, ADRs, and an AI governance framework — proving I bring the exact technical depth, reliability mindset, and team-multiplier habits this squad needs on day one.

---

## Phần B: Câu hỏi Ngược (Reverse Questions) để hỏi Interviewer ở cuối buổi

> [!TIP]
> **Chiến lược:** Ở level Staff/Lead, câu hỏi bạn đặt ra cho interviewer cũng là một bài đánh giá! Đừng hỏi những câu chung chung như *"Một ngày làm việc của anh thế nào?"*. Hãy hỏi những câu cho thấy bạn đã suy nghĩ như một đồng nghiệp sắp bắt tay vào giải quyết bài toán của họ.

### 1. Hỏi Hiring Manager / Engineering Manager (Về định hướng Squad, Bài toán 6 tháng tới & AI Adoption)
1. *"Looking at the next 6 to 12 months for this squad, what is the hardest technical or architectural problem on the roadmap — is it around real-time capture/power budgets on new hardware, connectivity/offload reliability in the field, or scaling fleet stability?"*
2. *"The JD highlights driving safe AI-assisted development across the squad — something I'm really passionate about and built a governance framework for in AegisCam. Where is the team currently on that journey, and what has been the biggest friction point in making AI tools safe for embedded code here?"*
3. *"What does success look like in the first 90 days for the person stepping into this Staff/Lead role so that you look back and say 'that was a great hire'?"*

### 2. Hỏi Senior / Principal Engineers trong vòng Coding / System Design (Về Kiến trúc, HIL & Debug thực tế)
1. *"How do you currently balance fast host-based simulation testing on laptops versus Hardware-in-the-Loop (HIL) testing in CI? What's the toughest hardware dependency to mock or simulate today?"*
2. *"When a tough reliability issue happens in the field — say a rare watchdog reset or an upload stall on an agency's network — what does the diagnostic data pipeline look like from the device back to the squad?"*
3. *"How is the boundary drawn today between the real-time media/recording services and the network/cloud offload services on the device?"*

### 3. Hỏi Director of Engineering / Bar Raiser (Về Chiến lược Nền tảng & Văn hoá Kỹ thuật)
1. *"As devices add more edge capabilities — like real-time streaming, LTE, and on-device AI — while still needing 12-hour battery life and rock-solid evidentiary integrity, how does the organization think about platform reuse across device generations (e.g., body-worn vs in-car)?"*
2. *"How do Staff and Lead engineers across different squads align on shared architectural standards, ADRs, and operational readiness?"*
