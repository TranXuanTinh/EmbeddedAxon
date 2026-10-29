# ADR-0001: Hardware Abstraction Layer (HAL) Interfaces and Constructor Dependency Injection

- **Status:** Accepted
- **Requirement(s):** R8 (Everything testable on a laptop), R7 (24/7 stability)

## Context

Embedded camera firmware frequently couples business logic (state machines, recording pipelines, OTA management) directly to Linux kernel interfaces (`/dev/video0` V4L2 ioctls, `/dev/watchdog0`, `/dev/tpmrm0`, U-Boot environment variables, and `clock_gettime`). Direct coupling makes unit and system tests require physical target hardware, slows down developer feedback loops, and makes fault injection (e.g., storage full, corrupted flash sector, watchdog stall) nearly impossible in CI.

## Decision

We define narrow, pure-virtual C++17 interfaces for every hardware and OS boundary in `core/include/aegis/hal/`:
- `ICameraHal`
- `IStorageHal`
- `IWatchdogHal`
- `IKeyStoreHal`
- `IBootControlHal`
- `IClock` (`core/include/aegis/common/clock.h`)

`DeviceController` receives all HAL pointers and `IClock` via constructor dependency injection. Two complete HAL families are maintained side by side:
1. `core/hal/linux/`: Production Linux implementations (sysfs/devfs watchdog, V4L2 stub, POSIX `fsync` file storage).
2. `core/hal/sim/`: Deterministic in-memory / host-filesystem simulation implementations driven by `FakeClock`.

## Alternatives Considered

1. **Link-time symbol substitution (`#ifdef SIM` or separate `.cpp` link targets):** Rejected because preprocessor conditionals rot quickly, prevent testing both HALs in the same binary, and hide dependencies inside global state.
2. **CRTP / template policy classes for all HALs:** While zero-overhead, virtual dispatch happens at frame rate (30 Hz) or segment rate (0.1 Hz), where a single vtable call (~2 ns) is completely negligible compared to frame copy and disk I/O, whereas templates would bloat compile times and header coupling.

## Consequences

- **Positive:** `aegisd --sim` runs deterministically on Linux, macOS, and Windows laptops; `TICK <ms>` advances simulated time without wall-clock sleeps.
- **Positive:** Every failure mode (disk full, bad OTA SHA-256,bit-flip tampering, missed heartbeat) is covered in CI (`ctest` and `pytest tests/system`).
- **Negative:** Slight boilerplate overhead to maintain `sim` and `linux` implementations when an interface changes.
