# AegisCam — Project Specification (Source of Truth)

> Mission-critical **body-worn / in-vehicle evidence camera** device software platform.
> Built as a portfolio project aligned with a *Staff / Lead Embedded Software Engineer* JD
> (Linux-based embedded devices, tight security, low latency, high stability, networking,
> well-tested software, AI-assisted development governance, technical leadership).

This file is the **contract** shared by every module. If code and this spec disagree, fix one of them in the same PR.

---

## 1. Product context (fictional, inspired by public-safety devices)

A camera worn by an officer / mounted in a patrol car. Requirements that drive architecture:

| ID | Requirement | Consequence |
|----|-------------|-------------|
| R1 | Never lose the moment: **pre-event buffer** (default 30 s) must be included when recording is triggered | Lock-free ring buffer, bounded memory, real-time capture thread |
| R2 | Trigger→recording latency **< 100 ms** (button, holster sensor, peer "signal" broadcast, remote) | Event-driven state machine, no blocking I/O on capture path |
| R3 | **Chain of custody**: any modification/deletion of evidence must be detectable | SHA-256 per segment, hash chain, HMAC-signed manifest, append-only audit log |
| R4 | **Power-loss safe**: battery can be pulled at any time | Segment-based writes, `fsync` + atomic rename, manifest journal |
| R5 | Offline-first, upload when docked / on Wi-Fi / LTE through **VPN** | Go `netagent`: link failover, DNS/DHCP health, WireGuard supervisor, resumable chunked upload |
| R6 | **OTA with A/B slots and automatic rollback** | Boot-count based slot manager, signature/hash verification before switch |
| R7 | 24/7 stability | Hardware watchdog supervisor with per-thread heartbeats, bounded queues everywhere |
| R8 | Everything testable on a laptop | Every hardware dependency behind a HAL interface + `sim` implementations + injectable `IClock` |

---

## 2. Repository layout

```
aegiscam/
├── CMakeLists.txt              # top-level, C++17, options: AEGIS_BUILD_TESTS, AEGIS_ENABLE_FUZZ, AEGIS_SANITIZER
├── CMakePresets.json           # dev, asan-ubsan, tsan, coverage, release-aarch64
├── cmake/toolchain-aarch64.cmake
├── .clang-format / .clang-tidy
├── core/                       # C++17 device daemon "aegisd"
│   ├── include/aegis/<module>/*.h
│   ├── src/<module>/*.cpp
│   ├── hal/sim/*.h|.cpp        # simulation HALs (host)
│   ├── hal/linux/*.h|.cpp      # real Linux HALs (sysfs watchdog, V4L2 stub, file storage)
│   ├── apps/aegisd_main.cpp
│   ├── tests/unit/*.cpp        # GoogleTest (FetchContent)
│   ├── tests/integration/*.cpp
│   └── fuzz/fuzz_command_protocol.cpp  # libFuzzer
├── netagent/                   # Go 1.22 connectivity + upload agent
├── tests/system/               # pytest end-to-end tests driving `aegisd --sim`
├── tools/verify_evidence.py    # offline chain-of-custody verifier (stdlib only)
├── deploy/systemd/             # hardened unit files
├── deploy/yocto/meta-aegiscam/ # Yocto layer (recipes for aegisd, netagent, image)
├── docs/architecture/          # overview, threading & real-time, security model, networking
├── docs/adr/                   # Architecture Decision Records
├── docs/process/               # design review, code review, operational readiness, roadmap, mentoring
├── docs/ai_assisted_development/ # AI policy, quality gates, prompt library, review checklist
└── .github/                    # CI workflow + PR template
```

C++ namespace: `aegis` with sub-namespaces matching module folders (`aegis::crypto`, `aegis::evidence`, ...).
Include paths: `#include "aegis/crypto/sha256.h"`.

---

## 3. C++ core modules (`core/`)

