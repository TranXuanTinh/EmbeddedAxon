# Threading, Real-Time Scheduling & Watchdog Architecture

## 1. Real-Time Requirements

- **R1 (Pre-event capture):** 30 seconds of encoded video frames must be retained continuously without drops at 30 fps (33.3 ms frame period).
- **R2 (Trigger-to-recording latency):** `< 100 ms` from physical trigger assertion to active incident creation.
- **R7 (24/7 stability):** Deadlocks, priority inversions, or hung I/O threads must be detected and recovered deterministically via hardware watchdog reset.

---

## 2. Thread Budget & Scheduling Policy (`aegisd`)

| Thread | Linux Policy / Priority | Period / Wakeup | Responsibility | Forbidden Operations |
|--------|-------------------------|-----------------|----------------|----------------------|
| `supervisor` | `SCHED_FIFO` prio `90` | 100 ms periodic | Evaluates per-thread monotonic heartbeat deadlines; kicks `/dev/watchdog0` iff all registered threads are fresh. | Blocking I/O, dynamic memory allocation, waiting on worker mutexes. |
| `capture` | `SCHED_FIFO` prio `80` | Frame-rate driven (33.3 ms @ 30 fps) | Reads encoded frame from `ICameraHal`, pushes into lock-free `SpscRingBuffer`, updates capture heartbeat. | Heap allocation (`new`/`malloc`), mutex locking, filesystem/network I/O, logging on fast path. |
| `controller` | `SCHED_OTHER` (nice `0`) | Event-driven (IPC + timer ticks) | Runs `TriggerArbiter`, `RecordingStateMachine`, `ABSlotManager`, and Unix socket IPC handler. | Synchronous multi-megabyte disk writes. |
| `writer` | `SCHED_OTHER` (nice `+5`) | Bounded queue notification | Drains `SpscRingBuffer` & `PreEventBuffer`, writes `seg_%06u.bin` via `tmp + fsync + rename`, updates `manifest.json` and `audit.log`. | Holding locks shared with `capture`. |

In addition, `mlockall(MCL_CURRENT | MCL_FUTURE)` (enabled via `LimitMEMLOCK=infinity` and `CAP_IPC_LOCK` in `aegisd.service`) prevents page-fault latency spikes on the real-time path.

---

## 3. Lock-Free Producer/Consumer Pipeline

```mermaid
flowchart LR
    CAM["ICameraHal::read_frame"] --> CAP["Capture Thread\n(SCHED_FIFO 80)"]
    CAP -->|"try_push (release)\nO(1), zero-lock"| SPSC["SpscRingBuffer<Frame, 2^k>"]
    SPSC -->|"try_pop (acquire)"| PEB["PreEventBuffer\n(30s window, keyframe-aligned drain)"]
    PEB -->|"Segment frames"| SEG["SegmentWriter\n(SCHED_OTHER nice +5)"]
```

- **`SpscRingBuffer<T>`:** Power-of-two capacity ring buffer using `std::atomic<size_t>` `head_` and `tail_` indices with `std::memory_order_acquire` and `std::memory_order_release` semantics. Cache-line padding (`alignas(64)`) separates producer and consumer indices to eliminate false sharing across ARM Cortex-A cores.
- **`PreEventBuffer`:** Maintains a sliding window of frames covering `pre_event_ms` (default 30,000 ms). When `drain()` is invoked on trigger, it scans forward to the oldest **keyframe** (`Frame::keyframe == true`) within the window so the first recorded segment is immediately decodable without missing reference frames.

---

## 4. Watchdog Supervisor Design

A common anti-pattern in embedded Linux systems is kicking `/dev/watchdog0` from a dedicated timer thread that does not check whether worker threads are actually making forward progress.

In AegisCam, `WatchdogSupervisor` requires every critical thread (`capture`, `controller`, `writer`) to register a deadline $D_i$ and call `beat(thread_id)` on each iteration:

$$\text{kick\_allowed}(t) \iff \forall i \in \text{Threads}, \quad (t - \text{last\_beat}_i) \le D_i$$

If `writer` stalls indefinitely on a hung flash controller or `controller` deadlocks, its heartbeat expires, `WatchdogSupervisor` stops kicking `IWatchdogHal`, and the hardware watchdog resets the SoC into a known-good state (with A/B boot counter protection if an OTA update was in flight).

---

## 5. Deterministic Simulation Mode (`--sim`)

Hardware timers and OS thread scheduling make timing-dependent tests flaky on shared CI runners. When launched with `aegisd --sim`:
- Time is controlled exclusively by an injected `FakeClock`.
- Advancing time via `TICK <ms>` deterministically synthesizes the exact number of camera frames (`fps * ms / 1000`, keyframe every 30 frames), processes state-machine timers, and flushes segments on the same thread before returning `OK {...}`.
- Every system test in `tests/system/test_system_e2e.py` executes in milliseconds with zero sleep-based flakiness.
