# Operational Readiness Checklist (Production Release Gate)

Before cutting a production firmware release (`meta-aegiscam` image) for fleet deployment, the Lead Embedded Engineer and QA Lead must verify and sign off on every item below.

---

## 1. Evidence Integrity & Data Durability
- [ ] **Power-pull stress test passed:** 1,000 automated relay power-cuts during active `Recording` and `PostRecord` states produce zero corrupted completed segments and 100% pass rate under `tools/verify_evidence.py`.
- [ ] **Pre-event buffer verification:** Triggering across all sources (`holster`, `button`, `signal`, `remote`) retains the full 30-second pre-event window starting on an IDR keyframe.
- [ ] **Storage exhaustion behavior:** Filling the evidence partition triggers `StorageFull` $\to$ cleanly seals the active incident manifest $\to$ transitions to `Fault` state $\to$ emits audit log record.

## 2. Real-Time Latency & Watchdog Stability
- [ ] **Trigger-to-recording latency (R2):** p99 latency `< 100 ms` measured under simultaneous `netagent` chunk upload and CPU stress (`stress-ng`).
- [ ] **72-hour soak test (R7):** Zero unhandled exceptions, zero memory growth (flat RSS for both `aegisd` and `netagent`), and zero unexpected watchdog resets over 72 hours of continuous buffering and periodic recording.
- [ ] **Fault-injected watchdog reset:** Simulated thread hang in `capture`, `controller`, or `writer` withholds hardware watchdog kick and resets the board within the configured hardware timeout.

## 3. Networking & Offload Resilience
- [ ] **Link failover & hysteresis:** Pulling Ethernet (`eth0`) fails over to Wi-Fi (`wlan0`) within `FallThreshold` probes; intermittent Ethernet cable bounce does not preempt Wi-Fi until `RiseThreshold` consecutive probes pass.
- [ ] **Metered LTE policy:** Normal-priority incidents remain queued on `wwan0`; high-priority incidents upload within the configured token-bucket bandwidth cap.
- [ ] **VPN kill-switch:** Expiring the WireGuard handshake immediately halts chunk `PATCH` requests until tunnel re-establishment succeeds.

## 4. OTA & Security Hardening
- [ ] **A/B rollback drill:** Staging a deliberately broken image to slot B rolls back to slot A automatically after 3 unconfirmed boot attempts.
- [ ] **Production keys & rootfs:** Sim key (`aegis-sim-device-key-DO-NOT-SHIP`) is compiled out / rejected in production mode; TPM 2.0 `IKeyStoreHal` is active; `read-only-rootfs` and `systemd-analyze security` score meets target on both `aegisd.service` and `netagent.service`.