| Module | Header(s) | Responsibility |
|--------|-----------|----------------|
| common | `result.h`, `clock.h`, `logger.h` | `Result<T>` / `Status` with `ErrorCode` enum (no exceptions on hot path); `IClock` (`now_ms()`), `SystemClock`, `FakeClock` (`advance(ms)`); minimal thread-safe logger |
| util | `spsc_ring_buffer.h`, `bounded_queue.h` | Lock-free single-producer/single-consumer ring (power-of-2 capacity, `std::atomic` acquire/release); blocking bounded MPMC queue with drop-oldest / reject policy + `close()` |
| crypto | `sha256.h`, `hmac.h`, `hash_chain.h` | Self-contained FIPS 180-4 SHA-256 (streaming `update/final`, hex helper), HMAC-SHA256 (RFC 2104), hash chain `H_n = SHA256(H_{n-1} ‖ segment_hash)` with genesis = 32 zero bytes |
| hal | `i_camera_hal.h`, `i_storage_hal.h`, `i_watchdog_hal.h`, `i_key_store_hal.h`, `i_boot_control_hal.h` | Pure interfaces. Key store returns device HMAC key (prod: TPM/secure element). Boot control: active slot, boot counter, mark-good |
| evidence | `frame.h`, `pre_event_buffer.h`, `segment_writer.h`, `evidence_store.h`, `audit_log.h` | `Frame{seq, ts_ms, keyframe, payload}`; pre-event buffer keeps last N ms of frames and on drain starts **from a keyframe**; segment writer writes `seg_%06u.bin` via tmp file + fsync + atomic rename; evidence store maintains `manifest.json` per incident and verifies it; audit log is append-only hash-chained text |
| recording | `recording_state_machine.h` | States: `Idle`, `Buffering`, `Recording`, `PostRecord`, `Fault`. Events: `PowerOn`, `Trigger(src)`, `Stop(src)`, `Tick`, `StorageFull`, `FaultCleared`. Pure logic, table-driven, no I/O; returns `Actions` list |
| trigger | `trigger_arbiter.h` | Debounce (default 50 ms) per source, source priority, ignore STOP from lower priority than the one that started recording (e.g. `remote` cannot stop a `holster` recording) |
| ota | `ab_slot_manager.h` | Slots A/B; `stage(image_path, expected_sha256)` → verify hash → mark other slot pending with `boot_attempts_left=3`; on boot, decrement; if 0 and not marked good → rollback; `mark_good()` |
| health | `watchdog_supervisor.h` | Threads register heartbeats with deadlines; supervisor kicks HW watchdog only if **all** heartbeats are fresh |
| ipc | `command_protocol.h` | Parse/format the line protocol of §5. Must never crash on arbitrary input (fuzzed) |
| app | `device_controller.h` | Wires everything together (DI via constructor, all HALs injected). Owns capture thread, writer thread, supervisor. Exposes `handle(const Command&) -> Response` |

### Threads (aegisd)
| Thread | Priority (prod) | Work | Never does |
|--------|-----------------|------|------------|
| capture | SCHED_FIFO 80 | `ICameraHal::read_frame` → SPSC ring | allocate, lock, file I/O |
| controller | SCHED_OTHER | IPC commands, state machine, triggers | block on disk |
| writer | SCHED_OTHER nice 5 | drain ring → segment writer → hash → manifest | — |
| supervisor | SCHED_FIFO 90 | heartbeats → watchdog kick | — |

In `--sim` mode time is driven by `FakeClock` + `TICK` commands, threads may be run deterministically (single-threaded step mode) so tests are reproducible.

---

## 4. On-disk evidence format

```
<store_root>/
├── audit.log                         # append-only, one record per line
├── incidents/<incident_id>/
│   ├── seg_000001.bin                # raw frames (sim: deterministic bytes)
│   ├── seg_000002.bin
│   └── manifest.json
└── ota/slot_state.json
```

`incident_id` = `INC-<device_id>-<start_ts_ms>`.

`manifest.json`:
```json
{
  "version": 1,
  "device_id": "AEG-0001",
  "incident_id": "INC-AEG-0001-1700000000000",
  "trigger_source": "holster",
  "pre_event_ms": 30000,
  "segments": [
    {"index": 1, "file": "seg_000001.bin", "bytes": 4096, "sha256": "<hex>", "chain": "<hex>"}
  ],
  "chain_head": "<hex of last chain value>",
  "hmac": "<hex HMAC-SHA256(key, chain_head_hex_ascii)>",
  "sealed": true
}
```
Chain rule: `chain_1 = SHA256(32 zero bytes ‖ raw(sha256_1))`, `chain_n = SHA256(raw(chain_{n-1}) ‖ raw(sha256_n))` (raw = 32 binary bytes).

`audit.log` line: `<seq>|<ts_ms>|<event>|<detail>|<prev_hash_hex>|<hash_hex>` where
`hash = SHA256(prev_hash_hex ‖ "|" ‖ seq ‖ "|" ‖ ts_ms ‖ "|" ‖ event ‖ "|" ‖ detail)` (ASCII), genesis prev = 64 `'0'`.

Sim device HMAC key (test only): ASCII `aegis-sim-device-key-DO-NOT-SHIP`.

