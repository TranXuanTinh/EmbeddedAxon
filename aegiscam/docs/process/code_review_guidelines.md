# Code Review Guidelines

Code review in AegisCam is a primary engineering quality control for mission-critical public-safety software. Every pull request must leave the codebase safer, clearer, and better tested than before.

---

## 1. Review Turnaround & Sizing

- **Target PR size:** `< 400` lines of production code (excluding tests and docs). Large features must be stacked into incremental PRs (e.g., HAL interface $\to$ core logic + unit tests $\to$ wiring + E2E test).
- **Turnaround SLA:** First review response within **4 business hours**; prioritize unblocking teammates over starting new work.
- **Approval requirements:**
  - Standard PRs: **1 senior/lead approval** + all 8 CI gates green.
  - PRs labeled `ai-assisted` or touching `core/src/crypto/`, `core/src/ota/`, or `deploy/systemd/`: **2 human approvals** + explicit security/invariant attestation.

---

## 2. What Reviewers Must Check

### 2.1 Real-Time & Concurrency Correctness (`core/`)
- **Capture path purity:** Verify that no code reachable from `capture` (`SCHED_FIFO 80`) calls `new`, `malloc`, `std::vector::push_back` (without fixed reserve), `std::mutex::lock`, filesystem I/O, or blocking logging.
- **Bounded memory:** Every queue or buffer (`SpscRingBuffer`, `BoundedQueue`, `PreEventBuffer`) must have an explicit compile-time or constructor-configured capacity bound and a documented overflow policy.
- **Atomics & lock ordering:** Any `std::atomic` operation outside standard primitives must justify its memory order (`acquire`/`release` vs `seq_cst`) in a comment and pass the `tsan` CI preset.

### 2.2 Power-Loss & Chain-of-Custody Integrity (`evidence/`, `tools/`)
- **Crash-safe file updates:** Never overwrite `seg_*.bin` or `manifest.json` in place. Always write to `<name>.tmp`, call `fsync`, and atomically `rename`.
- **Error propagation:** Check every `Result<T>` / `Status` return value (`[[nodiscard]]`). Never silently swallow an `IoError` or `IntegrityError`.

### 2.3 Go `netagent` Resilience (`netagent/`)
- **Context propagation:** Every blocking network or timer operation must honor `ctx.Done()`.
- **Injectable boundaries:** Never call `http.DefaultClient`, `net.LookupIP`, or `time.Sleep` directly inside domain logic; inject `http.RoundTripper`, `dnscheck.Resolver`, and clock/sleep functions so tests run in milliseconds without real network sockets.

### 2.4 Test Quality
- **Falsifiability:** Ask *"If I invert the `if` condition or drop the `fsync`/hash update in the implementation, which test fails?"*
- **Zero sleep-based tests:** Tests must advance `FakeClock` (`TICK <ms>` in `--sim` or injected `Now` in Go) rather than sleeping on wall-clock time.
