# ADR-0003: Crash-Safe Segment Storage with Binary SHA-256 Hash Chain and HMAC Manifest

- **Status:** Accepted
- **Requirement(s):** R3 (Chain of custody), R4 (Power-loss safe)

## Context

Recording to a single monolithic MP4 file during a 2-hour incident is unsafe on a field device: if the battery is knocked loose at minute 59, an unfinalized container header can render the entire recording unplayable, and cryptographic signing can only happen at the end of the incident.

## Decision

1. **Fixed-Duration Segments (`seg_%06u.bin`, default 10 s):** Recording is split into self-contained segment files written via `seg_XXXXXX.bin.tmp` $\to$ `fsync()` $\to$ atomic `rename()`. At most the currently open segment is in flight if power is pulled.
2. **Incremental Binary Hash Chain:** As each segment $i$ is sealed, its SHA-256 digest $d_i$ is chained into $H_i = \text{SHA256}_{\text{raw}}(H_{i-1} \,\|\, d_i)$ with $H_0 = 0^{256}$ (32 zero bytes).
3. **Continuously Updated Signed `manifest.json`:** After every segment seal (and upon incident seal), `manifest.json` is atomically rewritten with the updated segment list, `chain_head`, and $\text{HMAC-SHA256}(K_{\text{device}}, \text{chain\_head\_hex})$.
4. **Offline Verifier (`tools/verify_evidence.py`):** A zero-dependency Python 3 tool independently recomputes every segment SHA-256, the binary hash chain, the manifest HMAC, and the `audit.log` hash chain.

## Alternatives Considered

1. **Per-segment independent signatures without chaining:** Detects modification of an individual segment, but does **not** detect if an attacker deletes segment #3 and renumbers segments, or truncates the tail of an incident.
2. **SQLite database for manifest and audit log:** Adds an external library dependency and makes independent third-party forensic verification harder than plain JSON + line-delimited ASCII `audit.log`.

## Consequences

- **Positive:** Any byte flip, segment deletion, segment reordering, or truncation is detected both on-device (`VERIFY`) and offline (`verify_evidence.py`).
- **Positive:** Completed 10-second segments can be uploaded incrementally by `netagent` while the incident is still recording.
- **Negative:** Requires one `fsync` per segment and per manifest update (every 10 seconds, easily within eMMC IOPS budget).
