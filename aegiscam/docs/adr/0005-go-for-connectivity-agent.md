# ADR-0005: Process Split — C++17 `aegisd` Core and Go 1.22 `netagent` Connectivity Daemon

- **Status:** Accepted
- **Requirement(s):** R1, R2 (Deterministic real-time capture), R5 (Multi-link failover, VPN, resumable upload), R7 (24/7 stability)

## Context

AegisCam combines two workloads with diametrically opposed engineering characteristics:
1. **Hard real-time video capture & cryptographic sealing (`aegisd`):** Requires `SCHED_FIFO` threads, `mlockall`, zero heap allocations on the capture path, and predictable sub-millisecond wakeup latency.
2. **Untrusted network I/O & cloud upload (`netagent`):** Parses HTTP headers, DNS replies, and DHCP leases; manages concurrent HTTP/TLS connections, retry timers, token-bucket rate limiting, and WireGuard supervision.

Running complex HTTP/TLS/DNS client stacks inside the same address space as the `SCHED_FIFO` capture daemon creates unacceptable memory-safety, latency, and blast-radius risks.

## Decision

We split the system across two OS processes communicating over a local Unix domain socket (`/run/aegis/aegisd.sock`) using the fuzzed ASCII line protocol (spec §5):
- **`aegisd` (C++17):** Owns camera capture, pre-event buffer, segment writer, HMAC manifest signing, OTA slot manager, and hardware watchdog supervisor. Runs under `aegis` user with `CAP_SYS_NICE` and `CAP_IPC_LOCK`.
- **`netagent` (Go 1.22, stdlib only):** Owns uplink failover (`internal/link`), DNS canary (`internal/dnscheck`), DHCP lease state (`internal/dhcp`), WireGuard supervisor (`internal/vpn`), and resumable chunked uploads (`internal/uploader`). Runs under `aegis-net` user with `MemoryMax=64M`, `CPUWeight=50`, and `ReadOnlyPaths=/var/lib/aegis/incidents`.

## Alternatives Considered

1. **Single monolithic C++ daemon with `libcurl` + `c-ares`:** Increases C++ attack surface on untrusted network bytes and risks thread pool/allocator contention with `capture`.
2. **Python connectivity daemon:** Higher RSS memory footprint (~35–50 MiB base) and slower startup/CPU efficiency on embedded ARMv8 compared to a static `CGO_ENABLED=0` Go binary (~6 MiB).

## Consequences

- **Positive:** Memory safety (bounds-checked slices, garbage collection) on all network-facing code; cgroup resource limits (`MemoryMax=64M`, `Nice=10`) guarantee `netagent` GC pauses can never affect `aegisd` `SCHED_FIFO` capture threads; crash or restart of `netagent` has zero impact on active recording.
- **Negative:** Two toolchains (CMake/C++17 and Go 1.22) in CI and Yocto recipes.