`tools/verify_evidence.py <store_root> [--key <ascii>]` re-computes everything and exits 0 (OK) / 2 (tampered) / 1 (usage error).

---

## 5. IPC line protocol (stdin in sim mode, Unix socket `/run/aegis/aegisd.sock` in prod)

Requests (one per line, ASCII, max 512 bytes, tokens separated by single spaces):

| Command | Meaning |
|---------|---------|
| `TRIGGER <src>` | src ∈ `button\|holster\|signal\|remote` |
| `STOP <src>` | stop request from a source |
| `TICK <ms>` | advance sim clock (sim only), 1..60000 |
| `STATUS` | current status |
| `VERIFY` | verify all incidents + audit log |
| `NET <up\|down>` | link state reported by netagent |
| `UPLOAD_ACK <incident_id> <segment_index>` | segment safely in cloud |
| `OTA_STAGE <image_path> <sha256hex>` | stage update to inactive slot |
| `OTA_REBOOT` | simulate reboot (sim) |
| `OTA_MARK_GOOD` | confirm current slot healthy |
| `QUIT` | graceful shutdown (drain → seal → audit → exit 0) |

Responses: exactly one line per request:
- `OK {json}` e.g. `OK {"state":"Recording","incident":"INC-...","segments":2,"net":"up","slot":"A"}`
- `ERR <ERROR_CODE> <human message>` with codes: `PARSE_ERROR`, `INVALID_ARGUMENT`, `INVALID_STATE`, `IO_ERROR`, `INTEGRITY_ERROR`, `NOT_SUPPORTED`.

Asynchronous events (may be interleaved, prefixed `EVT `):
`EVT {"type":"segment_sealed","incident":"...","index":3,"path":"...","sha256":"..."}`,
`EVT {"type":"incident_sealed","incident":"...","segments":5}`.

`aegisd` CLI: `aegisd --sim --store <dir> --device-id AEG-0001 [--pre-event-ms 30000] [--segment-ms 10000] [--post-record-ms 5000] [--fps 30]`.
In sim mode frames are generated by `SimCameraHal` as `TICK` advances time (fps-based), keyframe every 30 frames.

---

## 6. Go netagent (`netagent/`, module `github.com/tinhtran/aegiscam/netagent`, Go 1.22, stdlib only)

| Package | Responsibility |
|---------|----------------|
| `internal/link` | Link manager: interfaces `eth > wifi > lte` priority, health score, **hysteresis** (must be healthy N consecutive probes before failback), metered-link policy (LTE: upload only high-priority incidents) |
| `internal/dnscheck` | Resolver health: query primary, fall back to secondary, cache with TTL, detect captive portal / DNS hijack (expected answer check) — resolver injectable for tests |
| `internal/dhcp` | Lease model: T1 (50%) renew, T2 (87.5%) rebind, expiry; parse `udhcpc`/`dhclient`-style lease file into struct; state machine BOUND→RENEWING→REBINDING→INIT |
| `internal/vpn` | WireGuard config renderer (`wg-quick` format) + tunnel supervisor: handshake age monitor, restart with backoff, kill-switch flag (no upload outside tunnel) |
| `internal/uploader` | Resumable chunked upload (default 4 MiB chunks), per-chunk SHA-256, server offset query, exponential backoff with full jitter, max in-flight, bandwidth cap via token bucket; transport injectable (`http.RoundTripper` / interface) |
| `internal/ipc` | JSON-lines/line-protocol client to aegisd (sends `NET up/down`, `UPLOAD_ACK`) |
| `cmd/netagent` | wiring, flags, signal handling (`SIGTERM` graceful) |

Every package has table-driven `_test.go` tests, using fakes (no real network).

---

## 7. Quality gates (CI)

1. Format (`clang-format --dry-run -Werror`, `gofmt -l`), lint (`clang-tidy`, `go vet`, `staticcheck` optional)
2. Build matrix: gcc + clang, Debug/Release, cross aarch64 (build only)
3. Unit + integration tests (`ctest`, `go test -race ./...`)
4. Sanitizers: ASan+UBSan, TSan jobs
5. Coverage gate: core ≥ 80 % lines for `crypto`, `evidence`, `recording`, `ota`
6. Fuzz smoke: 60 s libFuzzer on `command_protocol`
7. System tests: `pytest tests/system` against `aegisd --sim`
8. AI-assisted change gate: PR template checkbox + label `ai-assisted` requires 2 human approvals & tests touching changed lines (see `docs/ai_assisted_development/quality_gates.md`)

