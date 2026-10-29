# AI-Assisted Software Development Policy

> Governance policy for using Large Language Models (LLMs) and AI coding assistants on the AegisCam mission-critical evidence camera platform.

## 1. Guiding Principle: Human Accountability

AI assistants are force multipliers for boilerplate, table-driven test generation, documentation, and refactoring—**never a substitute for engineering judgment**. Every line of code committed to `aegiscam` has a single human author of record who is 100% accountable for its correctness, real-time determinism, memory safety, and security.

---

## 2. Risk-Tiered Usage Policy

We classify repository paths into three risk tiers with strict rules for AI assistance:

| Risk Tier | Modules / Paths | AI Usage Policy | Review & Verification Requirement |
|-----------|-----------------|-----------------|-----------------------------------|
| **Tier 1 — High Assurance (Safety & Crypto)** | `core/src/crypto/`, `core/src/ota/`, `core/src/health/`, `core/include/aegis/util/spsc_ring_buffer.h`, `deploy/systemd/` | AI may be used for test scaffolding, comments, or initial drafts, **never** merged without line-by-line human derivation of memory orders, cryptographic test vectors, and failure states. | **2 human approvals** (including 1 Staff/Security reviewer), 100% test coverage of changed lines, ASan+UBSan+TSan clean. |
| **Tier 2 — Core Production Logic** | `core/src/evidence/`, `core/src/recording/`, `core/src/trigger/`, `core/src/ipc/`, `netagent/internal/` | Allowed for implementation and unit tests when guided by `PROJECT_SPEC.md` and `prompt_library.md`. | **2 human approvals** when labeled `ai-assisted`; `>= 80%` line coverage; falsifiable unit + E2E tests. |
| **Tier 3 — Tooling, Simulation & Docs** | `core/hal/sim/`, `tests/`, `tools/`, `docs/` | Encouraged for rapid test case expansion, fuzz corpora, and documentation drafts. | Standard PR review + CI gates green. |

---

## 3. Mandatory Rules (Non-Negotiable)

1. **Zero Secrets or Proprietary Keys in Prompts:** Never paste production cryptographic keys, customer incident logs, internal IP addresses, or security vulnerability details into external AI tools. Use the public simulation key (`aegis-sim-device-key-DO-NOT-SHIP`) and RFC 5737 documentation IPs (`192.0.2.0/24`, `198.51.100.0/24`, `203.0.113.0/24`).
2. **Mandatory PR Disclosure:** Every PR containing AI-generated or AI-modified code must check `[x] This PR includes AI-assisted code` in `.github/pull_request_template.md` and carry the `ai-assisted` GitHub label (enforced by CI Gate 8).
3. **No Unverified Dependencies:** AI tools frequently hallucinate third-party packages or CMake `FetchContent` URLs. Both `core/` (stdlib + GoogleTest) and `netagent/` (Go 1.22 stdlib only) forbid unapproved external runtime dependencies.
4. **Tests Must Fail When Code Is Broken:** Never accept AI-generated tests that only test happy paths or mock out the unit under test. Every AI-assisted PR must include negative/fault tests (corrupted bytes, expired timers, full queues).
