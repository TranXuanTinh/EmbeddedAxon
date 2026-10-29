# Engineering Design Review Template

> Use this template for any change touching real-time capture, evidence format, cryptography, OTA boot flow, or IPC/network protocols. If the decision is long-lived, also record a summary ADR in `docs/adr/`.

---

## 1. Metadata
- **Title:**
- **Author(s):**
- **Reviewer(s):** (Staff/Lead Embedded Engineer + Security Reviewer if touching `crypto/` or `ota/`)
- **Status:** `Draft | In Review | Approved | Superseded`
- **Target Spec Requirement(s):** `R1..R8 / §1..§7`

## 2. Problem Statement & Operational Context
- What field problem or requirement are we solving?
- What happens if we do nothing?

## 3. Non-Negotiable Constraints & Budgets
| Dimension | Budget / Constraint | Impact of This Design |
|-----------|---------------------|-----------------------|
| **Capture Thread (`SCHED_FIFO 80`)** | 0 heap allocs, 0 blocking locks, `< 1 ms` per frame | |
| **Trigger Latency (R2)** | `< 100 ms` end-to-end | |
| **RAM Footprint** | Bounded queues only; specify max bytes | |
| **Flash Write Amplification (R4)** | Specify `fsync` frequency and bytes/sec | |
| **Power-Loss Safety (R4)** | State invariant if battery is pulled mid-operation | |

## 4. Proposed Architecture & Data Flow
- Component diagram (`mermaid` flowchart or sequence diagram)
- Public C++ HAL/module interfaces or Go package APIs
- State machine transitions and error codes (`ErrorCode`)

## 5. Failure Modes & Effects Analysis (FMEA)
| Failure Scenario | Detection Mechanism | Recovery Action | Test Case Covering It |
|------------------|---------------------|-----------------|-----------------------|
| Sudden battery pull during segment write | `.tmp` file left on disk; `manifest.json` points to last sealed segment | Ignore/clean `.tmp` on boot; `verify_evidence.py` passes | |
| Flash storage full | `IStorageHal` returns `IoError` / `StorageFull` event | Seal current incident, transition to `Fault`, log audit event | |
| Worker thread deadlock | Heartbeat deadline exceeded in `WatchdogSupervisor` | Stop kicking `/dev/watchdog0` $\to$ hardware reset | |

## 6. Security & Chain-of-Custody Impact
- Does this change affect `manifest.json`, `audit.log`, key handling, or network exposure?
- How is tampering or spoofing detected?

## 7. Verification & Rollout Plan
- **Unit tests (`ctest` / `go test -race`):**
- **Sanitizers & Fuzzing (`asan-ubsan`, `tsan`, `libFuzzer`):**
- **System E2E tests (`pytest tests/system`):**
- **OTA rollback compatibility:** Can slot B roll back cleanly to slot A with the on-disk store format?
