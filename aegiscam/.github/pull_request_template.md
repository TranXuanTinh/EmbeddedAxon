## Summary

<!-- Explain the problem, the architectural trade-offs considered, and the solution. -->

## Related Requirement / ADR

- Spec Requirement(s): `R_ / §_`
- ADR (if applicable): `docs/adr/000X-...`

## Verification & Quality Gates

- [ ] Unit tests added/updated (`ctest` / `go test -race ./...`)
- [ ] Sanitizer clean (`asan-ubsan` and `tsan` presets)
- [ ] System E2E tests (`pytest tests/system`) and offline `tools/verify_evidence.py` verified
- [ ] Real-time & memory safety checked: zero heap allocations, locks, or blocking I/O added to `capture` thread (`SCHED_FIFO 80`)
- [ ] Security & chain of custody checked: no secret keys logged, `fsync` + atomic rename preserved on evidence/manifest writes

## AI-Assisted Development Disclosure (`docs/ai_assisted_development/policy.md`)

- [ ] This PR includes AI-assisted code (if checked, apply the `ai-assisted` label — requires **2 human approvals** and tests covering all changed lines)
- [ ] This PR is 100% human-authored

### AI Review Attestation (required if AI-assisted)

- **Modules touched by AI:** <!-- Note: `crypto/`, `ota/`, and key handling require line-by-line human security review -->
- **Prompt / pattern used:** <!-- Reference `docs/ai_assisted_development/prompt_library.md` if applicable -->
- **Author attestation:** I have read, understood, and verified every line of AI-suggested code in this PR, confirmed all invariants under fault/power-loss conditions, and verified that tests fail when the implementation is broken.
