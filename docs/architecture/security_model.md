# Security & Chain-of-Custody Threat Model

## 1. Security Objectives

Evidence recorded by AegisCam must be admissible in court and resilient against both opportunistic physical tampering in the field and network-level attacks during offload:

1. **Authenticity & Integrity (R3):** Detect any modification, insertion, truncation, or deletion of recorded video segments or audit log entries.
2. **Confidentiality in Transit (R5):** All cloud uploads traverse a mutually authenticated WireGuard tunnel with a strict kill-switch.
3. **Verified Software Updates (R6):** Reject corrupted or unauthenticated OTA images before switching boot slots, and automatically roll back images that fail to reach health check.
4. **Least Privilege & Sandboxing:** Compromise of the network-facing `netagent` must not grant write access to on-disk evidence or access to the device's HMAC signing key.

---

## 2. STRIDE Threat Model

| Threat Category | Attack Scenario | Mitigation in AegisCam | Verification |
|-----------------|-----------------|------------------------|--------------|
| **Spoofing** | Rogue Wi-Fi AP or DNS hijack redirects upload traffic to an attacker server. | `dnscheck` canary prefix check + WireGuard cryptokey routing (`internal/vpn`) + TLS ingest endpoint. | `dnscheck_test.go`, `vpn_test.go` |
| **Tampering (Segment)** | Attacker with shell/SD card access flips bytes in `seg_000002.bin` to obscure a face or timestamp. | Segment SHA-256 mismatch invalidates `chain_2..chain_N` and breaks `manifest.json` HMAC. | `test_tamper_detection_segment_and_audit` |
| **Tampering (Deletion / Truncation)** | Attacker deletes the final segment `seg_000005.bin` or drops an entire incident entry from `audit.log`. | `manifest.json` `chain_head` is signed by hardware-backed HMAC key; `audit.log` is hash-chained from genesis. | `tools/verify_evidence.py` |
| **Repudiation** | Operator claims recording was stopped by a remote system rather than manual button press. | Every `TRIGGER` and `STOP` source is recorded in the hash-chained `audit.log` and `manifest.json` `trigger_source`. | `audit_log` unit & system tests |
| **Information Disclosure** | Packet capture on cellular/Wi-Fi link exposes video evidence in transit. | WireGuard supervisor (`KillSwitch: true`) blocks all uploads unless the tunnel handshake is fresh (`<= 3 min`). | `vpn_test.go` |
| **Elevation of Privilege** | Malformed IPC command or network payload exploits parser bug. | Line protocol parser is fuzzed (`fuzz_command_protocol`); `netagent` runs as unprivileged `aegis-net` user with `ReadOnlyPaths=/var/lib/aegis/incidents`. | CI Gate 6 (libFuzzer), `deploy/systemd/netagent.service` |

---

## 3. Cryptographic Chain-of-Custody Construction

### 3.1 Per-Incident Segment Hash Chain

For an incident with segments $S_1, S_2, \dots, S_n$:

$$d_i = \text{SHA256}_{\text{raw}}(S_i) \in \{0,1\}^{256}$$

$$H_0 = 0^{256} \quad (\text{32 zero bytes})$$

$$H_i = \text{SHA256}_{\text{raw}}(H_{i-1} \,\|\, d_i) \quad \text{for } i = 1 \dots n$$

$$\text{tag} = \text{HMAC-SHA256}\left(K_{\text{device}},\, \text{hex\_ascii}(H_n)\right)$$

- In production, $K_{\text{device}}$ resides inside a TPM 2.0 / secure element accessed via `IKeyStoreHal` so the raw key material cannot be extracted from the filesystem.
- In `--sim` mode, `SimKeyStoreHal` supplies the deterministic test key `aegis-sim-device-key-DO-NOT-SHIP` so `tools/verify_evidence.py` can independently verify simulated stores.

### 3.2 Append-Only Audit Log Chain

Each line in `<store_root>/audit.log` has the format:
```
<seq>|<ts_ms>|<event>|<detail>|<prev_hash_hex>|<hash_hex>
```
where `prev_hash_hex` is `64` ASCII `'0'` characters for `seq = 1`, and:
```
hash_hex = hex(SHA256(prev_hash_hex || "|" || seq || "|" || ts_ms || "|" || event || "|" || detail))
```
Deleting, inserting, or modifying any line invalidates `prev_hash_hex` or `hash_hex` for that line and every subsequent line in the log.
