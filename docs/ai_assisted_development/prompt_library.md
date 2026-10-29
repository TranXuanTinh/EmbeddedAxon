# Curated Prompt Library for Embedded & Connectivity Engineering

This library provides battle-tested prompt templates anchored to `docs/architecture/PROJECT_SPEC.md`. Always supply the relevant spec section and interface header as context so the assistant adheres to AegisCam's real-time, memory-safety, and dependency constraints.

---

## 1. Implementing a C++17 Core Module Under Strict Real-Time Constraints

```text
Context:
- Read `docs/architecture/PROJECT_SPEC.md` (§3 and §4).
- Target header: `core/include/aegis/<module>/<header>.h`.

Hard Constraints:
1. C++17 standard library only. Namespace `aegis::<module>`.
2. No exceptions on hot paths: use `aegis::Result<T>` / `aegis::Status` with `ErrorCode`.
3. Zero dynamic memory allocation, mutex locking, or blocking I/O on any method called from the `capture` thread.
4. Inject time via `aegis::IClock&` (`now_ms()`) and hardware via `aegis::hal::I*Hal&` interfaces.

Task:
Implement `<ClassName>` and write a GoogleTest suite in `core/tests/unit/test_<module>.cpp` covering:
- Normal nominal transitions
- Boundary conditions (capacity full, 0 bytes, max sequence number)
- Fault injection (HAL returning `ErrorCode::IoError` or `ErrorCode::IntegrityError`)
```

---

## 2. Writing Table-Driven Go `netagent` Packages (Zero External Dependencies)

```text
Context:
- Read `docs/architecture/PROJECT_SPEC.md` (§6).
- Module: `github.com/tinhtran/aegiscam/netagent`, Go 1.22 standard library ONLY.

Hard Constraints:
1. Never touch real network interfaces, real DNS servers, or wall-clock `time.Sleep` directly in unit tests.
2. Inject external boundaries via small interfaces or function fields (`Prober`, `Resolver`, `http.RoundTripper`, `Now func() time.Time`, `Sleep func(context.Context, time.Duration) error`).
3. Ensure concurrency safety under `go test -race ./...`.

Task:
Implement `internal/<pkg>` and table-driven tests in `internal/<pkg>/<pkg>_test.go` exercising hysteresis/backoff edge cases and context cancellation.
```

---

## 3. Adversarial Security & Fault-Injection Review Prompt

```text
Act as a Staff Embedded Security & Reliability Reviewer auditing the diff below against `docs/architecture/PROJECT_SPEC.md`.
List concrete bugs or risks in these 5 categories (with file:line references):
1. Real-time violation: Any allocation, lock, or unbounded loop reachable from `capture` (`SCHED_FIFO 80`).
2. Power-loss corruption: Any file write missing `.tmp` + `fsync` + atomic `rename`.
3. Chain-of-custody bypass: Any path where segment modification, deletion, or truncation fails to invalidate `manifest.json` or `audit.log`.
4. Parser / integer safety: Any unchecked integer conversion, signed overflow, or out-of-bounds slice/span access on untrusted input.
5. Missing falsifiable test: Specify the exact test case needed to prove the fix.
```
