# AegisCam — Mission-Critical Embedded Evidence Camera Platform

**AegisCam** is a reference software platform for a mission-critical **body-worn / in-vehicle evidence camera**, engineered to demonstrate Staff / Lead Embedded Software Engineer practices across real-time Linux C++17, cryptographic chain-of-custody, resilient networking (Go), A/B OTA updates, hardware watchdog supervision, Yocto/systemd integration, and governed AI-assisted development.

---

## Architecture Overview

AegisCam splits device responsibilities across two cooperating daemons with strict fault and privilege boundaries:

1. **`aegisd` (`core/`, C++17)** — Real-time capture, lock-free SPSC ring buffer, keyframe-aligned pre-event circular buffer, deterministic table-driven recording state machine, debounced priority trigger arbiter, power-loss safe segment writer (`tmp` + `fsync` + atomic `rename`), SHA-256 hash chain + HMAC-SHA256 signed `manifest.json`, append-only hash-chained `audit.log`, A/B OTA slot manager with boot-counter rollback, and per-thread deadline watchdog supervisor.
2. **`netagent` (`netagent/`, Go 1.22)** — Multi-interface link manager (`eth > wifi > lte`) with failback hysteresis and metered-link policy, DNS health + captive-portal/hijack detection, DHCP lease state machine (`BOUND → RENEWING → REBINDING → INIT`), WireGuard VPN config renderer + handshake supervisor with kill-switch, and resumable chunked evidence uploader with per-chunk SHA-256, token-bucket bandwidth cap, and full-jitter exponential backoff.

Every hardware dependency in `aegisd` sits behind a pure C++ interface (`ICameraHal`, `IStorageHal`, `IWatchdogHal`, `IKeyStoreHal`, `IBootControlHal`, `IClock`), enabling 100% deterministic host simulation (`aegisd --sim`) driven by virtual time (`TICK <ms>`).

---

## Repository Layout

```text
aegiscam/
├── CMakeLists.txt                  # Top-level C++17 build (aegis_core, HALs, aegisd, tests, fuzz)
├── CMakePresets.json               # Presets: dev, asan-ubsan, tsan, coverage, release-aarch64
├── cmake/toolchain-aarch64.cmake   # ARM64 Linux cross-compilation toolchain
├── .clang-format / .clang-tidy     # Strict formatting & static analysis rules
├── core/                           # C++17 device daemon "aegisd"
│   ├── include/aegis/<module>/     # Public headers (common, util, crypto, hal, evidence, ...)
│   ├── src/<module>/               # Core module implementations
│   ├── hal/sim/                    # Deterministic simulation HALs (FakeClock, fault injection)
│   ├── hal/linux/                  # Embedded Linux HALs (/dev/watchdog, POSIX fsync, V4L2)
│   ├── apps/aegisd_main.cpp        # aegisd CLI & line-protocol server
│   ├── tests/unit/                 # GoogleTest unit test suite
│   ├── tests/integration/          # End-to-end DeviceController integration tests
│   └── fuzz/                       # libFuzzer target for IPC command parser
├── netagent/                       # Go 1.22 connectivity, VPN & resumable upload agent
├── tests/system/                   # Pytest black-box system tests driving `aegisd --sim`
├── tools/verify_evidence.py        # Offline chain-of-custody & audit log verifier (Python stdlib)
├── deploy/systemd/                 # Hardened systemd unit files (aegisd.service, aegis-netagent.service)
├── deploy/yocto/meta-aegiscam/     # Yocto layer recipes for aegisd, netagent, and read-only rootfs image
└── docs/                           # Architecture, ADRs, engineering process, AI governance
```

---

## Quick Start: Build & Test

> 📖 **Xem hướng dẫn cài đặt, biên dịch & vận hành chi tiết tại: [SETUP.md (How to Set Up)](./SETUP.md)**

### 1. C++17 Core (`aegisd` & GoogleTest suite)

```bash
# Configure and build (dev preset)
cmake --preset dev
cmake --build --preset dev

# Run unit + integration tests
ctest --preset dev

# Run with AddressSanitizer + UndefinedBehaviorSanitizer
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan

# Cross-compile for ARM64 Embedded Linux (aarch64-linux-gnu)
cmake --preset release-aarch64
cmake --build --preset release-aarch64
```

### 2. Go Connectivity & Upload Agent (`netagent`)

```bash
cd netagent
go test -race ./...
go build ./cmd/netagent
```

### 3. End-to-End System Tests & Chain-of-Custody Verifier

```bash
# Run black-box pytest suite against aegisd --sim
pytest tests/system

# Verify an evidence store directory offline
python3 tools/verify_evidence.py /path/to/store_root
```

---

## Alignment with Staff / Lead Embedded Software Engineer Requirements

