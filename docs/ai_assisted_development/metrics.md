# AI-Assisted Engineering Metrics & Governance KPIs

To ensure AI-assisted development accelerates delivery **without** degrading firmware reliability or security, we track a balanced set of velocity, quality, and safety metrics across every release cycle.

---

## 1. Core Metrics Dashboard

| Category | Metric | Target / Guardrail | How Measured |
|----------|--------|--------------------|--------------|
| **Quality (Primary Guardrail)** | Post-merge defect escape rate (AI-assisted vs. human-only PRs) | $\le$ human-only baseline (`0` P0/P1 escapes) | GitHub issues linked to regressions (`git bisect` / blame on `ai-assisted` PRs) |
| **Verification Strength** | Critical module line coverage (`crypto`, `evidence`, `recording`, `ota`) | $\ge 80\%$ (hard CI gate), target $\ge 90\%$ | CI Gate 5 (`gcovr`) |
| **Sanitizer & Fuzz Health** | ASan/UBSan/TSan/Race/Fuzzer findings per 1,000 LoC | `0` open findings on `main` | CI Gates 3, 4, and 6 |
| **Review Rigor** | PR size & review turnaround for `ai-assisted` PRs | `< 400` LoC; 2 human approvals; `< 4h` time-to-first-review | GitHub PR analytics |
| **Velocity** | Cycle time from ADR/spec sign-off to merged E2E-tested feature | 30–40% reduction in boilerplate/test authoring time | Sprint tracking |

---

## 2. Anti-Patterns We Actively Monitor

1. **PR Size Inflation ("Code Dumping"):** If average PR size increases because an engineer generates 1,200 lines of AI code in one shot, reviewer comprehension drops sharply. **Action:** Reject oversized PRs and require decomposing into `< 400` LoC stacked PRs.
2. **High CI Churn on First Push:** If `ai-assisted` PRs repeatedly fail CI Gate 1 (`clang-tidy`), Gate 4 (`tsan`), or Gate 7 (`verify_evidence.py`) on push, engineers are relying on CI instead of running local presets first. **Action:** Update `prompt_library.md` with stricter constraints and require local `ctest` / `go test -race` before opening a PR.
3. **Coverage Without Assertions:** High line coverage where tests call functions without asserting on `Result<T>` error codes or output bytes. **Action:** Enforce mutation/fault-injection checks during code review (`review_checklist.md`).

---

## 3. Monthly Governance Retrospective

Once per month during the Architecture Clinic, the Staff/Lead Engineer reviews:
- Any bug caught in code review or sanitizer runs that originated from an AI suggestion.
- Updates to `.clang-tidy`, `fuzz_command_protocol`, `review_checklist.md`, and `prompt_library.md` so the entire team and its AI workflows permanently learn from every near-miss.
