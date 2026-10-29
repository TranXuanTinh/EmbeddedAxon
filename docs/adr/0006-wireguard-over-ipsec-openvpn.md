# ADR-0006: WireGuard over IPsec and OpenVPN for Field Evidence Offload

- **Status:** Accepted
- **Requirement(s):** R5 (Upload when docked / on Wi-Fi / LTE through VPN)

## Context

Evidence cameras roam continuously across station Ethernet, vehicle Wi-Fi, and carrier LTE/5G NATs. Traditional VPN protocols struggle in this environment:
- **OpenVPN (TCP/UDP userspace):** Expensive context switches (`tun` $\leftrightarrow$ userspace), slow TLS re-handshakes on IP roam, and large codebase (~100k+ LoC).
- **IPsec (IKEv2 / strongSwan):** Complex state negotiation, fragile NAT-T traversal on restrictive cellular carriers, and large configuration surface.

## Decision

We standardize on **WireGuard** (`CONFIG_WIREGUARD=m` in kernel + `netagent/internal/vpn` supervisor):
1. **Built-in Roaming:** WireGuard seamlessly updates peer endpoint IP:port upon receiving an authenticated packet when `netagent` switches between `eth0`, `wlan0`, and `wwan0`—no tunnel teardown or IKE re-negotiation required.
2. **In-Kernel Cryptography (ChaCha20-Poly1305):** High throughput and low battery/CPU draw on ARM Cortex-A53/A55 cores without dedicated AES-NI extensions.
3. **Handshake-Age Supervision & Kill-Switch:** `vpn.Supervisor` polls `LastHandshake` and enforces `KillSwitch: true`: if the latest handshake is older than `MaxHandshakeAge` (3 minutes), `AllowTraffic()` returns `false` and `uploader` pauses until the tunnel is re-established with exponential backoff.

## Alternatives Considered

1. **Mutual TLS (mTLS) directly to public cloud ingest without VPN:** Exposes the ingest HTTPS load balancer directly to the public Internet and requires managing X.509 revocation lists on every camera. Defense-in-depth (WireGuard private overlay + HTTPS) reduces exposed attack surface.
2. **IPsec (strongSwan) or OpenVPN:** Rejected due to roaming latency and operational complexity.

## Consequences

- **Positive:** Sub-second uplink roaming across Ethernet/Wi-Fi/LTE, minimal kernel footprint, and auditable `wg-quick` configuration rendering.
- **Negative:** Requires UDP transport reachability to the agency WireGuard concentrator.
