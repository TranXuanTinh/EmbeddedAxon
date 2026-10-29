# AI-Assisted Code Review Checklist

Reviewers of any pull request labeled `ai-assisted` must walk through this checklist before approving.

---

## 1. Spec & Architectural Alignment
- [ ] Does the code match `docs/architecture/PROJECT_SPEC.md` exact wire formats, JSON field names, and error codes? (AI models frequently invent slightly different JSON keys or command names.)
- [ ] Are zero external dependencies added to `core/` or `netagent/go.mod`?
- [ ] Are all hardware and clock dependencies injected via HAL interfaces (`IClock`, `IStorageHal`, `http.RoundTripper`) rather than calling OS globals directly?

## 2. Embedded C++17 Safety (`core/`)
- [ ] **No hidden allocations on real-time path:** Check for accidental `std::string` concatenation, `std::function` heap boxing, or `std::vector` reallocation on the `capture` thread.
- [ ] **No exception throwing on hot paths:** Confirm `std::from_chars` or manual bounds-checked parsing is used instead of `std::stoi`/`std::stoul` (which throw `std::invalid_argument` / `std::out_of_range`).
- [ ] **Memory ordering & thread safety:** Verify every shared variable between threads is protected by a mutex or justified `std::atomic` acquire/release order, and verified under `tsan`.
- [ ] **Integer & buffer bounds:** Check for signed/unsigned comparison warnings, `size_t` underflow when subtracting lengths, and off-by-one errors in ring buffer wrap-around.

## 3. Cryptography & Crash Safety (`crypto/`, `evidence/`, `ota/`, `tools/`)
- [ ] **Binary vs. ASCII hashing:** Verify that `hash_chain` hashes the **raw 32-byte binary** digests (`SHA256(raw(H_{n-1}) || raw(sha256_n))`), whereas `HMAC` signs the **64-char lowercase hex ASCII** `chain_head`, and `audit.log` hashes the pipe-delimited **ASCII** record (per spec §4).
- [ ] **Constant-time MAC comparison:** Confirm HMAC comparison uses constant-time comparison (`hmac.compare_digest` in Python, constant-time byte XOR loop in C++) rather than `operator==` or `memcmp`.
- [ ] **Atomic persistence:** Confirm every segment and manifest write uses `<file>.tmp` $\to$ `fsync` $\to$ `rename`.

## 4. Test Falsifiability
- [ ] Did the author verify that breaking the production invariant (e.g., flipping a byte, skipping `fsync`, returning an error from the HAL) causes the newly added test to fail?
- [ ] Are all tests deterministic (zero `std::this_thread::sleep_for` or wall-clock `time.Sleep` calls)?
