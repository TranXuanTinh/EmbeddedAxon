# ADR-0004: Symmetric A/B OTA Updates with Boot-Counter Automatic Rollback

- **Status:** Accepted
- **Requirement(s):** R6 (OTA with A/B slots and automatic rollback)

## Context

Field cameras receive firmware updates over cellular or station Wi-Fi while unattended in charging docks. An interrupted update or a defective firmware image that crashes on boot must never brick a device or require physical RMA disassembly.

## Decision

We implement a symmetric A/B partition scheme (`ABSlotManager` + `IBootControlHal`):
1. **Stage to Inactive Slot:** While running on active slot `A`, `stage(image_path, expected_sha256)` streams and verifies the SHA-256 digest of the incoming image before writing it to inactive slot `B`. If the digest mismatches, staging is aborted with `ErrorCode::IntegrityError` and slot `A` remains untouched.
2. **Pending Trial State (`boot_attempts_left = 3`):** Once verified, slot `B` is marked pending with a boot counter initialized to `3`.
3. **Boot Counter Decrement & Rollback:** On each boot (`on_boot()`), if the active slot is not yet marked good, the bootloader/early-init decrements `boot_attempts_left`. If the counter reaches `0` without `mark_good()` being called (e.g., `aegisd` crashes repeatedly or fails self-test), `ABSlotManager` automatically rolls back to the previous known-good slot (`A`).
4. **Health Confirmation (`OTA_MARK_GOOD`):** Once `aegisd` initializes all HALs, passes evidence store verification, and establishes watchdog heartbeats, it invokes `mark_good()`, clearing the pending state and locking in slot `B`.

## Alternatives Considered

1. **In-place single-partition update with recovery ramdisk:** Leaves a window where power loss corrupts the only rootfs, and recovery ramdisks rarely have full cellular/VPN credentials to fetch a replacement image.
2. **Delta/ostree file-level updates without block-level A/B:** Higher runtime complexity and harder to combine with `dm-verity` read-only rootfs signing.

## Consequences

- **Positive:** Atomic switchover and guaranteed recovery within 3 failed boot cycles without human intervention.
- **Negative:** Doubles rootfs partition flash footprint (mitigated by minimal Yocto `aegiscam-image.bb` footprint).
