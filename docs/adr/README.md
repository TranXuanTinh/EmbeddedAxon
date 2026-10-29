# Architecture Decision Records (ADRs)

This directory records the key architectural decisions made in the AegisCam platform, their context, alternatives considered, and trade-offs.

## Index

| ADR | Title | Status |
|-----|-------|--------|
| [ADR-0001](./0001-hal-and-dependency-injection.md) | Hardware Abstraction Layer (HAL) Interfaces and Constructor Dependency Injection | Accepted |
| [ADR-0002](./0002-lock-free-pre-event-buffer.md) | Lock-Free SPSC Ring Buffer and Keyframe-Aligned Pre-Event Buffer | Accepted |
| [ADR-0003](./0003-segment-based-evidence-with-hash-chain.md) | Crash-Safe Segment Storage with Binary SHA-256 Hash Chain and HMAC Manifest | Accepted |
| [ADR-0004](./0004-ab-ota-with-boot-counter-rollback.md) | Symmetric A/B OTA Updates with Boot-Counter Automatic Rollback | Accepted |
| [ADR-0005](./0005-go-for-connectivity-agent.md) | Process Split: C++17 `aegisd` Core and Go 1.22 `netagent` Connectivity Daemon | Accepted |
| [ADR-0006](./0006-wireguard-over-ipsec-openvpn.md) | WireGuard over IPsec and OpenVPN for Field Evidence Offload | Accepted |
