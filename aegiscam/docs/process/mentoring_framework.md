# Engineering Mentoring & Technical Growth Framework

As a Staff / Lead Embedded Software Engineer, multiplying team capability is as important as writing core code. This framework structures how we onboard, mentor, and grow engineers from L3 (Junior) to L5 (Senior) on the AegisCam platform.

---

## 1. Core Principles

1. **Architecture Teaches Good Habits:** Because every hardware dependency sits behind a pure HAL interface (`ADR-0001`) and `aegisd --sim` runs deterministically on a laptop, new engineers can write, break, and debug real firmware state machines on day 1 without waiting for a development board.
2. **Teach Invariants, Not Just Syntax:** Reviews and pairing sessions focus on *why* an invariant exists (e.g., why `capture` cannot allocate, why `manifest.json` uses `fsync` + atomic rename, why `TriggerArbiter` blocks lower-priority stops).
3. **Graduated Autonomy (See One $\to$ Pair One $\to$ Lead One):** Engineers progress from implementing well-scoped features with an existing ADR to authoring design docs (`docs/process/design_review_template.md`) and leading cross-component reviews.

---

## 2. Competency Matrix & Growth Milestones

| Competency Area | L3 (Embedded Engineer I) | L4 (Embedded Engineer II) | L5 (Senior Embedded Engineer) |
|-----------------|--------------------------|---------------------------|-------------------------------|
| **Real-Time & Concurrency** | Understands thread table (`SCHED_FIFO` vs `SCHED_OTHER`); avoids locks/allocations on capture path. | Designs bounded queues and lock-free pipelines; diagnoses TSan/ASan failures independently. | Defines system-wide latency budgets, worst-case execution time (WCET), and watchdog recovery policies. |
| **Evidence & Storage Safety** | Writes unit tests using `SimStorageHal` and `FakeClock`. | Implements crash-safe file persistence (`tmp + fsync + rename`) and fault-injection tests. | Owns cryptographic chain-of-custody evolution and cross-version OTA storage compatibility. |
| **Networking & Distributed Systems** | Adds table-driven tests and metrics to `netagent` packages. | Designs state machines with hysteresis, backoff with jitter, and injectable transports. | Architects end-to-end device-to-cloud protocols, VPN supervision, and fleet bandwidth policies. |
| **AI-Assisted Engineering** | Follows `policy.md` disclosure and writes falsifiable tests for AI-assisted diffs. | Reviews AI-assisted PRs critically; catches subtle concurrency or error-handling hallucinations. | Curates team `prompt_library.md`, refines CI quality gates, and tracks defect escape metrics. |

---

## 3. Structured Mentoring Cadence

- **30/60/90-Day Onboarding Track:**
  - *Days 1–30:* Build `aegisd` and `netagent` locally, run all 8 CI gates, trace a `TRIGGER holster` command from IPC through `RecordingStateMachine` to `verify_evidence.py`, and ship a small bugfix or test improvement.
  - *Days 31–60:* Own a medium feature end-to-end (design review doc $\to$ implementation $\to$ sanitizer/fuzz verification $\to$ E2E pytest).
  - *Days 61–90:* Shadow on-call / operational readiness drills and co-review PRs with a Senior/Staff mentor.
- **Bi-Weekly Architecture Clinic:** 45-minute team walkthrough of a real failure scenario (e.g., eMMC stall, captive portal hijack, A/B boot loop) using live `--sim` or Go test harnesses.