| Area | Implementation in AegisCam |
|------|----------------------------|
| **Embedded Linux & C++17** | `SCHED_FIFO` real-time capture/supervisor design, lock-free `SpscRingBuffer`, bounded MPMC queues, zero heap/lock on capture path, V4L2 / `/dev/watchdog` / POSIX `fsync` HALs, Yocto layer & hardened `systemd` units |
| **Security & Chain of Custody** | FIPS 180-4 SHA-256, RFC 2104 HMAC-SHA256, cryptographic segment hash chain ($H_n = \text{SHA256}(H_{n-1} \parallel h_n)$), tamper-evident `audit.log`, offline `verify_evidence.py` |
| **Low Latency & Power-Loss Safety** | Keyframe-aligned pre-event buffer, < 100 ms trigger-to-record state machine, atomic `tmp` + `fsync` + `rename` segment & manifest writes |
| **Resilient Networking** | Go `netagent` with `eth > wifi > lte` hysteresis failover, DNS hijack/captive-portal detection, DHCP state machine, WireGuard supervisor + kill-switch, resumable chunked uploads |
| **OTA & 24/7 Reliability** | A/B slot manager with SHA-256 verification and 3-boot-attempt automatic rollback; per-thread heartbeat deadline supervisor gating hardware watchdog kicks |
| **Testing & AI Governance** | Deterministic `--sim` mode with `FakeClock`, unit/integration/system/fuzz tests, ASan/UBSan/TSan presets, and documented AI-assisted development quality gates (`docs/ai_assisted_development/`) |

---

## 📜 Design & Architecture Documentation Index

### Detailed System Design (`docs/design/`)
* [System Design (SDD)](docs/design/system_design.md) — 3-Layer & Dual-Process (`aegisd` + `netagent`) architecture, end-to-end data flow, RAM & eMMC flash A/B partition budget, and fault-tolerance matrix.
* [Software Design (SWDD)](docs/design/software_design.md) — Package decomposition, HAL interface contracts (`ICameraHal`, `IStorageHal`, `IWatchdogHal`, `IKeyStoreHal`, `IBootControlHal`), Dependency Injection wiring, IPC line protocol, and 4-tier testing architecture.
* [High-Level Design (HLD)](docs/design/high_level_design.md) — Module responsibilities inventory, subsystem interaction diagram, DI class diagram, sequence diagrams, and requirements traceability matrix (R1–R8).
* [Low-Level Design (LLD)](docs/design/low_level_design.md) — C++17 data structures, lock-free `SpscRingBuffer` memory layout & acquire/release ordering, table-driven FSM transition matrix, cryptographic chain-of-custody formulas, atomic filesystem writes, and Go `netagent` algorithms.
* [Process, Threading & Real-Time Resource Model](docs/design/threading_and_process_model.md) — Why `aegisd` (C++17) and `netagent` (Go 1.22) run as isolated OS processes, SoC ARM64 CPU affinity map, thread & goroutine inventory (`SCHED_FIFO` vs `SCHED_OTHER`), priority inversion avoidance, `WatchdogSupervisor`, and 4-phase graceful shutdown.
* [End-to-End System Workflow Specification](docs/design/system_workflow.md) — Complete 12-step walkthrough of a full program lifecycle from boot to power-off with function-level code trace, data transformations, and millisecond timing budget.

### Architecture, ADRs, Process & AI Governance (`docs/`)
* **Architecture Specs**: [PROJECT_SPEC.md](docs/architecture/PROJECT_SPEC.md), [System Overview](docs/architecture/system_overview.md), [Threading & Real-Time](docs/architecture/threading_and_realtime.md), [Security Model](docs/architecture/security_model.md), [Networking](docs/architecture/networking.md)
* **Architecture Decision Records (ADRs)**: [ADR Index](docs/adr/README.md) (`0001` HAL & DI, `0002` Lock-Free Pre-Event Buffer, `0003` Segment Evidence & Hash Chain, `0004` A/B OTA Rollback, `0005` Go for Connectivity Agent, `0006` WireGuard over IPsec/OpenVPN)
* **Engineering Process & Leadership**: [Design Review Template](docs/process/design_review_template.md), [Code Review Guidelines](docs/process/code_review_guidelines.md), [Operational Readiness](docs/process/operational_readiness.md), [Tech Roadmap](docs/process/tech_roadmap.md), [Mentoring Framework](docs/process/mentoring_framework.md)
* **AI-Assisted Development Governance**: [AI Policy](docs/ai_assisted_development/policy.md), [Quality Gates](docs/ai_assisted_development/quality_gates.md), [Prompt Library](docs/ai_assisted_development/prompt_library.md), [Review Checklist](docs/ai_assisted_development/review_checklist.md), [Metrics](docs/ai_assisted_development/metrics.md)

